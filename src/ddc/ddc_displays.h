/** @file ddc_displays.h
 *
 *  Access displays, whether DDC or USB
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAYS_H_
#define DDC_DISPLAYS_H_

#include "public/ddcutil_types.h"

#include "config.h"

#include "base/core.h"
#include "base/displays.h"

#include "i2c/i2c_bus_core.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

extern bool check_phantom_displays;

void ddc_set_async_threshold(int threshold);

bool
ddc_initial_checks_by_dref(Display_Ref * dref);

GPtrArray *
ddc_get_all_displays();  // returns GPtrArray of Display_Ref instances, including invalid displays

GPtrArray *
ddc_get_filtered_displays(bool include_invalid_displays);

void
ddc_report_display_by_dref(Display_Ref * dref, int depth);

int
ddc_get_display_count(bool include_invalid_displays);

int
ddc_report_displays(bool include_invalid_displays, int depth);

Display_Ref*
get_display_ref_for_display_identifier(
   Display_Identifier* pdid,
   Call_Options        callopts);

void
ddc_dbgrpt_display_ref(Display_Ref * drec, int depth);

void
ddc_dbgrpt_display_refs(GPtrArray * recs, int depth);

// GPtrArray *
// ddc_detect_all_displays();

void
ddc_ensure_displays_detected();

void
ddc_discard_detected_displays();

void
ddc_redetect_displays();

bool
ddc_is_valid_display_ref(Display_Ref * dref);

void
dbgrpt_dref_ptr_array(char * msg, GPtrArray* ptrarray, int depth);

void
dbgrpt_valid_display_refs(int depth);


bool
ddc_displays_already_detected();

DDCA_Status
ddc_enable_usb_display_detection(bool onoff);

bool
ddc_is_usb_display_detection_enabled();

void
init_ddc_displays();

#endif /* DDC_DISPLAYS_H_ */
