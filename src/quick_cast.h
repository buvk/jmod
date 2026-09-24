#ifndef QUICK_CAST_H
#define QUICK_CAST_H
#include <windows.h>
void quick_cast_init(int enabled);
void quick_cast_release_all(void);
/* Returns nonzero if the message was consumed. */
int quick_cast_on_message(MSG *msg, HWND game_window);
#endif
