/* ddc_displays.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \file
 *
 */

#ifndef DDC_DISPLAYS_H_
#define DDC_DDC_DISPLAYS_H_

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"

#include "i2c/i2c_bus_core.h"

#include "adl/adl_shim.h"

#include "usb/usb_displays.h"

void ddc_set_async_threshold(int threshold);

// bool initial_checks_by_dh(Display_Handle * dh);   not used externally
bool
initial_checks_by_dref(Display_Ref * dref);

GPtrArray *
ddc_get_all_displays();  // returns GPtrArray of Display_Ref instances, including invalid displays

int
ddc_report_active_displays(int depth);

#define DDC_REPORT_ALL_DISPLAYS false
#define DDC_REPORT_VALID_DISPLAYS_ONLY true
int
ddc_report_displays(bool valid_only, int depth);

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
dbgreport_display_ref(Display_Ref * drec, int depth);

GPtrArray *
ddc_detect_all_displays();

void
ddc_ensure_displays_detected();

#endif /* DDC_DISPLAYS_H_ */
