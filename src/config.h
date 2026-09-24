#ifndef D2MOD_CONFIG_H
#define D2MOD_CONFIG_H

#include <windows.h>

typedef struct D2ModConfig {
    int quick_cast;
    int always_show_items;
    int auto_gold_pickup;
    int gold_pickup_in_town;
    int rune_color;
    int gold_pickup_range;
    DWORD gold_scan_interval_ms;
    DWORD gold_request_interval_ms;
    DWORD gold_retry_interval_ms;
    int loot_filter_enabled;
    int min_gold;
    int min_health_potion;
    int min_mana_potion;
    int min_rejuvenation_potion;
} D2ModConfig;

void read_options(HMODULE module, D2ModConfig *config);

#endif
