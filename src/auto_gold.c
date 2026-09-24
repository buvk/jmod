#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "auto_gold.h"
#include "game_ui.h"

#define PICKUP_GOLD_MESSAGE (WM_APP + 0x313)
static const D2ModConfig *config;
static DWORD last_gold_at;
static struct { DWORD id, at; } recent_gold[32];
static volatile LONG gold_message_pending;

static void pick_up_nearby_gold(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    HMODULE common = GetModuleHandleA("D2Common.dll");
    HMODULE net = GetModuleHandleA("D2Net.dll");
    typedef int (__stdcall *unit_coord_fn)(const void *);
    typedef const void *(__stdcall *unit_room_fn)(const void *);
    typedef DWORD (__stdcall *room_level_fn)(const void *);
    typedef const BYTE *(__stdcall *item_text_fn)(DWORD);
    typedef void (__stdcall *send_packet_fn)(DWORD, const BYTE *, DWORD);
    unit_coord_fn unit_x, unit_y;
    unit_room_fn unit_room;
    room_level_fn room_level;
    item_text_fn item_text;
    send_packet_fn send_packet;
    union { FARPROC raw; item_text_fn typed; } item_text_export;
    union { FARPROC raw; send_packet_fn typed; } send_packet_export;
    union { FARPROC raw; unit_room_fn typed; } unit_room_export;
    union { FARPROC raw; room_level_fn typed; } room_level_export;
    const BYTE *player;
    const BYTE *item;
    const BYTE *record;
    BYTE packet[13];
    DWORD id, now, level;
    int x, y, ix, iy;
    unsigned bucket, visited, slot;
    const BYTE *const *items;

    if (!config->auto_gold_pickup || game_menu_open() ||
        !client || !common || !net)
        return;
    player = *(const BYTE *const *)(client + 0x127578);
    if (!player || *(const DWORD *)player != 0 ||
        !*(const void *const *)(player + 0x38))
        return;

    if (!config->gold_pickup_in_town) {
        const void *room;
        /* D2Common 1.09b: GetRoom(Unit*) and GetLevelID(Room*). */
        unit_room_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10342));
        room_level_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10057));
        unit_room = unit_room_export.typed;
        room_level = room_level_export.typed;
        if (!unit_room || !room_level || !(room = unit_room(player)))
            return;
        level = room_level(room);
        if (level == 0 || level == 1 || level == 40 || level == 75 ||
            level == 103 || level == 109)
            return;
    }

    unit_x = (unit_coord_fn)GetProcAddress(common, MAKEINTRESOURCEA(10327));
    unit_y = (unit_coord_fn)GetProcAddress(common, MAKEINTRESOURCEA(10330));
    item_text_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10600));
    send_packet_export.raw = GetProcAddress(net, MAKEINTRESOURCEA(10005));
    item_text = item_text_export.typed;
    send_packet = send_packet_export.typed;
    if (!unit_x || !unit_y || !item_text || !send_packet)
        return;

    x = unit_x(player) >> 16;
    y = unit_y(player) >> 16;
    now = GetTickCount();
    if ((DWORD)(now - last_gold_at) < config->gold_request_interval_ms)
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
                (DWORD)(now - recent_gold[slot].at) < config->gold_retry_interval_ms)
                continue;
            record = item_text(*(const DWORD *)(item + 0x04));
            if (!record || *(const DWORD *)(record + 0x144) != 0x20646c67)
                continue;
            ix = (unit_x(item) >> 16) - x;
            iy = (unit_y(item) >> 16) - y;
            if (ix < -config->gold_pickup_range || ix > config->gold_pickup_range ||
                iy < -config->gold_pickup_range || iy > config->gold_pickup_range ||
                ix * ix + iy * iy > config->gold_pickup_range * config->gold_pickup_range)
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

void auto_gold_init(const D2ModConfig *options)
{
    config = options;
}

void auto_gold_reset(void)
{
    InterlockedExchange(&gold_message_pending, 0);
}

void auto_gold_poll(HWND game_window, int hooked)
{
    if (config->auto_gold_pickup && !game_menu_open() &&
        hooked && game_window &&
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
