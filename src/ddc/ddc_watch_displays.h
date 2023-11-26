/** @file ddc_watch_displays.h  Watch for monitor addition and removal  */

// Copyright (C) 2019-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_H_
#define DDC_WATCH_DISPLAYS_H_

/** \cond */
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"
/** \endcond */

#ifdef OLD_HOTPLUG_VERSION

typedef enum {Changed_None    = 0,
              Changed_Added   = 1,
              Changed_Removed = 2,
              Changed_Both    = 3  // n. == Changed_Added | Changed_Removed
} Displays_Change_Type;

const char * displays_change_type_name(Displays_Change_Type change_type);

typedef void (*Display_Change_Handler)(
                 Displays_Change_Type change_type,
                 GPtrArray *          removed,
                 GPtrArray *          added);
#endif

DDCA_Status ddc_start_watch_displays(bool use_udev_if_possible);
DDCA_Status ddc_stop_watch_displays();

void init_ddc_watch_displays();

#endif /* DDC_WATCH_DISPLAYS_H_ */
