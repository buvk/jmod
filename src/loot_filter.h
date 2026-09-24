#ifndef JMOD_LOOT_FILTER_H
#define JMOD_LOOT_FILTER_H

#include "config.h"

/* Install the 1.09b ground-label hook. Returns zero if the client does not match. */
int loot_filter_init(const D2ModConfig *options);
int loot_filter_show_item(const void *item);

#endif
