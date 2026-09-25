#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "d2_109b.h"

static int call_target_matches(const BYTE *client, DWORD site_offset,
                               DWORD target_offset)
{
    const BYTE *site = client + site_offset;
    LONG relative;

    if (site[0] != 0xe8)
        return 0;
    CopyMemory(&relative, site + 1, sizeof(relative));
    return site + 5 + relative == client + target_offset;
}

int d2_109b_client_matches(void)
{
    static const BYTE loot_label_expected[] = {
        0xf6, 0x85, 0xec, 0x00, 0x00, 0x00, 0x80,
        0x0f, 0x84, 0x91, 0x03, 0x00, 0x00
    };
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    const BYTE *render;
    DWORD render_target;

    if (!client)
        return 0;

    /* Validate the image before touching the highest raw global used by jmod. */
    dos = (IMAGE_DOS_HEADER *)client;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0 || (DWORD)dos->e_lfanew > 0x1000u)
        return 0;
    nt = (IMAGE_NT_HEADERS *)(client + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage <
            D2CLIENT_PLAYER_PTR_OFFSET + sizeof(DWORD))
        return 0;

    if (memcmp(client + D2CLIENT_HOOK_LOOT_LABEL_OFFSET,
               loot_label_expected, sizeof(loot_label_expected)) != 0)
        return 0;

    if (!call_target_matches(client, D2CLIENT_HOOK_LOOT_HOVER_OFFSET,
                             D2CLIENT_FN_GET_SELECTED_UNIT_OFFSET) ||
        !call_target_matches(client, D2CLIENT_HOOK_LOOT_CURSOR_OFFSET,
                             D2CLIENT_FN_SET_CURSOR_TARGET_OFFSET) ||
        !call_target_matches(client, D2CLIENT_HOOK_LABEL_TOOLTIP_OFFSET,
                             D2CLIENT_FN_DRAW_HOVER_OFFSET) ||
        !call_target_matches(client, D2CLIENT_HOOK_LABEL_DRAW_CALL_OFFSET,
                             D2CLIENT_FN_DRAW_ITEM_LABELS_OFFSET))
        return 0;

    render = client + D2CLIENT_HOOK_LABEL_RENDER_FLAG_OFFSET;
    if (render[0] != 0xa1)
        return 0;
    CopyMemory(&render_target, render + 1, sizeof(render_target));
    if (render_target !=
        (DWORD)(ULONG_PTR)(client + D2CLIENT_ITEM_LABEL_RENDER_FLAG_OFFSET))
        return 0;

    return 1;
}
