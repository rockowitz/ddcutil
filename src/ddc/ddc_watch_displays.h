/** @file ddc_watch_displays.h  Watch for monitor addition and removal  */

// Copyright (C) 2019-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_WATCH_DISPLAYS_H_
#define DDC_WATCH_DISPLAYS_H_

/** \cond */
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"
/** \endcond */


typedef void (*Display_Change_Handler)(
                 GPtrArray *          buses_removed,
                 GPtrArray *          buses_added,
                 GPtrArray *          connectors_removed,
                 GPtrArray *          connectors_added);

typedef enum {
   Watch_Mode_Full_Poll,
   Watch_Mode_Simple_Udev,
} DDC_Watch_Mode;

extern DDC_Watch_Mode ddc_watch_mode;
extern bool           ddc_slow_watch;

const char * ddc_watch_mode_name(DDC_Watch_Mode mode);
DDCA_Status  ddc_start_watch_displays();
DDCA_Status  ddc_stop_watch_displays(bool wait);
DDCA_Status  ddc_register_display_sleep_event_callback(DDCA_Display_Sleep_Evemt_Callback_Func func);
DDCA_Status  ddc_unregister_display_sleep_event_callback(DDCA_Display_Sleep_Evemt_Callback_Func func);
void         init_ddc_watch_displays();

#endif /* DDC_WATCH_DISPLAYS_H_ */
