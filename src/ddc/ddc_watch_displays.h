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
#define DEFAULT_EXTRA_STABILIZE_SECS 6
extern int            extra_stabilize_seconds;

const char * ddc_watch_mode_name(DDC_Watch_Mode mode);
Error_Info * ddc_start_watch_displays(DDCA_Display_Event_Class event_classes);
DDCA_Status  ddc_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes);
DDCA_Status  ddc_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc);
bool         is_watch_thread_executing();
void         init_ddc_watch_displays();

#endif /* DDC_WATCH_DISPLAYS_H_ */
