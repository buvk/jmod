#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "loot_filter.h"

static const D2ModConfig *filter_options;
static const BYTE *(__stdcall *get_item_text)(DWORD);
static int (__stdcall *get_unit_stat)(const void *, DWORD);

static int show_ground_item(const BYTE *item)
{
    const BYTE *record;
    DWORD code;

    /* If an item cannot be classified, leave its label visible. */
    if (!item || *(const DWORD *)item != 4 ||
        !get_item_text || !get_unit_stat)
        return 1;
    record = get_item_text(*(const DWORD *)(item + 4));
    if (!record) return 1;
    code = *(const DWORD *)(record + 0x144);
    if ((code & 0x00ffffff) == 0x00646c67) { /* "gld" */
        if (!filter_options->min_gold) return 1;
        return get_unit_stat(item, 14) >= filter_options->min_gold;
    }
    if ((code & 0x0000ffff) == 0x7068 && (code >> 16 & 0xff) >= '1' &&
        (code >> 16 & 0xff) <= '5')
        return filter_options->min_health_potion == 0 ||
            (int)((code >> 16 & 0xff) - '0') >= filter_options->min_health_potion;
    if ((code & 0x0000ffff) == 0x706d && (code >> 16 & 0xff) >= '1' &&
        (code >> 16 & 0xff) <= '5')
        return filter_options->min_mana_potion == 0 ||
            (int)((code >> 16 & 0xff) - '0') >= filter_options->min_mana_potion;
    if ((code & 0x00ffffff) == 0x00737672) /* "rvs" */
        return filter_options->min_rejuvenation_potion == 0;
    if ((code & 0x00ffffff) == 0x006c7672) /* "rvl" */
        return filter_options->min_rejuvenation_potion != 2;
    return 1;
}

int loot_filter_show_item(const void *item)
{
    return !filter_options || show_ground_item((const BYTE *)item);
}

/* The original test at D2Client+0x63bf9 is followed by a JZ. Preserve all
   registers and leave ZF set when the item is hidden or lacks the original
   visible flag. popad and ret preserve the condition flags. */
static void __attribute__((naked)) filter_label_candidate(void)
{
    __asm__ __volatile__(
        "pushal\n\t"
        "pushl %%ebp\n\t"
        "call %P0\n\t"
        "addl $4, %%esp\n\t"
        "testb $0x80, 0xec(%%ebp)\n\t"
        "jz 1f\n\t"
        "testl %%eax, %%eax\n\t"
        "jmp 2f\n"
        "1: xorl %%eax, %%eax\n"
        "2: popal\n\t"
        "ret\n\t"
        : : "i" (show_ground_item));
}

int loot_filter_init(const D2ModConfig *options)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    HMODULE common = GetModuleHandleA("D2Common.dll");
    BYTE *site;
    DWORD old_protection, unused;
    static const BYTE expected[] = {
        0xf6, 0x85, 0xec, 0x00, 0x00, 0x00, 0x80,
        0x0f, 0x84, 0x91, 0x03, 0x00, 0x00
    };
    union { FARPROC raw; const BYTE *(__stdcall *typed)(DWORD); } item_text;
    union { FARPROC raw; int (__stdcall *typed)(const void *, DWORD); } unit_stat;

    if (!options->loot_filter_enabled) return 1;
    if (!client || !common) return 0;
    site = client + 0x63bf9;
    if (memcmp(site, expected, sizeof(expected)) != 0) return 0;
    item_text.raw = GetProcAddress(common, MAKEINTRESOURCEA(10600));
    unit_stat.raw = GetProcAddress(common, MAKEINTRESOURCEA(10519));
    if (!item_text.raw || !unit_stat.raw) return 0;
    filter_options = options;
    get_item_text = item_text.typed;
    get_unit_stat = unit_stat.typed;
    if (!VirtualProtect(site, 7, PAGE_EXECUTE_READWRITE, &old_protection))
        return 0;
    site[0] = 0xe8;
    *(DWORD *)(site + 1) = (DWORD)((BYTE *)filter_label_candidate - (site + 5));
    site[5] = site[6] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), site, 7);
    VirtualProtect(site, 7, old_protection, &unused);
    return 1;
}
