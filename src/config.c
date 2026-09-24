#include "config.h"
#include <string.h>

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
}
