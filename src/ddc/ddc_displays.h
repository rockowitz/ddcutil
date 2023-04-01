/** @file ddc_displays.h
 *
 *  Access displays, whether DDC or USB
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAYS_H_
#define DDC_DISPLAYS_H_

#include "public/ddcutil_types.h"

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "base/displays.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

// Initial Checks
void        ddc_set_async_threshold(int threshold);
bool        ddc_initial_checks_by_dref(Display_Ref * dref);

// Get Display Information
GPtrArray * ddc_get_all_displays();  // returns GPtrArray of Display_Ref instances, including invalid displays
GPtrArray * ddc_get_filtered_displays(bool include_invalid_displays);
GPtrArray * ddc_get_bus_open_errors();
int         ddc_get_display_count(bool include_invalid_displays);

// Display Detection
void        ddc_ensure_displays_detected();
void        ddc_discard_detected_displays();
void        ddc_redetect_displays();
bool        ddc_displays_already_detected();
DDCA_Status ddc_enable_usb_display_detection(bool onoff);
bool        ddc_is_usb_display_detection_enabled();
void        dbgrpt_bus_open_errors(GPtrArray * open_errors, int depth);
bool        ddc_is_valid_display_ref(Display_Ref * dref);

// Initialization
void        init_ddc_displays();

#endif /* DDC_DISPLAYS_H_ */
