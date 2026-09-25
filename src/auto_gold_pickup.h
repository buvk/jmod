#ifndef AUTO_GOLD_PICKUP_H
#define AUTO_GOLD_PICKUP_H
#include <windows.h>
#include "config.h"

#define AUTO_GOLD_PICKUP_SCAN_INTERVAL_MS 40
#define AUTO_GOLD_PICKUP_REQUEST_INTERVAL_MS 40
#define AUTO_GOLD_PICKUP_RETRY_INTERVAL_MS 200
void auto_gold_pickup_init(const D2ModConfig *options);
void auto_gold_pickup_reset(void);
void auto_gold_pickup_poll(HWND game_window, int hooked);
/* Returns nonzero if the message was consumed. */
int auto_gold_pickup_on_message(MSG *msg, HWND game_window);
#endif
