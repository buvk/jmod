#include "config.h"
#include <string.h>

static int potion_tier(const char *value)
{
    static const char *const names[] = {
        "Any", "Minor", "Light", "Standard", "Greater", "Super", "None"
    };
    unsigned i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (lstrcmpiA(value, names[i]) == 0)
            return (int)i;
    return 0;
}

static int rejuv_tier(const char *value)
{
    if (lstrcmpiA(value, "Full") == 0) return 1;
    if (lstrcmpiA(value, "None") == 0) return 2;
    return 0;
}

void read_options(HMODULE module, D2ModConfig *config)
{
    char path[MAX_PATH];
    char *slash;
    int value;
    DWORD length = GetModuleFileNameA(module, path, MAX_PATH);

    if (!config)
        return;
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
    {
        char option[32];
        GetPrivateProfileStringA("LootFilter", "MinHealthPotion", "Any", option, sizeof(option), path);
        config->min_health_potion = potion_tier(option);
        GetPrivateProfileStringA("LootFilter", "MinManaPotion", "Any", option, sizeof(option), path);
        config->min_mana_potion = potion_tier(option);
        GetPrivateProfileStringA("LootFilter", "MinRejuvenationPotion", "Any", option, sizeof(option), path);
        config->min_rejuvenation_potion = rejuv_tier(option);
    }
}
