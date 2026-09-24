#ifndef ITEM_LABELS_H
#define ITEM_LABELS_H
#include <windows.h>
int item_labels_init(void);
/* Returns nonzero if the key event was consumed. */
int item_labels_on_message(MSG *msg, HWND game_window);
void item_labels_resync_keys(void);
#endif
