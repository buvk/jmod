#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <string.h>
#include "item_stat_ranges.h"
#include "d2_109b.h"

/* 1.09b UniqueItems.bin is 0xE4 bytes. The ten property slots begin at
   +0x44 and are the compiled prop/param/min/max columns from UniqueItems.txt. */
#define D2UNIQUE_PROPERTY_OFFSET 0x44u
#define D2UNIQUE_PROPERTY_COUNT  10u
#define D2UNIQUE_PROPERTY_STRIDE 0x10u

/* 1.09b magicprefix/magicsuffix/automagic records are 0x84 bytes. The
   leading modifier layout is shared: three prop/param/min/max slots. */
#define D2AFFIX_PROPERTY_OFFSET 0x24u
#define D2AFFIX_PROPERTY_COUNT  3u
#define D2AFFIX_PROPERTY_STRIDE 0x10u
#define D2ITEM_AFFIX_SLOT_COUNT 3u

/* 1.09b SetItems.bin is the old combined-set format. D2Common and D2Client
   both locate a member from the six 0x3C-byte descriptors at +0x8C. The
   member properties are seven 0x10-byte slots per member starting at +0x1F4.
   Code1/Code2 are the member's intrinsic properties; CodeA-E are conditional
   partial-set properties, so only the first two are safe without set-state
   information. */
#define D2SET_MEMBER_OFFSET              0x8cu
#define D2SET_MEMBER_COUNT               6u
#define D2SET_MEMBER_STRIDE              0x3cu
#define D2SET_PROPERTIES_OFFSET          0x1f4u
#define D2SET_PROPERTIES_PER_MEMBER      7u
#define D2SET_BASE_PROPERTY_COUNT        2u
#define D2SET_PROPERTY_STRIDE            0x10u

/* D2Common's 1.09b property dispatcher is an 8-byte table. Its second
   dword is the primary ItemStatCost id for most properties. */
#define D2COMMON_PROPERTY_DISPATCH_OFFSET 0x95bd0u
#define D2COMMON_PROPERTY_COUNT_OFFSET    0x96370u
#define D2COMMON_EXPECTED_PROPERTY_COUNT  244u

#define D2ITEM_QUALITY_SET                5
#define D2ITEM_QUALITY_UNIQUE             7

/* Properties.txt row ids in the exact 244-row 1.09b property table. */
#define D2PROP_DAMAGE_PERCENT              29u
#define D2PROP_RESIST_ALL                  41u
#define D2PROP_MAX_RESIST_ALL              42u
#define D2PROP_AMAZON_SKILLS               67u
#define D2PROP_PALADIN_SKILLS              68u
#define D2PROP_NECROMANCER_SKILLS          69u
#define D2PROP_SORCERESS_SKILLS            70u
#define D2PROP_BARBARIAN_SKILLS            71u
#define D2PROP_REANIMATE                   117u
#define D2PROP_DRUID_SKILLS                121u
#define D2PROP_ASSASSIN_SKILLS             122u
#define D2PROP_SINGLE_SKILL                123u
#define D2PROP_SKILL_TAB                   124u
#define D2PROP_AURA                        125u
#define D2PROP_ATTACK_SKILL                126u
#define D2PROP_HIT_SKILL                   127u
#define D2PROP_GETHIT_SKILL                128u
#define D2PROP_FIRE_DAMAGE                 134u
#define D2PROP_LIGHTNING_DAMAGE            135u
#define D2PROP_MAGIC_DAMAGE                136u
#define D2PROP_COLD_DAMAGE                 137u
#define D2PROP_POISON_DAMAGE               138u
#define D2PROP_THROW_DAMAGE                139u
#define D2PROP_NORMAL_DAMAGE               140u
#define D2PROP_PER_LEVEL_FIRST             141u
#define D2PROP_REPLENISH_QUANTITY          180u
#define D2PROP_BY_TIME_FIRST               195u
#define D2PROP_BY_TIME_LAST                231u
#define D2PROP_CHARGED_SKILL               243u

/* Exact 1.09b ItemStatCost ids used by multi-slot/multi-stat properties. */
#define D2STAT_ITEM_MAXDAMAGE_PERCENT      17u
#define D2STAT_ITEM_MINDAMAGE_PERCENT      18u
#define D2STAT_FIRE_RESIST                 39u
#define D2STAT_MAX_FIRE_RESIST             40u
#define D2STAT_LIGHT_RESIST                41u
#define D2STAT_MAX_LIGHT_RESIST            42u
#define D2STAT_COLD_RESIST                 43u
#define D2STAT_MAX_COLD_RESIST             44u
#define D2STAT_POISON_RESIST               45u
#define D2STAT_MAX_POISON_RESIST           46u
#define D2STAT_SINGLE_SKILL1              107u
#define D2STAT_SINGLE_SKILL3              109u
#define D2STAT_SINGLE_SKILL4              181u
#define D2STAT_SINGLE_SKILL10             187u
#define D2STAT_SKILL_TAB1                 188u
#define D2STAT_SKILL_TAB6                 193u

static BYTE *tooltip_continue;
static void *tooltip_item;
static DWORD tooltip_desc;
static WCHAR *tooltip_output;
static DWORD tooltip_return;
static BYTE *client_base;
static BYTE *common_base;

static int (__stdcall *get_item_quality)(const void *);
static int (__stdcall *get_item_file_index)(const void *);
static const BYTE *(__stdcall *get_item_text)(DWORD);
static const BYTE *(__stdcall *get_magic_affix_record)(int);
static const BYTE *(__stdcall *get_unique_record)(int);
static const BYTE *(__stdcall *get_set_record)(int);
static WORD (__stdcall *get_auto_affix)(const void *);
static WORD (__stdcall *get_prefix_id)(const void *, int);
static WORD (__stdcall *get_suffix_id)(const void *, int);

static void __attribute__((naked)) tooltip_after(void);

typedef struct RangeAccumulator {
    LONGLONG minimum;
    LONGLONG maximum;
    DWORD layer_property;
    int layer_param;
    int matched;
    int variable;
    int layer_seen;
    int ambiguous_layer;
} RangeAccumulator;

static int common_layout_matches(void)
{
    DWORD count;
    DWORD first_stat;
    DWORD strength_stat;

    if (!common_base)
        return 0;
    count = *(const DWORD *)(common_base + D2COMMON_PROPERTY_COUNT_OFFSET);
    first_stat = *(const DWORD *)(common_base +
        D2COMMON_PROPERTY_DISPATCH_OFFSET + 4);
    strength_stat = *(const DWORD *)(common_base +
        D2COMMON_PROPERTY_DISPATCH_OFFSET + 7 * 8 + 4);

    /* Properties.txt rows 0 and 7 are ac and str in this exact build. */
    return count == D2COMMON_EXPECTED_PROPERTY_COUNT &&
           first_stat == 31u && strength_stat == 0u;
}

static DWORD descriptor_stat(DWORD descriptor)
{
    /* D2Client+0xDAF98 is the 16-byte tooltip descriptor table consumed by
       D2Client+0x4BB20. The first dword is the ItemStatCost id. */
    if (descriptor >= 256u)
        return 0xffffffffu;
    return *(const DWORD *)(client_base +
        D2CLIENT_TOOLTIP_DESCRIPTOR_TABLE_OFFSET + descriptor * 16u);
}

static int property_uses_roll_bounds(DWORD property)
{
    /* These min/max fields describe two different values or a value that
       changes with level/time; they are not an item's random roll bounds. */
    if (property == D2PROP_ATTACK_SKILL ||
        property == D2PROP_HIT_SKILL ||
        property == D2PROP_GETHIT_SKILL ||
        property == D2PROP_CHARGED_SKILL)
        return 0;

    if (property >= D2PROP_FIRE_DAMAGE &&
        property <= D2PROP_NORMAL_DAMAGE)
        return 0;

    if (property >= D2PROP_PER_LEVEL_FIRST &&
        property <= D2PROP_REPLENISH_QUANTITY)
        return 0;

    if (property >= D2PROP_BY_TIME_FIRST &&
        property <= D2PROP_BY_TIME_LAST)
        return 0;

    return 1;
}

static int property_is_layered(DWORD property)
{
    switch (property) {
    case D2PROP_AMAZON_SKILLS:
    case D2PROP_PALADIN_SKILLS:
    case D2PROP_NECROMANCER_SKILLS:
    case D2PROP_SORCERESS_SKILLS:
    case D2PROP_BARBARIAN_SKILLS:
    case D2PROP_REANIMATE:
    case D2PROP_DRUID_SKILLS:
    case D2PROP_ASSASSIN_SKILLS:
    case D2PROP_SINGLE_SKILL:
    case D2PROP_SKILL_TAB:
    case D2PROP_AURA:
        return 1;
    default:
        return 0;
    }
}

static int property_matches_stat(int property, DWORD primary_stat, DWORD stat)
{
    if (primary_stat == stat)
        return 1;

    /* The dispatcher has no single primary stat for these properties. */
    switch ((DWORD)property) {
    case D2PROP_DAMAGE_PERCENT:
        return stat == D2STAT_ITEM_MAXDAMAGE_PERCENT ||
               stat == D2STAT_ITEM_MINDAMAGE_PERCENT;

    case D2PROP_RESIST_ALL:
        return stat == D2STAT_FIRE_RESIST ||
               stat == D2STAT_LIGHT_RESIST ||
               stat == D2STAT_COLD_RESIST ||
               stat == D2STAT_POISON_RESIST;

    case D2PROP_MAX_RESIST_ALL:
        return stat == D2STAT_MAX_FIRE_RESIST ||
               stat == D2STAT_MAX_LIGHT_RESIST ||
               stat == D2STAT_MAX_COLD_RESIST ||
               stat == D2STAT_MAX_POISON_RESIST;

    /* 1.09b stores several simultaneous +single-skill/+skill-tab modifiers
       in separate ItemStatCost slots. The property dispatcher names only the
       first slot, so accept every slot that can hold that property. */
    case D2PROP_SINGLE_SKILL:
        return (stat >= D2STAT_SINGLE_SKILL1 &&
                stat <= D2STAT_SINGLE_SKILL3) ||
               (stat >= D2STAT_SINGLE_SKILL4 &&
                stat <= D2STAT_SINGLE_SKILL10);

    case D2PROP_SKILL_TAB:
        return stat >= D2STAT_SKILL_TAB1 && stat <= D2STAT_SKILL_TAB6;

    default:
        return 0;
    }
}

static void add_property(RangeAccumulator *range, const BYTE *slot, DWORD stat)
{
    DWORD property_count;
    DWORD property_stat;
    DWORD property;
    int param;
    int min_value;
    int max_value;
    int low;
    int high;

    if (!range || !slot)
        return;

    property = *(const DWORD *)(slot + 0x00);
    param = *(const int *)(slot + 0x04);
    min_value = *(const int *)(slot + 0x08);
    max_value = *(const int *)(slot + 0x0c);
    property_count = *(const DWORD *)(common_base +
        D2COMMON_PROPERTY_COUNT_OFFSET);

    if (property == 0xffffffffu || property >= property_count ||
        !property_uses_roll_bounds(property))
        return;

    property_stat = *(const DWORD *)(common_base +
        D2COMMON_PROPERTY_DISPATCH_OFFSET + property * 8u + 4u);
    if (!property_matches_stat((int)property, property_stat, stat))
        return;

    if (property_is_layered(property)) {
        if (!range->layer_seen) {
            range->layer_seen = 1;
            range->layer_property = property;
            range->layer_param = param;
        } else if (range->layer_property != property ||
                   range->layer_param != param) {
            /* The formatter's descriptor identifies the stat slot but not
               the layer/skill id. With more than one layer present, do not
               guess which property generated this particular line. */
            range->ambiguous_layer = 1;
        }
    }

    low = min_value < max_value ? min_value : max_value;
    high = min_value < max_value ? max_value : min_value;
    range->minimum += (LONGLONG)low;
    range->maximum += (LONGLONG)high;
    range->matched = 1;
    if (low != high)
        range->variable = 1;
}

static void add_property_slots(RangeAccumulator *range, const BYTE *record,
                               DWORD offset, unsigned count, DWORD stride,
                               DWORD stat)
{
    unsigned i;

    if (!record)
        return;
    for (i = 0; i < count; ++i)
        add_property(range, record + offset + i * stride, stat);
}

static void add_affix(RangeAccumulator *range, WORD affix_id, DWORD stat)
{
    const BYTE *record;

    if (!affix_id)
        return;
    record = get_magic_affix_record((int)affix_id);
    if (!record)
        return;
    add_property_slots(range, record, D2AFFIX_PROPERTY_OFFSET,
                       D2AFFIX_PROPERTY_COUNT, D2AFFIX_PROPERTY_STRIDE, stat);
}

static void add_item_affixes(RangeAccumulator *range, const void *item,
                             DWORD stat)
{
    unsigned i;

    add_affix(range, get_auto_affix(item), stat);
    for (i = 0; i < D2ITEM_AFFIX_SLOT_COUNT; ++i) {
        add_affix(range, get_prefix_id(item, (int)i), stat);
        add_affix(range, get_suffix_id(item, (int)i), stat);
    }
}

static void add_unique_properties(RangeAccumulator *range, const void *item,
                                  DWORD stat)
{
    const BYTE *record;

    if (get_item_quality(item) != D2ITEM_QUALITY_UNIQUE)
        return;
    record = get_unique_record(get_item_file_index(item));
    add_property_slots(range, record, D2UNIQUE_PROPERTY_OFFSET,
                       D2UNIQUE_PROPERTY_COUNT,
                       D2UNIQUE_PROPERTY_STRIDE, stat);
}

static void add_set_base_properties(RangeAccumulator *range, const void *item,
                                    DWORD stat)
{
    const BYTE *record;
    const BYTE *item_text;
    DWORD item_code;
    unsigned member;

    if (get_item_quality(item) != D2ITEM_QUALITY_SET)
        return;

    record = get_set_record(get_item_file_index(item));
    item_text = get_item_text(
        *(const DWORD *)((const BYTE *)item + D2UNIT_CLASS_ID_OFFSET));
    if (!record || !item_text)
        return;

    item_code = *(const DWORD *)(item_text + D2ITEMTXT_CODE_OFFSET);
    for (member = 0; member < D2SET_MEMBER_COUNT; ++member) {
        const BYTE *member_record = record + D2SET_MEMBER_OFFSET +
            member * D2SET_MEMBER_STRIDE;
        DWORD property_offset;

        if (*(const DWORD *)member_record != item_code)
            continue;

        property_offset = D2SET_PROPERTIES_OFFSET +
            member * D2SET_PROPERTIES_PER_MEMBER * D2SET_PROPERTY_STRIDE;
        add_property_slots(range, record, property_offset,
                           D2SET_BASE_PROPERTY_COUNT,
                           D2SET_PROPERTY_STRIDE, stat);
        return;
    }
}

static int find_item_range(const void *item, DWORD stat,
                           int *minimum, int *maximum)
{
    RangeAccumulator range;

    if (!item || !minimum || !maximum || stat == 0xffffffffu ||
        *(const DWORD *)((const BYTE *)item + D2UNIT_TYPE_OFFSET) !=
            D2UNIT_ITEM)
        return 0;

    ZeroMemory(&range, sizeof(range));

    /* Automagic/prefix/suffix records are the authoritative roll source for
       magic/rare/crafted affixes, and automagic can also exist on other item
       qualities. Fixed contributors are intentionally included so a line fed
       by fixed + variable sources gets the true total possible range. */
    add_item_affixes(&range, item, stat);
    add_unique_properties(&range, item, stat);
    add_set_base_properties(&range, item, stat);

    if (!range.matched || !range.variable || range.ambiguous_layer ||
        range.minimum == range.maximum ||
        range.minimum < INT_MIN || range.minimum > INT_MAX ||
        range.maximum < INT_MIN || range.maximum > INT_MAX)
        return 0;

    *minimum = (int)range.minimum;
    *maximum = (int)range.maximum;
    return 1;
}

static void append_current_range(void)
{
    WCHAR suffix[48];
    int minimum, maximum;
    int length;
    DWORD stat;

    if (!tooltip_output || !tooltip_item)
        return;
    stat = descriptor_stat(tooltip_desc);
    if (!find_item_range(tooltip_item, stat, &minimum, &maximum))
        return;

    /* D2 tooltip lines are short, but keep generous headroom in the game's
       256-WCHAR line buffers. ':' is the original client's darker green;
       restore color 3 (blue) so later text keeps vanilla unique-mod color. */
    length = lstrlenW(tooltip_output);
    if (length < 0 || length > 220)
        return;
    wsprintfW(suffix, L" \x00ff" L"c:[%d - %d]\x00ff" L"c3",
              minimum, maximum);
    lstrcatW(tooltip_output, suffix);
}

/* Replace the formatter's return address, then execute its displaced
   `sub esp, 0x230`. D2Client+0x4BB20 uses ECX=item and EDX=descriptor.
   Its fourth stack argument is the destination WCHAR buffer. */
static void __attribute__((naked)) tooltip_hook(void)
{
    __asm__ __volatile__(
        "movl %%ecx, %0\n\t"
        "movl %%edx, %1\n\t"
        "movl 16(%%esp), %%eax\n\t"
        "movl %%eax, %2\n\t"
        "movl (%%esp), %%eax\n\t"
        "movl %%eax, %3\n\t"
        "movl $%P4, (%%esp)\n\t"
        "subl $0x230, %%esp\n\t"
        "jmp *%5\n\t"
        : "=m" (tooltip_item), "=m" (tooltip_desc),
          "=m" (tooltip_output), "=m" (tooltip_return)
        : "i" (tooltip_after), "m" (tooltip_continue)
        : "eax");
}

static void __attribute__((naked)) tooltip_after(void)
{
    __asm__ __volatile__(
        "pushfl\n\t"
        "pushal\n\t"
        "call %P0\n\t"
        "popal\n\t"
        "popfl\n\t"
        "jmp *%1\n\t"
        : : "i" (append_current_range), "m" (tooltip_return));
}

int item_stat_ranges_init(void)
{
    BYTE *site;
    DWORD old_protection, unused;
    HMODULE common;
    union { FARPROC raw; int (__stdcall *typed)(const void *); } quality;
    union { FARPROC raw; int (__stdcall *typed)(const void *); } file_index;
    union { FARPROC raw; const BYTE *(__stdcall *typed)(DWORD); } item_text;
    union { FARPROC raw; const BYTE *(__stdcall *typed)(int); } affix_record;
    union { FARPROC raw; const BYTE *(__stdcall *typed)(int); } unique_record;
    union { FARPROC raw; const BYTE *(__stdcall *typed)(int); } set_record;
    union { FARPROC raw; WORD (__stdcall *typed)(const void *); } auto_affix;
    union { FARPROC raw; WORD (__stdcall *typed)(const void *, int); } prefix;
    union { FARPROC raw; WORD (__stdcall *typed)(const void *, int); } suffix;
    static const BYTE expected[] = { 0x81, 0xec, 0x30, 0x02, 0x00, 0x00 };

    client_base = (BYTE *)GetModuleHandleA("D2Client.dll");
    common = GetModuleHandleA("D2Common.dll");
    common_base = (BYTE *)common;
    if (!client_base || !common_base || !common_layout_matches())
        return 0;

    site = client_base + D2CLIENT_FN_TOOLTIP_STAT_LINE_OFFSET;
    if (memcmp(site, expected, sizeof(expected)) != 0)
        return 0;

    quality.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_ITEM_QUALITY_ORDINAL));
    file_index.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_ITEM_FILE_INDEX_ORDINAL));
    item_text.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_ITEM_TEXT_ORDINAL));
    affix_record.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_MAGIC_AFFIX_RECORD_ORDINAL));
    unique_record.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_UNIQUE_RECORD_ORDINAL));
    set_record.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_SET_RECORD_ORDINAL));
    auto_affix.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_AUTO_AFFIX_ORDINAL));
    prefix.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_PREFIX_ID_ORDINAL));
    suffix.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_SUFFIX_ID_ORDINAL));
    if (!quality.raw || !file_index.raw || !item_text.raw ||
        !affix_record.raw || !unique_record.raw || !set_record.raw ||
        !auto_affix.raw || !prefix.raw || !suffix.raw)
        return 0;

    get_item_quality = quality.typed;
    get_item_file_index = file_index.typed;
    get_item_text = item_text.typed;
    get_magic_affix_record = affix_record.typed;
    get_unique_record = unique_record.typed;
    get_set_record = set_record.typed;
    get_auto_affix = auto_affix.typed;
    get_prefix_id = prefix.typed;
    get_suffix_id = suffix.typed;

    tooltip_continue = site + sizeof(expected);
    if (!VirtualProtect(site, sizeof(expected), PAGE_EXECUTE_READWRITE,
                        &old_protection))
        return 0;
    site[0] = 0xe9;
    *(DWORD *)(site + 1) =
        (DWORD)((BYTE *)tooltip_hook - (site + 5));
    site[5] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(expected));
    VirtualProtect(site, sizeof(expected), old_protection, &unused);
    return 1;
}
