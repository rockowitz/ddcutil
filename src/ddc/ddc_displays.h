/** @file ddc_displays.h
 *
 *  Access displays, whether DDC, ADL, or USB
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_DISPLAYS_H_
#define DDC_DDC_DISPLAYS_H_

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"

#include "i2c/i2c_bus_core.h"

#include "adl/adl_shim.h"

#include "usb/usb_displays.h"

void ddc_set_async_threshold(int threshold);

bool
initial_checks_by_dref(Display_Ref * dref);

GPtrArray *
ddc_get_all_displays();  // returns GPtrArray of Display_Ref instances, including invalid displays

void
ddc_report_display_by_dref(Display_Ref * dref, int depth);

int
ddc_report_displays(bool include_invalid_displays, int depth);

Display_Ref*
get_display_ref_for_display_identifier(
   Display_Identifier* pdid,
   Call_Options        callopts);

Display_Ref*
ddc_find_display_by_dispno(
   int           dispno);

Display_Ref*
ddc_find_display_by_mfg_model_sn(
   const char *  mfg_id,
   const char *  model,
   const char *  sn,
   Byte          findopts);

Display_Ref*
ddc_find_display_by_edid(
   const Byte *  pEdidBytes,
   Byte          findopts);

void
ddc_dbgrpt_display_ref(Display_Ref * drec, int depth);

// GPtrArray *
// ddc_detect_all_displays();

void
ddc_ensure_displays_detected();

bool
ddc_displays_already_detected();

DDCA_Status
ddc_enable_usb_display_detection(bool onoff);

bool
ddc_is_usb_display_detection_enabled();

void
init_ddc_displays();

#endif /* DDC_DISPLAYS_H_ */
