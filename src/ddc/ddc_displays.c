/* ddc_displays.c
 *
 * Created on: Dec 28, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>

#include "util/report_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/linux_errno.h"
#include "base/msg_control.h"
#include "base/parms.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

// #include "ddc/ddc_edid.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"
#include "ddc/vcp_feature_codes.h"
#include "ddc/ddc_packet_io.h"                // TODO: CHECK IF CIRCULAR DEPENDENCY

#include "ddc/ddc_displays.h"


// Trace class for this file
// static Trace_Group TRACE_GROUP = TRC_DDC;   // currently unused


//
//  Display Specification
//

/** Tests if a DisplayRef identifies an attached display.
 */
bool ddc_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   assert( dref );
   // char buf[100];
   // DBGMSG("Starting.  %s   ", displayRefShortName(pdisp, buf, 100) );
   bool result;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      result = i2c_is_valid_bus(dref->busno, emit_error_msg );
   }
   else {
      // result = adl_is_valid_adlno(dref->iAdapterIndex, dref->iDisplayIndex, true /* emit_error_msg */);
      result = adlshim_is_valid_display_ref(dref, emit_error_msg);
   }
   // DBGMSG("Returning %d", result);
   return result;
}


/* Converts display identifiers passed on the command line to a logical
 * identifier for an I2C or ADL display.  If a bus number or ADL adapter.display
 * number is specified, the translation is direct.  If a model name/serial number
 * pair, an EDID, or a display number is specified, the attached displays are searched.
 *
 * Arguments:
 *    pdid      display identifiers
 *    emit_error_msg
 * OLD   validate  if searching was not necessary, validate that that bus number or
 * OLD             ADL number does in fact reference an attached display
 *
 * Returns:
 *    DisplayRef instance specifying the display using either an I2C bus number
 *    or an ADL adapter.display number, NULL if display not found
 */
Display_Ref* get_display_ref_for_display_identifier(Display_Identifier* pdid, bool emit_error_msg) {
   Display_Ref* dref = NULL;
   bool validated = true;

   switch (pdid->id_type) {
   case DISP_ID_DISPNO:
      dref = ddc_find_display_by_dispno(pdid->dispno);
      if (!dref && emit_error_msg) {
         fprintf(stderr, "Invalid display number\n");
      }
      validated = false;
      break;
   case DISP_ID_BUSNO:
      dref = create_bus_display_ref(pdid->busno);
      validated = false;
      break;
   case DISP_ID_ADL:
      dref = create_adl_display_ref(pdid->iAdapterIndex, pdid->iDisplayIndex);
      validated = false;
      break;
   case DISP_ID_MONSER:
      dref = ddc_find_display_by_model_and_sn(pdid->model_name, pdid->serial_ascii);  // in ddc_packet_io
      if (!dref && emit_error_msg) {
         fprintf(stderr, "Unable to find monitor with the specified model and serial number\n");
      }
      break;
   case DISP_ID_EDID:
      dref = ddc_find_display_by_edid(pdid->edidbytes);
      if (!dref && emit_error_msg) {
         fprintf(stderr, "Unable to find monitor with the specified EDID\n" );
      }
      break;
   // no default case because switch is exhaustive, compiler warns if case missing
   }  // switch

   if (dref) {
      if (!validated)      // DISP_ID_BUSNO or DISP_ID_ADL
        validated = ddc_is_valid_display_ref(dref, emit_error_msg);
      if (!validated) {
         free(dref);
         dref = NULL;
      }
   }

   // DBGMSG("Returning: %s", (pdref)?"non-null": "NULL" );
   return dref;
}


//
// Functions to get display information
//

/* Creates a list of all displays found.  The list first contains any displays
 * on /dev/i2c-n busses, then any ADL displays.
 *
 * The displays are assigned a display number (starting from 1) based on the
 * above order.
 *
 * Arguments: none
 *
 * Returns:
 *    Display_Info_list struct
 */
Display_Info_List * ddc_get_valid_displays() {
   int ndx;

   Display_Info_List i2c_displays = i2c_get_valid_displays();
   Display_Info_List adl_displays = adlshim_get_valid_displays();

   // merge the lists
   int displayct = i2c_displays.ct + adl_displays.ct;
   Display_Info_List * all_displays = calloc(1, sizeof(Display_Info_List));
   all_displays->info_recs = calloc(displayct, sizeof(Display_Info));
   all_displays->ct = displayct;
   memcpy(all_displays->info_recs,
          i2c_displays.info_recs,
          i2c_displays.ct * sizeof(Display_Info));
   memcpy(all_displays->info_recs + i2c_displays.ct*sizeof(Display_Info),
          adl_displays.info_recs,
          adl_displays.ct * sizeof(Display_Info));
   if (i2c_displays.info_recs)
      free(i2c_displays.info_recs);
   if (adl_displays.info_recs)
      free(adl_displays.info_recs);
   int displayctr = 0;
   for (ndx = 0; ndx < displayct; ndx++) {
      if (ddc_is_valid_display_ref(all_displays->info_recs[ndx].dref, false /* emit msgs */)) {
         displayctr++;
         all_displays->info_recs[ndx].dispno = displayctr;  // displays are numbered from 1, not 0
      }
      else {
         // Do not assign display number in case of I2C bus entry that isn't in fact a display
         // that supports DDC
         all_displays->info_recs[ndx].dispno = -1;
      }
   }

   // DBGMSG("all_displays in main.c:");
   // report_display_info_list(all_displays, 0);
   return all_displays;
}


/* Returns a Display_Ref for the nth display.
 *
 * Arguments:
 *    dispno     display number
 *
 * Returns:
 *    Display_Ref for the dispno'th display, NULL if
 *    dispno < 1 or dispno > number of actual displays
 */
Display_Ref* ddc_find_display_by_dispno(int dispno) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting.  dispno=%d", dispno);
   Display_Ref * result = NULL;
   Display_Info_List * all_displays = ddc_get_valid_displays();
   if (dispno >= 1 && dispno <= all_displays->ct) {
      // we're not done yet.   There may be an invalid display in the list.
      int ndx;
      for (ndx=0; ndx<all_displays->ct; ndx++) {
         if (all_displays->info_recs[ndx].dispno == dispno) {
            result = all_displays->info_recs[ndx].dref;
            break;
         }
      }
   }
   if (debug) {
      DBGMSG("Returning: %p  ", result );
      if (result)
         report_display_ref(result, 0);
   }
   return result;
}


Display_Ref* ddc_find_display_by_model_and_sn(const char * model, const char * sn) {
   // DBGMSG("Starting.  model=%s, sn=%s   ", model, sn );
   Display_Ref * result = NULL;
   Bus_Info * businfo = i2c_find_bus_info_by_model_sn(model, sn);
   if (businfo) {
      result = create_bus_display_ref(businfo->busno);
   }
   else {
      result = adlshim_find_display_by_model_sn(model, sn);
   }
   // DBGMSG("Returning: %p  ", result );
   return result;
}


Display_Ref* ddc_find_display_by_edid(const Byte * pEdidBytes) {
   // DBGMSG("Starting.  model=%s, sn=%s   ", model, sn );
   Display_Ref * result = NULL;
   Bus_Info * businfo = i2c_find_bus_info_by_edid((pEdidBytes));
   if (businfo) {
      result = create_bus_display_ref(businfo->busno);
   }
   else {
      result = adlshim_find_display_by_edid(pEdidBytes);
   }
   // DBGMSG("Returning: %p  ", result );
   return result;
}


/* Show information about a display.
 *
 * Output is written using report functions
 *
 * Arguments:
 *    curinfo   pointer to display information
 *    depth     logical indentation depth
 */
void ddc_report_active_display(Display_Info * curinfo, int depth) {
   if (curinfo->dref->ddc_io_mode == DDC_IO_DEVI2C)
      i2c_report_active_display_by_busno(curinfo->dref->busno, depth);
   else {
      adlshim_report_active_display_by_display_ref(curinfo->dref, depth);
   }

   Output_Level output_level = get_output_level();
   if (output_level >= OL_NORMAL  && ddc_is_valid_display_ref(curinfo->dref, false)) {
      Display_Handle * dh = ddc_open_display(curinfo->dref, EXIT_IF_FAILURE);
      // char * short_name = display_ref_short_name(curinfo->dref);
          // printf("Display:       %s\n", short_name);
          // works, but TMI
          // printf("Mfg:           %s\n", cur_info->edid->mfg_id);
      // don't want debugging  output if OL_VERBOSE
      if (output_level >= OL_VERBOSE)
         set_output_level(OL_NORMAL);

      Version_Spec vspec = get_vcp_version_by_display_handle(dh);

      // printf("VCP version:   %d.%d\n", vspec.major, vspec.minor);
      if (vspec.major == 0)
         rpt_vstring(depth, "VCP version: detection failed");
      else
         rpt_vstring(depth, "VCP version:         %d.%d", vspec.major, vspec.minor);

      if (output_level >= OL_VERBOSE) {
         // display controller mfg, firmware version
         Parsed_Nontable_Vcp_Response* code_info;

         Global_Status_Code gsc = get_nontable_vcp_value(
                dh,
                0xc8,         // controller manufacturer
                &code_info);
         if (gsc != 0) {
            DBGMSG("get_vcp_by_display_ref() returned %s", gsc_desc(gsc));
         }
         else {
            Feature_Value_Entry * vals = pxc8_display_controller_type_values;
            char * mfg_name =  get_feature_value_name(
                                  vals,
                                  code_info->sl);
            rpt_vstring(depth, "Controller mfg:      %s", (mfg_name) ? mfg_name : "not set");
            if (mfg_name) {
               Global_Status_Code gsc = get_nontable_vcp_value(
                        dh,
                        0xc9,         // firmware version
                        &code_info);
               if (gsc != 0) {
                  DBGMSG("get_vcp_by_display_ref() returned %s", gsc_desc(gsc));
               }
               else {
                  rpt_vstring(depth, "Firmware version:    %d.%d", code_info->sh, code_info->sl);
               }
            }
         }
      }
      ddc_close_display(dh);
      if (output_level >= OL_VERBOSE)
         set_output_level(output_level);
   }
}


/* Reports all displays found.
 *
 * Output is written to the current report destination using
 * report functions.
 *
 * Arguments:
 *    depth       logical indentation depth
 *
 * Returns:
 *    number of displays
 */
int ddc_report_active_displays(int depth) {
   Display_Info_List * display_list = ddc_get_valid_displays();
   int ndx;
   int valid_display_ct = 0;
   for (ndx=0; ndx<display_list->ct; ndx++) {
      Display_Info * curinfo = &display_list->info_recs[ndx];
      if (curinfo->dispno == -1)
         rpt_vstring(depth, "Invalid display");
      else {
         rpt_vstring(depth, "Display %d", curinfo->dispno);
         valid_display_ct++;
      }
      ddc_report_active_display(curinfo, depth+1);
      puts("");
   }
   if (valid_display_ct == 0)
      rpt_vstring(depth, "No active displays found");
   return valid_display_ct;
}




