#ifndef JMOD_INVENTORY_QOL_H
#define JMOD_INVENTORY_QOL_H

#include <windows.h>
#include "config.h"

#define INVENTORY_QOL_POLL_INTERVAL_MS 20

int inventory_qol_init(const D2ModConfig *options);
int inventory_qol_on_message(MSG *msg, HWND game_window);
void inventory_qol_poll(HWND game_window, int hooked);
void inventory_qol_reset(void);
int inventory_qol_pending(void);

#endif
