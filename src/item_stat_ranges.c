#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include "item_stat_ranges.h"
#include "d2_109b.h"
#include "armor_defense_bounds.h"

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
#define D2ITEM_QUALITY_SUPERIOR           3
#define D2ITEM_FLAG_ETHEREAL              0x400000u

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
#define D2STAT_ITEM_ARMOR_PERCENT          16u
#define D2STAT_ITEM_MAXDAMAGE_PERCENT      17u
#define D2STAT_ITEM_MINDAMAGE_PERCENT      18u
#define D2STAT_ARMOR_CLASS                31u
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
#define D2STAT_ITEM_NUMSOCKETS            194u

static BYTE *tooltip_continue;
static void *tooltip_item;
static DWORD tooltip_desc;
static WCHAR *tooltip_output;
static DWORD tooltip_return;
static void *item_tooltip_item;
static WCHAR *item_tooltip_output;
static DWORD item_tooltip_capacity;
static DWORD item_tooltip_return;
static BYTE *item_tooltip_continue;
static void *defense_line_item;
static WCHAR *defense_line_output;
static DWORD defense_line_return;
static BYTE *defense_line_continue;
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
static int (__stdcall *get_unit_stat)(const void *, DWORD);
static int (__stdcall *check_item_flag)(const void *, DWORD, int, const char *);

static void __attribute__((naked)) tooltip_after(void);
static void __attribute__((naked)) item_tooltip_after(void);
static void __attribute__((naked)) defense_line_after(void);

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

        /* Conditional member bonuses are shown in the set tooltip, but
           CodeA-E are separate tiers. Only infer a range when exactly one
           conditional slot can produce the line and no intrinsic source
           already matched it. Tal's belt MF is CodeC (10-15). */
        if (!range->matched) {
            RangeAccumulator conditional;
            unsigned i, matches = 0;

            ZeroMemory(&conditional, sizeof(conditional));
            for (i = D2SET_BASE_PROPERTY_COUNT;
                 i < D2SET_PROPERTIES_PER_MEMBER; ++i) {
                RangeAccumulator candidate;

                ZeroMemory(&candidate, sizeof(candidate));
                add_property(&candidate, record + property_offset +
                             i * D2SET_PROPERTY_STRIDE, stat);
                if (candidate.matched && candidate.variable) {
                    conditional = candidate;
                    ++matches;
                }
            }
            if (matches == 1)
                *range = conditional;
        }
        return;
    }
}

static void collect_item_range(RangeAccumulator *range, const void *item,
                               DWORD stat)
{
    ZeroMemory(range, sizeof(*range));
    if (!item || stat == 0xffffffffu ||
        *(const DWORD *)((const BYTE *)item + D2UNIT_TYPE_OFFSET) !=
            D2UNIT_ITEM)
        return;

    /* Automagic/prefix/suffix records are the authoritative roll source for
       magic/rare/crafted affixes, and automagic can also exist on other item
       qualities. Fixed contributors are intentionally included so a line fed
       by fixed + variable sources gets the true total possible range. */
    add_item_affixes(range, item, stat);
    add_unique_properties(range, item, stat);
    add_set_base_properties(range, item, stat);
}

static int find_item_range(const void *item, DWORD stat,
                           int *minimum, int *maximum)
{
    RangeAccumulator range;

    if (!minimum || !maximum)
        return 0;
    collect_item_range(&range, item, stat);

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

/* D2Client+0x3EAB0 assembles the armor's Defense line in its own output
   buffer. The bracket on this line must describe final item Defense. */
static int line_contains(const WCHAR *line, const WCHAR *end,
                         const WCHAR *needle)
{
    int size = lstrlenW(needle);
    const WCHAR *p;

    for (p = line; p + size <= end; ++p) {
        if (wcsncmp(p, needle, size) == 0)
            return 1;
    }
    return 0;
}

static void insert_tooltip_suffix(WCHAR *output, DWORD capacity,
                                  WCHAR *end, const WCHAR *suffix,
                                  int total_length)
{
    int suffix_length = lstrlenW(suffix);

    if ((DWORD)(total_length + suffix_length) >= capacity ||
        suffix_length > 48)
        return;
    MoveMemory(end + suffix_length, end,
               (total_length - (end - output) + 1) *
               sizeof(WCHAR));
    CopyMemory(end, suffix, suffix_length * sizeof(WCHAR));
}

static int read_defense_value(const WCHAR *start, const WCHAR *end, int *value)
{
    int n = 0;
    int found = 0;

    while (start < end) {
        if (*start == 0xff && end - start >= 3 && start[1] == L'c') {
            start += 3;
        } else if (*start == L' ' || *start == L'\t') {
            ++start;
        } else {
            break;
        }
    }
    while (start < end && *start >= L'0' && *start <= L'9') {
        found = 1;
        n = n * 10 + *start++ - L'0';
        if (n > 1000000)
            return 0;
    }
    *value = n;
    return found;
}

static int find_defense_bounds(const void *item,
                               const ArmorDefenseBounds *armor,
                               int displayed, int *minimum, int *maximum)
{
    RangeAccumulator percent, flat;
    LONGLONG base_low, base_high, lower, upper;
    int current_percent;

    collect_item_range(&percent, item, D2STAT_ITEM_ARMOR_PERCENT);
    collect_item_range(&flat, item, D2STAT_ARMOR_CLASS);
    if (percent.ambiguous_layer || flat.ambiguous_layer ||
        /* Socketed modifiers are not represented by the item's own record. */
        get_unit_stat(item, D2STAT_ITEM_NUMSOCKETS) != 0)
        return 0;

    current_percent = get_unit_stat(item, D2STAT_ITEM_ARMOR_PERCENT);
    if (percent.matched) {
        if (current_percent < percent.minimum ||
            current_percent > percent.maximum)
            return 0;
    } else if (current_percent != 0) {
        /* A superior item's innate ED is fixed; other unexplained ED can
           come from sockets or an effect that changes the base roll. */
        if (get_item_quality(item) != D2ITEM_QUALITY_SUPERIOR)
            return 0;
        percent.minimum = percent.maximum = current_percent;
    }
    if (percent.minimum < -99 || percent.maximum > 1000 ||
        flat.minimum < -10000 || flat.maximum > 10000)
        return 0;

    /* Armor spawned with enhanced defense rolls (maxac + 1). Ethereal
       multiplies that base before percentage ED; flat defense is added last.
       Without ED, the ordinary armor.txt base still has a random roll. */
    base_low = percent.matched || current_percent != 0 ?
        armor->maximum + 1 : armor->minimum;
    base_high = percent.matched || current_percent != 0 ?
        armor->maximum + 1 : armor->maximum;
    if (check_item_flag(item, D2ITEM_FLAG_ETHEREAL, 0, "item_stat_ranges.c")) {
        base_low = base_low * 3 / 2;
        base_high = base_high * 3 / 2;
    }
    lower = base_low * (100 + percent.minimum) / 100 + flat.minimum;
    upper = base_high * (100 + percent.maximum) / 100 + flat.maximum;

    /* Unknown modifiers and legacy items can have different Defense. A
       bracket that excludes the actual number is worse than no bracket. */
    if (lower < 0 || upper > INT_MAX || lower >= upper ||
        displayed < lower || displayed > upper)
        return 0;
    *minimum = (int)lower;
    *maximum = (int)upper;
    return 1;
}

static void append_item_base_ranges(const void *item, WCHAR *output,
                                    DWORD capacity, int defense_line)
{
    const BYTE *item_text;
    DWORD code;
    unsigned i;
    WCHAR *line;
    WCHAR *end;
    WCHAR suffix[56];
    int total_length;
    int defense_index = -1;
    int displayed_defense, defense_minimum, defense_maximum;
    int ed_minimum, ed_maximum;

    if (!item || !output ||
        capacity < 32 || capacity > 4096 ||
        *(const DWORD *)((const BYTE *)item +
                         D2UNIT_TYPE_OFFSET) != D2UNIT_ITEM)
        return;
    item_text = get_item_text(*(const DWORD *)((const BYTE *)item +
                                       D2UNIT_CLASS_ID_OFFSET));
    if (!item_text)
        return;
    code = *(const DWORD *)(item_text + D2ITEMTXT_CODE_OFFSET) &
           D2ITEM_CODE_3CHAR_MASK;
    if (defense_line) {
        for (i = 0; i < sizeof(armor_defense_bounds) /
                        sizeof(armor_defense_bounds[0]); ++i)
            if (armor_defense_bounds[i].code == code) {
                defense_index = (int)i;
                break;
            }
        if (defense_index < 0)
            return;
    } else if (code != ('j' | ('e' << 8) | ('w' << 16))) {
        return;
    }

    total_length = lstrlenW(output);
    if (total_length < 0 || (DWORD)total_length >= capacity)
        return;
    for (line = output; *line; line = end + (*end == L'\n')) {
        WCHAR *visible = line;

        end = line;
        while (*end && *end != L'\n')
            ++end;
        /* Item colors are embedded as \xff c N before the visible text. */
        while (visible + 3 <= end && visible[0] == 0xff &&
               visible[1] == L'c')
            visible += 3;
        /* The 1.09b English tooltip uses "Defense:" for the armor value. */
        if (defense_line && defense_index >= 0) {
            const WCHAR *label;
            for (label = visible; label + 8 <= end; ++label) {
                if (wcsncmp(label, L"Defense:", 8) != 0)
                    continue;
                if (read_defense_value(label + 8, end, &displayed_defense) &&
                    find_defense_bounds(item,
                        &armor_defense_bounds[defense_index],
                        displayed_defense, &defense_minimum,
                        &defense_maximum)) {
                    wsprintfW(suffix,
                              L" \x00ff" L"c:[%d - %d]\x00ff" L"c0",
                              defense_minimum, defense_maximum);
                    if (end - line + lstrlenW(suffix) <= 220)
                        insert_tooltip_suffix(output, capacity, end, suffix,
                                              total_length);
                }
                return;
            }
        }
        /* ED on a jewel can be assembled outside the stat-line formatter.
           Append its affix roll once, regardless of which path made the line. */
        if (!defense_line &&
            line_contains(visible, end, L"Enhanced Damage") &&
            !line_contains(visible, end, L"[")) {
            if (find_item_range(item,
                                D2STAT_ITEM_MAXDAMAGE_PERCENT,
                                &ed_minimum, &ed_maximum) ||
                find_item_range(item,
                                D2STAT_ITEM_MINDAMAGE_PERCENT,
                                &ed_minimum, &ed_maximum)) {
                wsprintfW(suffix, L" \x00ff" L"c:[%d - %d]\x00ff" L"c0",
                          ed_minimum, ed_maximum);
                if (end - line + lstrlenW(suffix) <= 220)
                    insert_tooltip_suffix(output, capacity, end, suffix,
                                          total_length);
            }
            return;
        }
        if (!*end)
            break;
    }
}

static void append_item_modifier_ranges(void)
{
    append_item_base_ranges(item_tooltip_item, item_tooltip_output,
                            item_tooltip_capacity, 0);
}

static void append_defense_line_range(void)
{
    /* The two callers allocate at least 0x200 WCHARs for this line. Use a
       smaller limit so insertion cannot approach either buffer boundary. */
    append_item_base_ranges(defense_line_item, defense_line_output, 256, 1);
}

/* This function receives ECX=item, EDX=output and its first stack argument
   is the WCHAR output capacity. Its original prologue is six bytes. */
static void __attribute__((naked)) item_tooltip_hook(void)
{
    __asm__ __volatile__(
        "movl %%ecx, %0\n\t"
        "movl %%edx, %1\n\t"
        "movl 4(%%esp), %%eax\n\t"
        "movl %%eax, %2\n\t"
        "movl (%%esp), %%eax\n\t"
        "movl %%eax, %3\n\t"
        "movl $%P4, (%%esp)\n\t"
        "subl $0x4ec, %%esp\n\t"
        "jmp *%5\n\t"
        : "=m" (item_tooltip_item), "=m" (item_tooltip_output),
          "=m" (item_tooltip_capacity), "=m" (item_tooltip_return)
        : "i" (item_tooltip_after), "m" (item_tooltip_continue)
        : "eax");
}

static void __attribute__((naked)) item_tooltip_after(void)
{
    __asm__ __volatile__(
        "pushfl\n\t"
        "pushal\n\t"
        "call %P0\n\t"
        "popal\n\t"
        "popfl\n\t"
        "jmp *%1\n\t"
        : : "i" (append_item_modifier_ranges), "m" (item_tooltip_return));
}

/* The five displaced bytes of D2Client+0x3EAB0 are sub esp, 0x30;
   push ebx; push ebp. ECX is the item and EDX the Defense line buffer. */
static void __attribute__((naked)) defense_line_hook(void)
{
    __asm__ __volatile__(
        "movl %%ecx, %0\n\t"
        "movl %%edx, %1\n\t"
        "movl (%%esp), %%eax\n\t"
        "movl %%eax, %2\n\t"
        "movl $%P3, (%%esp)\n\t"
        "subl $0x30, %%esp\n\t"
        "pushl %%ebx\n\t"
        "pushl %%ebp\n\t"
        "jmp *%4\n\t"
        : "=m" (defense_line_item), "=m" (defense_line_output),
          "=m" (defense_line_return)
        : "i" (defense_line_after), "m" (defense_line_continue)
        : "eax");
}

static void __attribute__((naked)) defense_line_after(void)
{
    __asm__ __volatile__(
        "pushfl\n\t"
        "pushal\n\t"
        "call %P0\n\t"
        "popal\n\t"
        "popfl\n\t"
        "jmp *%1\n\t"
        : : "i" (append_defense_line_range), "m" (defense_line_return));
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
    BYTE *armor_site;
    BYTE *defense_site;
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
    union { FARPROC raw; int (__stdcall *typed)(const void *, DWORD); } unit_stat;
    union { FARPROC raw; int (__stdcall *typed)(const void *, DWORD,
                                              int, const char *); } item_flag;
    static const BYTE expected[] = { 0x81, 0xec, 0x30, 0x02, 0x00, 0x00 };
    static const BYTE armor_expected[] = {
        0x81, 0xec, 0xec, 0x04, 0x00, 0x00
    };
    static const BYTE defense_expected[] = { 0x83, 0xec, 0x30, 0x53, 0x55 };

    client_base = (BYTE *)GetModuleHandleA("D2Client.dll");
    common = GetModuleHandleA("D2Common.dll");
    common_base = (BYTE *)common;
    if (!client_base || !common_base || !common_layout_matches())
        return 0;

    site = client_base + D2CLIENT_FN_TOOLTIP_STAT_LINE_OFFSET;
    if (memcmp(site, expected, sizeof(expected)) != 0)
        return 0;
    armor_site = client_base + D2CLIENT_FN_ITEM_TOOLTIP_OFFSET;
    if (memcmp(armor_site, armor_expected, sizeof(armor_expected)) != 0)
        return 0;
    defense_site = client_base + D2CLIENT_FN_DEFENSE_LINE_OFFSET;
    if (memcmp(defense_site, defense_expected, sizeof(defense_expected)) != 0)
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
    unit_stat.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_GET_UNIT_STAT_ORDINAL));
    item_flag.raw = GetProcAddress(common,
        MAKEINTRESOURCEA(D2COMMON_CHECK_ITEM_FLAG_ORDINAL));
    if (!quality.raw || !file_index.raw || !item_text.raw ||
        !affix_record.raw || !unique_record.raw || !set_record.raw ||
        !auto_affix.raw || !prefix.raw || !suffix.raw ||
        !unit_stat.raw || !item_flag.raw)
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
    get_unit_stat = unit_stat.typed;
    check_item_flag = item_flag.typed;

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

    item_tooltip_continue = armor_site + sizeof(armor_expected);
    if (!VirtualProtect(armor_site, sizeof(armor_expected),
                        PAGE_EXECUTE_READWRITE, &old_protection))
        return 1; /* Stat-line ranges are still installed. */
    armor_site[0] = 0xe9;
    *(DWORD *)(armor_site + 1) =
        (DWORD)((BYTE *)item_tooltip_hook - (armor_site + 5));
    armor_site[5] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), armor_site,
                          sizeof(armor_expected));
    VirtualProtect(armor_site, sizeof(armor_expected), old_protection,
                   &unused);

    defense_line_continue = defense_site + sizeof(defense_expected);
    if (!VirtualProtect(defense_site, sizeof(defense_expected),
                        PAGE_EXECUTE_READWRITE, &old_protection))
        return 1;
    defense_site[0] = 0xe9;
    *(DWORD *)(defense_site + 1) =
        (DWORD)((BYTE *)defense_line_hook - (defense_site + 5));
    FlushInstructionCache(GetCurrentProcess(), defense_site,
                          sizeof(defense_expected));
    VirtualProtect(defense_site, sizeof(defense_expected), old_protection,
                   &unused);
    return 1;
}
