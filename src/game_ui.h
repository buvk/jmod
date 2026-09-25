#ifndef GAME_UI_H
#define GAME_UI_H

#include <windows.h>
#include "d2_109b.h"

#define GAME_TOWN_UNKNOWN (-1)
#define GAME_TOWN_NO 0
#define GAME_TOWN_YES 1

static inline const BYTE *game_player(void)
{
    const BYTE *client = (const BYTE *)GetModuleHandleA("D2Client.dll");
    return client ?
        *(const BYTE *const *)(client + D2CLIENT_PLAYER_PTR_OFFSET) : NULL;
}

static inline int game_active(void)
{
    return game_player() != NULL;
}

static inline int game_menu_open(void)
{
    const BYTE *client = (const BYTE *)GetModuleHandleA("D2Client.dll");
    /* Escape menu state in the supported D2Client 1.09b build. */
    return client &&
        *(const DWORD *)(client + D2CLIENT_ESCAPE_MENU_STATE_OFFSET) != 0;
}

/* Returns GAME_TOWN_YES, GAME_TOWN_NO, or GAME_TOWN_UNKNOWN.
   The 1.09b D2Common room/level ordinals are centralized in d2_109b.h. */
static inline int game_town_state(void)
{
    typedef const void *(__stdcall *unit_room_fn)(const void *);
    typedef DWORD (__stdcall *room_level_fn)(const void *);
    static unit_room_fn unit_room;
    static room_level_fn room_level;
    static int exports_checked;
    HMODULE common;
    const BYTE *player;
    const void *room;
    DWORD level;

    player = game_player();
    common = GetModuleHandleA("D2Common.dll");
    if (!player ||
        *(const DWORD *)(player + D2UNIT_TYPE_OFFSET) != D2UNIT_PLAYER ||
        !*(const void *const *)(player + D2UNIT_PATH_OFFSET) || !common)
        return GAME_TOWN_UNKNOWN;

    if (!exports_checked) {
        union { FARPROC raw; unit_room_fn typed; } room_export;
        union { FARPROC raw; room_level_fn typed; } level_export;
        room_export.raw = GetProcAddress(
            common, MAKEINTRESOURCEA(D2COMMON_GET_ROOM_ORDINAL));
        level_export.raw = GetProcAddress(
            common, MAKEINTRESOURCEA(D2COMMON_GET_LEVEL_ID_ORDINAL));
        unit_room = room_export.typed;
        room_level = level_export.typed;
        exports_checked = 1;
    }
    if (!unit_room || !room_level || !(room = unit_room(player)))
        return GAME_TOWN_UNKNOWN;

    level = room_level(room);
    if (level == D2LEVEL_ROGUE_ENCAMPMENT ||
        level == D2LEVEL_LUT_GHOLEIN ||
        level == D2LEVEL_KURAST_DOCKS ||
        level == D2LEVEL_PANDEMONIUM_FORTRESS ||
        level == D2LEVEL_HARROGATH)
        return GAME_TOWN_YES;
    return level ? GAME_TOWN_NO : GAME_TOWN_UNKNOWN;
}

#endif
