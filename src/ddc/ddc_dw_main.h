/** @file ddc_dw_main.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DW_MAIN_H_
#define DDC_DW_MAIN_H_

/** \cond */
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"

#include "util/error_info.h"

#include "base/displays.h"
/** \endcond */

extern DDC_Watch_Mode ddc_watch_mode;
extern bool           enable_watch_displays;

Error_Info * ddc_start_watch_displays(DDCA_Display_Event_Class event_classes);
DDCA_Status  ddc_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes);
DDCA_Status  ddc_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc);
bool         is_watch_displays_executing();
void         init_ddc_watch_displays_main();

#endif /* DDC_DW_MAIN_H_ */
