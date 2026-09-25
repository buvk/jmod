#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "loot_filter.h"
#include "item_names.h"

static const D2ModConfig *filter_options;
static const BYTE *(__stdcall *get_item_text)(DWORD);
static int (__stdcall *get_unit_stat)(const void *, DWORD);
static int (__stdcall *get_item_quality)(const void *);
static BYTE item_masks[sizeof(item_names) / sizeof(item_names[0])];

static int find_item(DWORD code)
{
    int lo = 0, hi = (int)(sizeof(item_names) / sizeof(item_names[0])) - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (item_names[mid].code == code) return mid;
        if (item_names[mid].code < code) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static int quality_bit(int quality)
{
    switch (quality) {
    case 1: case 2: case 3: return 0x01;
    case 4: return 0x02;
    case 6: return 0x04;
    case 5: return 0x08;
    case 7: return 0x10;
    case 8: return 0x20;
    default: return 0; /* Unknown qualities remain visible. */
    }
}

static int show_ground_item(const BYTE *item)
{
    const BYTE *record;
    DWORD code;
    int index, bit;

    /* If an item cannot be classified, leave its label visible. */
    if (!item || *(const DWORD *)item != 4 ||
        !get_item_text || !get_unit_stat)
        return 1;
    record = get_item_text(*(const DWORD *)(item + 4));
    if (!record) return 1;
    code = *(const DWORD *)(record + 0x144);
    if ((code & 0x00ffffff) == 0x00646c67) { /* "gld" */
        if (!filter_options || !filter_options->min_gold) return 1;
        return get_unit_stat(item, 14) >= filter_options->min_gold;
    }
    index = find_item(code);
    if (index < 0 || item_names[index].quest || !get_item_quality) return 1;
    bit = quality_bit(get_item_quality(item));
    return !bit || (item_masks[index] & bit) != 0;
}

int loot_filter_show_item(const void *item)
{
    return !filter_options || !filter_options->loot_filter_enabled ||
           show_ground_item((const BYTE *)item);
}

/* The draw-site filter only removes labels. The client obtains its hovered
   world unit separately; clear a hidden item there too so its highlight and
   tooltip do not survive after the label has been filtered out. */
static void *__cdecl filter_hovered_unit(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void *(__cdecl *selected_unit_fn)(void);
    typedef void (__fastcall *select_unit_fn)(void *);
    void *selected = ((selected_unit_fn)(client + 0x14cf0))();

    if (selected && *(const DWORD *)selected == 4 &&
        !show_ground_item((const BYTE *)selected)) {
        ((select_unit_fn)(client + 0x14db0))(NULL);
        return NULL;
    }
    return selected;
}

/* The cursor update selects a world unit before the tooltip pass. Filter
   that candidate before the game records it as the highlighted target. */
static void __fastcall filter_cursor_target(void *target)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void (__fastcall *select_unit_fn)(void *);
    if (target && *(const DWORD *)target == 4 &&
        !show_ground_item((const BYTE *)target))
        target = NULL;
    ((select_unit_fn)(client + 0x14db0))(target);
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

int loot_filter_init(const D2ModConfig *options, HMODULE module)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    HMODULE common = GetModuleHandleA("D2Common.dll");
    BYTE *site, *hover_site, *cursor_site;
    DWORD old_protection, old_hover_protection, old_cursor_protection, unused;
    static const BYTE expected[] = {
        0xf6, 0x85, 0xec, 0x00, 0x00, 0x00, 0x80,
        0x0f, 0x84, 0x91, 0x03, 0x00, 0x00
    };
    union { FARPROC raw; const BYTE *(__stdcall *typed)(DWORD); } item_text;
    union { FARPROC raw; int (__stdcall *typed)(const void *, DWORD); } unit_stat;
    union { FARPROC raw; int (__stdcall *typed)(const void *); } quality;
    char path[MAX_PATH], option[32], fallback[32], previous[32], *slash, *end;
    DWORD length;
    unsigned i;
    unsigned long mask;

    if (!options) return 0;
    if (!options->loot_filter_enabled) return 1;
    if (!module || !client || !common) return 0;
    site = client + 0x63bf9;
    hover_site = client + 0x8729e;
    cursor_site = client + 0x155f4;
    if (memcmp(site, expected, sizeof(expected)) != 0) return 0;
    if (hover_site[0] != 0xe8 ||
        *(DWORD *)(hover_site + 1) !=
            (DWORD)(client + 0x14cf0 - (hover_site + 5))) return 0;
    if (cursor_site[0] != 0xe8 ||
        *(DWORD *)(cursor_site + 1) !=
            (DWORD)(client + 0x14db0 - (cursor_site + 5))) return 0;
    item_text.raw = GetProcAddress(common, MAKEINTRESOURCEA(10600));
    unit_stat.raw = GetProcAddress(common, MAKEINTRESOURCEA(10519));
    quality.raw = GetProcAddress(common, MAKEINTRESOURCEA(10695));
    if (!item_text.raw || !unit_stat.raw || !quality.raw) return 0;
    length = GetModuleFileNameA(module, path, sizeof(path));
    if (!length || length >= sizeof(path)) return 0;
    slash = strrchr(path, '\\');
    if (!slash || (size_t)(path + sizeof(path) - (slash + 1)) <
                  sizeof("loot_filter.ini")) return 0;
    memcpy(slash + 1, "loot_filter.ini", sizeof("loot_filter.ini"));
    for (i = 0; i < sizeof(item_masks) / sizeof(item_masks[0]); ++i) {
        /* Keep masks from earlier [Items], [Weapons], and [Armor] files.
           A more specific section takes precedence over its old section. */
        GetPrivateProfileStringA("Items", item_names[i].name, "0x3F",
                                 fallback, sizeof(fallback), path);
        if (item_names[i].legacy_section)
            GetPrivateProfileStringA(item_names[i].legacy_section,
                                     item_names[i].name, fallback,
                                     previous, sizeof(previous), path);
        else
            lstrcpynA(previous, fallback, sizeof(previous));
        GetPrivateProfileStringA(item_names[i].section, item_names[i].name, previous,
                                 option, sizeof(option), path);
        mask = strtoul(option, &end, 0);
        item_masks[i] = end != option && !*end && mask <= 0x3f ?
            (BYTE)mask : 0x3f;
    }
    get_item_text = item_text.typed;
    get_unit_stat = unit_stat.typed;
    get_item_quality = quality.typed;
    if (!VirtualProtect(site, 7, PAGE_EXECUTE_READWRITE, &old_protection))
        return 0;
    if (!VirtualProtect(hover_site, 5, PAGE_EXECUTE_READWRITE,
                        &old_hover_protection)) {
        VirtualProtect(site, 7, old_protection, &unused);
        return 0;
    }
    if (!VirtualProtect(cursor_site, 5, PAGE_EXECUTE_READWRITE,
                        &old_cursor_protection)) {
        VirtualProtect(hover_site, 5, old_hover_protection, &unused);
        VirtualProtect(site, 7, old_protection, &unused);
        return 0;
    }

    /* Publish the filter only after every hook site has been validated and
       made writable. A failed init must leave loot_filter_show_item inert. */
    filter_options = options;

    site[0] = 0xe8;
    *(DWORD *)(site + 1) = (DWORD)((BYTE *)filter_label_candidate - (site + 5));
    site[5] = site[6] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), site, 7);
    hover_site[0] = 0xe8;
    *(DWORD *)(hover_site + 1) =
        (DWORD)((BYTE *)filter_hovered_unit - (hover_site + 5));
    FlushInstructionCache(GetCurrentProcess(), hover_site, 5);
    cursor_site[0] = 0xe8;
    *(DWORD *)(cursor_site + 1) =
        (DWORD)((BYTE *)filter_cursor_target - (cursor_site + 5));
    FlushInstructionCache(GetCurrentProcess(), cursor_site, 5);
    VirtualProtect(cursor_site, 5, old_cursor_protection, &unused);
    VirtualProtect(hover_site, 5, old_hover_protection, &unused);
    VirtualProtect(site, 7, old_protection, &unused);
    return 1;
}
