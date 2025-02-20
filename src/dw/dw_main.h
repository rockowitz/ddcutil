/** @file dw_main.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_MAIN_H_
#define DW_MAIN_H_

/** \cond */
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"

#include "util/error_info.h"
/** \endcond */

extern DDC_Watch_Mode watch_displays_mode;
extern bool            enable_watch_displays;

Error_Info * dw_start_watch_displays(DDCA_Display_Event_Class event_classes);
DDCA_Status  dw_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes);
DDCA_Status  dw_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc);
void         dw_redetect_displays();
bool         dw_is_watch_displays_executing();
void         dw_get_display_watch_settings(DDCA_DW_Settings * settings_buffer);
DDCA_Status  dw_set_display_watch_settings(DDCA_DW_Settings * settings_buffer);
void         init_dw_main();

#endif /* DW_MAIN_H_ */
