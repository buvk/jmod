#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "auto_gold.h"
#include "game_ui.h"
#include "d2_109b.h"

#define PICKUP_GOLD_MESSAGE (WM_APP + 0x313)
#define RECENT_GOLD_COUNT 32

typedef int (__stdcall *unit_distance_fn)(const void *, const void *);
typedef int (__stdcall *unit_collision_fn)(const void *, const void *, int);
typedef const BYTE *(__stdcall *item_text_fn)(DWORD);
typedef void (__stdcall *send_packet_fn)(DWORD, const BYTE *, DWORD);

typedef struct RecentGold {
    DWORD id;
    DWORD at;
    int used;
} RecentGold;

static const D2ModConfig *config;
static DWORD last_gold_at;
static RecentGold recent_gold[RECENT_GOLD_COUNT];
static unsigned recent_gold_next;
static volatile LONG gold_message_pending;
static CRITICAL_SECTION gold_lock;
static unit_distance_fn unit_distance;
static unit_collision_fn unit_collision;
static item_text_fn item_text;
static send_packet_fn send_packet;

static int resolve_gold_exports(void)
{
    HMODULE common, net;
    union { FARPROC raw; unit_distance_fn typed; } distance_export;
    union { FARPROC raw; unit_collision_fn typed; } collision_export;
    union { FARPROC raw; item_text_fn typed; } text_export;
    union { FARPROC raw; send_packet_fn typed; } packet_export;

    if (unit_distance && unit_collision && item_text && send_packet)
        return 1;

    common = GetModuleHandleA("D2Common.dll");
    net = GetModuleHandleA("D2Net.dll");
    if (!common || !net)
        return 0;

    distance_export.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_UNIT_DISTANCE_ORDINAL));
    collision_export.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_TEST_INTERACTION_COLLISION_ORDINAL));
    text_export.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_GET_ITEM_TEXT_ORDINAL));
    packet_export.raw = GetProcAddress(
        net, MAKEINTRESOURCEA(D2NET_SEND_PACKET_ORDINAL));
    if (!distance_export.raw || !collision_export.raw ||
        !text_export.raw || !packet_export.raw)
        return 0;

    unit_distance = distance_export.typed;
    unit_collision = collision_export.typed;
    item_text = text_export.typed;
    send_packet = packet_export.typed;
    return 1;
}

static int gold_recently_requested(DWORD id, DWORD now)
{
    unsigned i;
    for (i = 0; i < RECENT_GOLD_COUNT; ++i) {
        if (recent_gold[i].used && recent_gold[i].id == id)
            return (DWORD)(now - recent_gold[i].at) <
                   AUTO_GOLD_RETRY_INTERVAL_MS;
    }
    return 0;
}

static void remember_gold_request(DWORD id, DWORD now)
{
    unsigned i;
    for (i = 0; i < RECENT_GOLD_COUNT; ++i) {
        if (recent_gold[i].used && recent_gold[i].id == id) {
            recent_gold[i].at = now;
            return;
        }
    }

    recent_gold[recent_gold_next].id = id;
    recent_gold[recent_gold_next].at = now;
    recent_gold[recent_gold_next].used = 1;
    recent_gold_next = (recent_gold_next + 1) % RECENT_GOLD_COUNT;
}

static void pick_up_nearby_gold_locked(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    const BYTE *player;
    const BYTE *item;
    const BYTE *record;
    BYTE packet[D2NET_PACKET_PICKUP_ITEM_SIZE];
    DWORD id, now;
    unsigned bucket, visited;
    const BYTE *const *items;

    if (!config || !config->auto_gold_pickup || game_menu_open() ||
        !client || !resolve_gold_exports())
        return;
    player = *(const BYTE *const *)(client + D2CLIENT_PLAYER_PTR_OFFSET);
    if (!player ||
        *(const DWORD *)(player + D2UNIT_TYPE_OFFSET) != D2UNIT_PLAYER ||
        !*(const void *const *)(player + D2UNIT_PATH_OFFSET))
        return;

    /* Preserve the old fail-closed behavior: when town detection is
       unavailable, do not auto-pick gold if town pickup is disabled. */
    if (!config->gold_pickup_in_town && game_town_state() != GAME_TOWN_NO)
        return;

    /* Use the same 1.09b distance and interaction-collision helpers as D2Game.
       resolve_gold_exports() caches those functions and the item-text/network
       exports once. */

    now = GetTickCount();
    if ((DWORD)(now - last_gold_at) < AUTO_GOLD_REQUEST_INTERVAL_MS)
        return;

    items = (const BYTE *const *)(client + D2CLIENT_UNIT_HASH_TABLES_OFFSET +
        D2UNIT_ITEM * D2CLIENT_UNIT_HASH_BUCKET_COUNT * sizeof(void *));
    for (bucket = 0, visited = 0;
         bucket < D2CLIENT_UNIT_HASH_BUCKET_COUNT; ++bucket) {
        for (item = items[bucket]; item && visited++ < 4096;
             item = *(const BYTE *const *)(item + D2UNIT_HASH_NEXT_OFFSET)) {
            if (*(const DWORD *)(item + D2UNIT_TYPE_OFFSET) != D2UNIT_ITEM ||
                *(const DWORD *)(item + D2UNIT_MODE_OFFSET) != D2ITEM_MODE_GROUND ||
                !*(const void *const *)(item + D2UNIT_PATH_OFFSET))
                continue;
            id = *(const DWORD *)(item + D2UNIT_ID_OFFSET);
            if (gold_recently_requested(id, now))
                continue;
            record = item_text(*(const DWORD *)(item + D2UNIT_CLASS_ID_OFFSET));
            if (!record ||
                *(const DWORD *)(record + D2ITEMTXT_CODE_OFFSET) != D2ITEM_CODE_GOLD)
                continue;
            if (unit_distance(player, item) > config->gold_pickup_range ||
                unit_collision(player, item, D2ITEM_INTERACT_COLLISION_MASK))
                continue;
            packet[0] = D2NET_PACKET_PICKUP_ITEM;
            *(DWORD *)(packet + D2NET_PACKET_PICKUP_UNIT_TYPE_OFFSET) =
                D2UNIT_ITEM;
            *(DWORD *)(packet + D2NET_PACKET_PICKUP_UNIT_ID_OFFSET) = id;
            *(DWORD *)(packet + D2NET_PACKET_PICKUP_RESERVED_OFFSET) = 0;
            send_packet(0, packet, sizeof(packet));
            last_gold_at = now;
            remember_gold_request(id, now);
            return;
        }
        if (visited >= 4096) break;
    }
}

static void pick_up_nearby_gold(void)
{
    EnterCriticalSection(&gold_lock);
    pick_up_nearby_gold_locked();
    LeaveCriticalSection(&gold_lock);
}

void auto_gold_init(const D2ModConfig *options)
{
    InitializeCriticalSection(&gold_lock);
    config = options;
}

void auto_gold_reset(void)
{
    InterlockedExchange(&gold_message_pending, 0);
    EnterCriticalSection(&gold_lock);
    last_gold_at = 0;
    ZeroMemory(recent_gold, sizeof(recent_gold));
    recent_gold_next = 0;
    LeaveCriticalSection(&gold_lock);
}

void auto_gold_poll(HWND game_window, int hooked)
{
    if (config && config->auto_gold_pickup && game_active() &&
        !game_menu_open() && hooked && game_window &&
        GetForegroundWindow() == game_window &&
        InterlockedCompareExchange(&gold_message_pending, 1, 0) == 0 &&
        !PostMessageA(game_window, PICKUP_GOLD_MESSAGE, 0, 0))
        InterlockedExchange(&gold_message_pending, 0);
}

int auto_gold_on_message(MSG *msg, HWND game_window)
{
    if (msg->message != PICKUP_GOLD_MESSAGE)
        return 0;
    /* Always clear pending, even if the selected game window changed. */
    InterlockedExchange(&gold_message_pending, 0);
    if (msg->hwnd == game_window && GetForegroundWindow() == msg->hwnd)
        pick_up_nearby_gold();
    return 1;
}
