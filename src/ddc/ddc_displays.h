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

Display_Info_List *
ddc_get_valid_displays_old();

Display_Info_List *
ddc_get_valid_displays();

GPtrArray * ddc_get_all_displays();

int
ddc_report_active_displays(int depth);

int
ddc_report_all_displays(int depth);

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

Display_Ref* ddc_find_display_by_edid(
   const Byte *  pEdidBytes,
   Byte          findopts);



#define DISPLAY_REC_MARKER "DREC"
/** Describes a single monitor detected.
 *
 * @remark
 * To facilitate conversion, this struct contains redundant information
 * from multiple existing data structures.
 */
typedef struct {
   char          marker[4];
   int           dispno;
   Display_Ref * dref;
   Parsed_Edid * edid;     // redundant, in detail
   DDCA_IO_Mode io_mode;   // redundant, also in Display_Ref
#ifdef OLD
   union {
      Bus_Info * bus_detail;
      ADL_Display_Detail * adl_detail;
#ifdef USE_USB
      Usb_Monitor_Info * usb_detail;
#endif
   } detail;
#endif
   void * detail2;
   // uint8_t     flags;            // currently unneeded

} Display_Rec;

void report_display_rec(Display_Rec * drec, int depth);

GPtrArray * ddc_detect_all_displays();

void ddc_ensure_displays_initialized();

#endif /* DDC_DISPLAYS_H_ */
