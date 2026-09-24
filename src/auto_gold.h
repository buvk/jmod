#ifndef AUTO_GOLD_H
#define AUTO_GOLD_H
#include <windows.h>
#include "config.h"
void auto_gold_init(const D2ModConfig *options);
void auto_gold_reset(void);
void auto_gold_poll(HWND game_window, int hooked);
/* Returns nonzero if the message was consumed. */
int auto_gold_on_message(MSG *msg, HWND game_window);
#endif
