#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "auto_gold.h"
#include "game_ui.h"

#define PICKUP_GOLD_MESSAGE (WM_APP + 0x313)
#define GOLD_INTERACT_COLLISION_MASK 0x804
static const D2ModConfig *config;
static DWORD last_gold_at;
static struct { DWORD id, at; } recent_gold[32];
static volatile LONG gold_message_pending;
static CRITICAL_SECTION gold_lock;

static void pick_up_nearby_gold_locked(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    HMODULE common = GetModuleHandleA("D2Common.dll");
    HMODULE net = GetModuleHandleA("D2Net.dll");
    typedef int (__stdcall *unit_distance_fn)(const void *, const void *);
    typedef int (__stdcall *unit_collision_fn)(const void *, const void *, int);
    typedef const BYTE *(__stdcall *item_text_fn)(DWORD);
    typedef void (__stdcall *send_packet_fn)(DWORD, const BYTE *, DWORD);
    unit_distance_fn unit_distance;
    unit_collision_fn unit_collision;
    item_text_fn item_text;
    send_packet_fn send_packet;
    union { FARPROC raw; unit_distance_fn typed; } unit_distance_export;
    union { FARPROC raw; unit_collision_fn typed; } unit_collision_export;
    union { FARPROC raw; item_text_fn typed; } item_text_export;
    union { FARPROC raw; send_packet_fn typed; } send_packet_export;
    const BYTE *player;
    const BYTE *item;
    const BYTE *record;
    BYTE packet[13];
    DWORD id, now;
    unsigned bucket, visited, slot;
    const BYTE *const *items;

    if (!config || !config->auto_gold_pickup || game_menu_open() ||
        !client || !common || !net)
        return;
    player = *(const BYTE *const *)(client + 0x127578);
    if (!player || *(const DWORD *)player != 0 ||
        !*(const void *const *)(player + 0x38))
        return;

    /* Preserve the old fail-closed behavior: when town detection is
       unavailable, do not auto-pick gold if town pickup is disabled. */
    if (!config->gold_pickup_in_town && game_town_state() != GAME_TOWN_NO)
        return;

    /* D2Game 1.09b uses D2Common ordinal 10399 for its item interaction
       range check. Use the same calculation instead of approximating it
       with Euclidean tile distance. */
    unit_distance_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10399));
    /* D2Game 1.09b uses ordinal 10363 with mask 0x804 immediately after
       its <= 4 distance check. A nonzero result makes it walk toward the
       item instead of picking it up immediately. */
    unit_collision_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10363));
    item_text_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10600));
    send_packet_export.raw = GetProcAddress(net, MAKEINTRESOURCEA(10005));
    unit_distance = unit_distance_export.typed;
    unit_collision = unit_collision_export.typed;
    item_text = item_text_export.typed;
    send_packet = send_packet_export.typed;
    if (!unit_distance || !unit_collision || !item_text || !send_packet)
        return;

    now = GetTickCount();
    if ((DWORD)(now - last_gold_at) < AUTO_GOLD_REQUEST_INTERVAL_MS)
        return;

    items = (const BYTE *const *)(client + 0x125d78 + 4 * 128 * sizeof(void *));
    for (bucket = 0, visited = 0; bucket < 128; ++bucket) {
        for (item = items[bucket]; item && visited++ < 4096;
             item = *(const BYTE *const *)(item + 0x108)) {
            if (*(const DWORD *)item != 4 || *(const DWORD *)(item + 0x0c) != 3 ||
                !*(const void *const *)(item + 0x38))
                continue;
            id = *(const DWORD *)(item + 0x08);
            slot = id % 32;
            if (recent_gold[slot].id == id &&
                (DWORD)(now - recent_gold[slot].at) < AUTO_GOLD_RETRY_INTERVAL_MS)
                continue;
            record = item_text(*(const DWORD *)(item + 0x04));
            if (!record || *(const DWORD *)(record + 0x144) != 0x20646c67)
                continue;
            if (unit_distance(player, item) > config->gold_pickup_range ||
                unit_collision(player, item, GOLD_INTERACT_COLLISION_MASK))
                continue;
            packet[0] = 0x16;
            *(DWORD *)(packet + 1) = 4;
            *(DWORD *)(packet + 5) = id;
            *(DWORD *)(packet + 9) = 0;
            send_packet(0, packet, sizeof(packet));
            last_gold_at = now;
            recent_gold[slot].id = id;
            recent_gold[slot].at = now;
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
