#include "config.h"
#include "d2_109b.h"
#include <string.h>

static void set_defaults(D2ModConfig *config)
{
    config->quick_cast = 1;
    config->always_show_items = 1;
    config->auto_gold_pickup = 1;
    config->gold_pickup_in_town = 0;
    config->rune_color = 1;
    config->gold_pickup_range = D2ITEM_IMMEDIATE_PICKUP_MAX_DISTANCE;
    config->loot_filter_enabled = 0;
    config->loot_filter_in_town = 0;
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
    config->gold_pickup_range = GetPrivateProfileIntA("Mods", "GoldPickupRange",
        D2ITEM_IMMEDIATE_PICKUP_MAX_DISTANCE, path);
    if (config->gold_pickup_range < 1)
        config->gold_pickup_range = 1;
    /* D2Game 1.09b picks an item up immediately only at distance <= 4.
       Larger values make the interaction path move the player toward it. */
    if (config->gold_pickup_range > D2ITEM_IMMEDIATE_PICKUP_MAX_DISTANCE)
        config->gold_pickup_range = D2ITEM_IMMEDIATE_PICKUP_MAX_DISTANCE;

    config->loot_filter_enabled = GetPrivateProfileIntA("LootFilter", "Enabled", 0, path) != 0;
    config->loot_filter_in_town = GetPrivateProfileIntA("LootFilter", "FilterInTown", 0, path) != 0;
    value = GetPrivateProfileIntA("LootFilter", "MinGold", 0, path);
    config->min_gold = value < 0 ? 0 : value;
}
