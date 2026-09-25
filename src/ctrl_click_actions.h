#ifndef JMOD_CTRL_CLICK_ACTIONS_H
#define JMOD_CTRL_CLICK_ACTIONS_H

#include <windows.h>
#include "config.h"

#define CTRL_CLICK_ACTIONS_POLL_INTERVAL_MS 20

int ctrl_click_actions_init(const D2ModConfig *options);
int ctrl_click_actions_on_message(MSG *msg, HWND game_window);
void ctrl_click_actions_poll(HWND game_window, int hooked);
void ctrl_click_actions_reset(void);
int ctrl_click_actions_pending(void);

#endif
