#ifndef GAME_UI_H
#define GAME_UI_H

#include <windows.h>

static inline int game_menu_open(void)
{
    const BYTE *client = (const BYTE *)GetModuleHandleA("D2Client.dll");
    /* Escape menu state in the supported D2Client 1.09b build. */
    return client && *(const DWORD *)(client + 0x125a58) != 0;
}

#endif
