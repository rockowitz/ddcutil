/** \file ddc_watch_displays.h - Watch for monitor addition and removal
 */

// Copyright (C) 2019-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_H_
#define DDC_WATCH_DISPLAYS_H_

/** \cond */
// for syscall
#define _GNU_SOURCE
#include <unistd.h>

#include <glib-2.0/glib.h>

#include "ddcutil_status_codes.h"

/** \endcond */

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

void dummy_display_change_handler(
        Displays_Change_Type change_type,
        GPtrArray *          removed,
        GPtrArray *          added);

DDCA_Status ddc_start_watch_displays();
DDCA_Status ddc_stop_watch_displays();
void init_ddc_watch_displays();

// GPtrArray *  get_sysfs_drm_displays();

#endif /* DDC_WATCH_DISPLAYS_H_ */
