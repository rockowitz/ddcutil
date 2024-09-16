/** @file ddc_watch_displays_main.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_MAIN_H_
#define DDC_WATCH_DISPLAYS_MAIN_H_

/** \cond */
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"

#include "util/error_info.h"
/** \endcond */

typedef enum {
   Watch_Mode_Full_Poll,
   Watch_Mode_Udev_Sysfs,
   Watch_Mode_Udev_I2C,
} DDC_Watch_Mode;

extern DDC_Watch_Mode ddc_watch_mode;

const char * ddc_watch_mode_name(DDC_Watch_Mode mode);
Error_Info * ddc_start_watch_displays(DDCA_Display_Event_Class event_classes);
DDCA_Status  ddc_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes);
DDCA_Status  ddc_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc);
void         init_ddc_watch_displays_main();

#endif /* DDC_WATCH_DISPLAYS_MAIN_H_ */
