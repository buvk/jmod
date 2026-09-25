#ifndef GAME_UI_H
#define GAME_UI_H

#include <windows.h>

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

#endif
