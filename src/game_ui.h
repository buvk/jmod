#ifndef GAME_UI_H
#define GAME_UI_H

#include <windows.h>

#define GAME_TOWN_UNKNOWN (-1)
#define GAME_TOWN_NO 0
#define GAME_TOWN_YES 1

static inline const BYTE *game_player(void)
{
    const BYTE *client = (const BYTE *)GetModuleHandleA("D2Client.dll");
    return client ? *(const BYTE *const *)(client + 0x127578) : NULL;
}

static inline int game_active(void)
{
    return game_player() != NULL;
}

static inline int game_menu_open(void)
{
    const BYTE *client = (const BYTE *)GetModuleHandleA("D2Client.dll");
    /* Escape menu state in the supported D2Client 1.09b build. */
    return client && *(const DWORD *)(client + 0x125a58) != 0;
}

/* Returns GAME_TOWN_YES, GAME_TOWN_NO, or GAME_TOWN_UNKNOWN.
   D2Common 1.09b ordinals: GetRoom(Unit*) = 10342, GetLevelID(Room*) = 10057. */
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
    if (!player || *(const DWORD *)player != 0 ||
        !*(const void *const *)(player + 0x38) || !common)
        return GAME_TOWN_UNKNOWN;

    if (!exports_checked) {
        union { FARPROC raw; unit_room_fn typed; } room_export;
        union { FARPROC raw; room_level_fn typed; } level_export;
        room_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10342));
        level_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10057));
        unit_room = room_export.typed;
        room_level = level_export.typed;
        exports_checked = 1;
    }
    if (!unit_room || !room_level || !(room = unit_room(player)))
        return GAME_TOWN_UNKNOWN;

    level = room_level(room);
    if (level == 1 || level == 40 || level == 75 ||
        level == 103 || level == 109)
        return GAME_TOWN_YES;
    return level ? GAME_TOWN_NO : GAME_TOWN_UNKNOWN;
}

#endif
