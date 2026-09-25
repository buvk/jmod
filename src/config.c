#include "config.h"
#include <string.h>

static void set_defaults(D2ModConfig *config)
{
    config->quick_cast = 1;
    config->always_show_items = 1;
    config->auto_gold_pickup = 1;
    config->gold_pickup_in_town = 0;
    config->rune_color = 1;
    config->gold_pickup_range = 4;
    config->gold_scan_interval_ms = 30;
    config->gold_request_interval_ms = 50;
    config->gold_retry_interval_ms = 500;
    config->loot_filter_enabled = 0;
    config->min_gold = 0;
}

void read_options(HMODULE module, D2ModConfig *config)
{
    char path[MAX_PATH];
    char *slash;
    int value;
    DWORD length;

    if (!config)
        return;

    /* Keep usable defaults even if the INI path cannot be resolved. */
    set_defaults(config);
    if (!module)
        return;

    length = GetModuleFileNameA(module, path, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return;

    slash = strrchr(path, '\\');
    if (!slash || (size_t)(path + sizeof(path) - (slash + 1)) <
                  sizeof("jmod.ini"))
        return;

    memcpy(slash + 1, "jmod.ini", sizeof("jmod.ini"));

    config->quick_cast = GetPrivateProfileIntA("Mods", "QuickCast", 1, path) != 0;
    config->always_show_items = GetPrivateProfileIntA("Mods", "AlwaysShowItems", 1, path) != 0;
    config->auto_gold_pickup = GetPrivateProfileIntA("Mods", "AutoGoldPickup", 1, path) != 0;
    config->gold_pickup_in_town = GetPrivateProfileIntA("Mods", "GoldPickupInTown", 0, path) != 0;
    config->rune_color = GetPrivateProfileIntA("Mods", "RuneColor", 1, path) != 0;
    config->gold_pickup_range = GetPrivateProfileIntA("Mods", "GoldPickupRange", 4, path);
    if (config->gold_pickup_range < 1)
        config->gold_pickup_range = 1;
    if (config->gold_pickup_range > 6)
        config->gold_pickup_range = 6;

    value = GetPrivateProfileIntA("Mods", "GoldScanIntervalMs", 30, path);
    config->gold_scan_interval_ms = (DWORD)(value < 10 ? 10 : value > 1000 ? 1000 : value);

    value = GetPrivateProfileIntA("Mods", "GoldRequestIntervalMs", 50, path);
    config->gold_request_interval_ms = (DWORD)(value < 10 ? 10 : value > 1000 ? 1000 : value);

    value = GetPrivateProfileIntA("Mods", "GoldRetryIntervalMs", 500, path);
    config->gold_retry_interval_ms = (DWORD)(value < 50 ? 50 : value > 10000 ? 10000 : value);

    config->loot_filter_enabled = GetPrivateProfileIntA("LootFilter", "Enabled", 0, path) != 0;
    value = GetPrivateProfileIntA("LootFilter", "MinGold", 0, path);
    config->min_gold = value < 0 ? 0 : value;
}
