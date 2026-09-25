#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "rune_color.h"
#include "d2_109b.h"

/* English 1.09b string IDs from expansionstring.tbl, r33 through r01. */
static const char *const expansion_names[33] = {
    "Zod Rune", "Cham Rune", "Jo Rune", "Ber Rune", "Sur Rune",
    "Lo Rune", "Ohm Rune", "Vex Rune", "Gul Rune", "Ist Rune",
    "Mal Rune", "Um Rune", "Pul Rune", "Lem Rune", "Fal Rune",
    "Ko Rune", "Lum Rune", "Po Rune", "Hel Rune", "Dol Rune",
    "Shae Rune", "Sol Rune", "Amn Rune", "Thul Rune", "Ort Rune",
    "Ral Rune", "Tal Rune", "Ith Rune", "Eth Rune", "Nef Rune",
    "Tir Rune", "Eld Rune", "El Rune"
};
static const struct { unsigned rune, id; const char *name; } patch_names[3] = {
    { 16, D2LANG_RUNE_IO_ID, "Io Rune" },
    { 13, D2LANG_RUNE_SHAEL_ID, "Shael Rune" },
    { 31, D2LANG_RUNE_JAH_ID, "Jah Rune" }
};

typedef const WORD *(__fastcall *lookup_fn)(unsigned);
typedef const WORD *(__fastcall *key_lookup_fn)(const char *);
static lookup_fn original_lookup;
static key_lookup_fn original_key_lookup;
static WORD expansion_color[33][40];
static WORD patch_color[3][40];

static void prepare(const char *name, WORD out[40])
{
    unsigned i;
    out[0] = 0xff; /* Diablo II text color prefix: ÿc8 = orange. */
    out[1] = 'c';
    out[2] = '8';
    for (i = 0; name[i] && i < 36; ++i)
        out[i + 3] = (BYTE)name[i];
    out[i + 3] = 0;
}

static int matches(const WORD *original, const char *expected)
{
    unsigned i;
    if (!original) return 0;
    /* Match the supplied English tables; a different build falls back. */
    for (i = 0; expected[i]; ++i)
        if (original[i] != (BYTE)expected[i]) return 0;
    return original[i] == 0;
}

static const WORD *__fastcall colored_lookup(unsigned id)
{
    unsigned i;
    const WORD *original = original_lookup(id);
    if (id >= D2LANG_RUNE_EXPANSION_FIRST_ID &&
        id <= D2LANG_RUNE_EXPANSION_LAST_ID) {
        i = id - D2LANG_RUNE_EXPANSION_FIRST_ID;
        if (matches(original, expansion_names[i])) return expansion_color[i];
    }
    for (i = 0; i < 3; ++i)
        if (id == patch_names[i].id && matches(original, patch_names[i].name))
            return patch_color[i];
    return original;
}

static const WORD *__fastcall colored_key_lookup(const char *key)
{
    const WORD *original = original_key_lookup(key);
    unsigned rune;
    unsigned i;
    if (!key || key[0] != 'r' || key[1] < '0' || key[1] > '9' ||
        key[2] < '0' || key[2] > '9' || key[3] != 0)
        return original;
    rune = (unsigned)(key[1] - '0') * 10 + (unsigned)(key[2] - '0');
    if (!rune || rune > 33) return original;
    i = 33 - rune; /* expansion_names runs from r33 down to r01. */
    if (matches(original, expansion_names[i])) return expansion_color[i];
    for (i = 0; i < 3; ++i)
        if (rune == patch_names[i].rune &&
            matches(original, patch_names[i].name))
            return patch_color[i];
    return original;
}

/* Locate this exact import instead of patching D2Lang code or other imports. */
static DWORD *find_lookup_import(HMODULE module, FARPROC expected, unsigned ordinal)
{
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        !nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress)
        return NULL;
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; ++imp) {
        IMAGE_THUNK_DATA32 *names, *addresses;
        unsigned i;
        if (!imp->OriginalFirstThunk ||
            lstrcmpiA((const char *)(base + imp->Name), "D2Lang.dll"))
            continue;
        names = (IMAGE_THUNK_DATA32 *)(base + imp->OriginalFirstThunk);
        addresses = (IMAGE_THUNK_DATA32 *)(base + imp->FirstThunk);
        for (i = 0; names[i].u1.AddressOfData; ++i) {
            if (IMAGE_SNAP_BY_ORDINAL32(names[i].u1.Ordinal) &&
                IMAGE_ORDINAL32(names[i].u1.Ordinal) == ordinal &&
                (FARPROC)(ULONG_PTR)addresses[i].u1.Function == expected)
                return &addresses[i].u1.Function;
        }
    }
    return NULL;
}

int rune_color_init(void)
{
    HMODULE lang = GetModuleHandleA("D2Lang.dll");
    HMODULE client = GetModuleHandleA("D2Client.dll");
    HMODULE common = GetModuleHandleA("D2Common.dll");
    FARPROC export_fn, key_export_fn;
    union { FARPROC raw; lookup_fn typed; } lookup;
    union { FARPROC raw; key_lookup_fn typed; } key_lookup;
    DWORD *client_import, *common_import, *client_key, *common_key;
    DWORD old_client, old_common, unused;
    BYTE *client_start, *common_start;
    SIZE_T client_size, common_size;
    unsigned i;
    if (!lang || !client || !common) return 0;
    export_fn = GetProcAddress(
        lang, MAKEINTRESOURCEA(D2LANG_LOOKUP_BY_ID_ORDINAL));
    key_export_fn = GetProcAddress(
        lang, MAKEINTRESOURCEA(D2LANG_LOOKUP_BY_KEY_ORDINAL));
    if (!export_fn || !key_export_fn) return 0;
    client_import = find_lookup_import(
        client, export_fn, D2LANG_LOOKUP_BY_ID_ORDINAL);
    common_import = find_lookup_import(
        common, export_fn, D2LANG_LOOKUP_BY_ID_ORDINAL);
    client_key = find_lookup_import(
        client, key_export_fn, D2LANG_LOOKUP_BY_KEY_ORDINAL);
    common_key = find_lookup_import(
        common, key_export_fn, D2LANG_LOOKUP_BY_KEY_ORDINAL);
    if (!client_import || !common_import || !client_key || !common_key) return 0;
    lookup.raw = export_fn;
    original_lookup = lookup.typed;
    key_lookup.raw = key_export_fn;
    original_key_lookup = key_lookup.typed;
    for (i = 0; i < 33; ++i)
        prepare(expansion_names[i], expansion_color[i]);
    for (i = 0; i < 3; ++i)
        prepare(patch_names[i].name, patch_color[i]);
    client_start = (BYTE *)((ULONG_PTR)client_import < (ULONG_PTR)client_key ? client_import : client_key);
    common_start = (BYTE *)((ULONG_PTR)common_import < (ULONG_PTR)common_key ? common_import : common_key);
    client_size = (SIZE_T)((BYTE *)((ULONG_PTR)client_import > (ULONG_PTR)client_key ? client_import : client_key)
                          - client_start) + sizeof(DWORD);
    common_size = (SIZE_T)((BYTE *)((ULONG_PTR)common_import > (ULONG_PTR)common_key ? common_import : common_key)
                          - common_start) + sizeof(DWORD);
    if (!VirtualProtect(client_start, client_size, PAGE_READWRITE,
                        &old_client)) return 0;
    if (!VirtualProtect(common_start, common_size, PAGE_READWRITE,
                        &old_common)) {
        VirtualProtect(client_start, client_size, old_client, &unused);
        return 0;
    }
    InterlockedExchange((volatile LONG *)client_import, (LONG)(ULONG_PTR)colored_lookup);
    InterlockedExchange((volatile LONG *)common_import, (LONG)(ULONG_PTR)colored_lookup);
    InterlockedExchange((volatile LONG *)client_key, (LONG)(ULONG_PTR)colored_key_lookup);
    InterlockedExchange((volatile LONG *)common_key, (LONG)(ULONG_PTR)colored_key_lookup);
    VirtualProtect(common_start, common_size, old_common, &unused);
    VirtualProtect(client_start, client_size, old_client, &unused);
    return 1;
}
