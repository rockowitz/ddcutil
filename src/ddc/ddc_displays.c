/* ddc_displays.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "base/linux_errno.h"
#include "base/parms.h"

#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

#include "usb/usb_core.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include "ddc/ddc_displays.h"


// Trace class for this file
// static Trace_Group TRACE_GROUP = TRC_DDC;   // currently unused

// forward references
Display_Ref * ddc_find_display_by_usb_busnum_devnum(int   busnum, int   devnum);


//
//  Display Specification
//

/* Tests if a Display_Ref identifies an attached display.
 *
 * Arguments:
 *    dref     display reference
 *    emit_error_msg emit error message if not valid
 *
 * Returns:
 *    true if dref identifies a valid Display_Ref, false if not
 */
bool ddc_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   bool debug = false;
   assert( dref );
   // char buf[100];
   // DBGMSG("Starting.  %s   ", displayRefShortName(pdisp, buf, 100) );
   bool result = false;
   switch(dref->io_mode) {
   case DDC_IO_DEVI2C:
      result = i2c_is_valid_bus(dref->busno, emit_error_msg );
      break;
   case DDC_IO_ADL:
      result = adlshim_is_valid_display_ref(dref, emit_error_msg);
      break;
   case USB_IO:
      result = usb_is_valid_display_ref(dref, emit_error_msg);

   }
   DBGMSF(debug, "Returning %s", bool_repr(result));
   return result;
}


/* Converts display identifier passed on the command line to a logical
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
      dref = ddc_find_display_by_model_and_sn(pdid->model_name, pdid->serial_ascii);
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
   case DISP_ID_USB:
      dref = ddc_find_display_by_usb_busnum_devnum(pdid->usb_bus, pdid->usb_device);
       if (!dref && emit_error_msg) {
          fprintf(stderr, "Unable to find monitor with the specified USB bus and device numbers\n");
       }
       break;
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


#ifdef REDUNDANT
void report_display_info(Display_Info * dinfo, int depth) {
   const int d1 = depth+1;
   const int d2 = depth+2;
   rpt_structure_loc("Display_Info", dinfo, depth);
   rpt_int("dispno", NULL, dinfo->dispno, d1);
   rpt_vstring(d1, "dref: %p",   dinfo->dref);
   if (dinfo->dref)
      report_display_ref(dinfo->dref, d2);
   rpt_vstring(d1, "edid: %p",   dinfo->edid);
   if (dinfo->edid)
      report_parsed_edid(dinfo->edid, false /* verbose */,  d2);
}
#endif


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
   bool debug = false;
   int ndx;

   Display_Info_List i2c_displays = i2c_get_valid_displays();
   if (debug) {
      DBGMSG("i2c_displays returned from i2c_get_valid_displays():");
      report_display_info_list(&i2c_displays,1);
   }

   Display_Info_List adl_displays = adlshim_get_valid_displays();

   Display_Info_List usb_displays = usb_get_valid_displays();
   if (debug) {
      DBGMSG("usb_displays returned from usb_get_valid_displays():");
      report_display_info_list(&usb_displays,1);
   }

   // merge the lists
   int displayct = i2c_displays.ct + adl_displays.ct + usb_displays.ct;
   // DBGMSG("displayct=%d", displayct);
   Display_Info_List * all_displays = calloc(1, sizeof(Display_Info_List));
   all_displays->info_recs = calloc(displayct, sizeof(Display_Info));
   all_displays->ct = displayct;
   // DBGMSG("dest addr = %p", all_displays->info_recs );
   // DBGMSG("copying %d bytes", i2c_displays.ct * sizeof(Display_Info));
   memcpy(all_displays->info_recs,
          i2c_displays.info_recs,
          i2c_displays.ct * sizeof(Display_Info));
   // DBGMSG("dest addr = %p", all_displays->info_recs + (i2c_displays.ct)*sizeof(Display_Info));
   // DBGMSG("dest addr = %p", (Byte *) all_displays->info_recs + (i2c_displays.ct)*sizeof(Display_Info));
   // DBGMSG("copying %d bytes",adl_displays.ct * sizeof(Display_Info));

   //works:
   // memcpy((Byte *)all_displays->info_recs + i2c_displays.ct*sizeof(Display_Info),
   //        adl_displays.info_recs,
   //        adl_displays.ct * sizeof(Display_Info));

   // cleaner:
   memcpy(all_displays->info_recs + i2c_displays.ct,
          adl_displays.info_recs,
          adl_displays.ct * sizeof(Display_Info));

   // DBGMSG("dest addr   = %p", (Byte *)all_displays->info_recs + (i2c_displays.ct+adl_displays.ct)*sizeof(Display_Info));
   // DBGMSG("source addr = %p", usb_displays.info_recs);
   // DBGMSG("copying %d bytes", usb_displays.ct * sizeof(Display_Info));

   // works
   // memcpy((Byte *)all_displays->info_recs + (i2c_displays.ct+adl_displays.ct)*sizeof(Display_Info),
   //        usb_displays.info_recs,
   //        usb_displays.ct * sizeof(Display_Info));

   memcpy(all_displays->info_recs + (i2c_displays.ct+adl_displays.ct),
          usb_displays.info_recs,
          usb_displays.ct * sizeof(Display_Info));

   if (i2c_displays.info_recs)
      free(i2c_displays.info_recs);
   if (adl_displays.info_recs)
      free(adl_displays.info_recs);
   if (usb_displays.info_recs)
      free(usb_displays.info_recs);
   // rpt_title("merged list:", 0);
   int displayctr = 1;
   for (ndx = 0; ndx < displayct; ndx++) {
      // report_display_info(&all_displays->info_recs[ndx],1);
      if (ddc_is_valid_display_ref(all_displays->info_recs[ndx].dref, false /* emit msgs */)) {
         all_displays->info_recs[ndx].dispno = displayctr++;  // displays are numbered from 1, not 0
      }
      else {
         // Do not assign display number in case of I2C bus entry that isn't in fact a display
         // that supports DDC
         all_displays->info_recs[ndx].dispno = -1;
      }
   }

   if (debug) {
      DBGMSG("Returning merged list:");
      report_display_info_list(all_displays, 1);
   }
   return all_displays;
}


/* Returns a Display_Ref for the nth display.
 *
 * Arguments:
 *    dispno     display number
 *
 * Returns:
 *    Display_Ref for the dispno'th display,
 *    NULL if dispno < 1 or dispno > number of actual displays
 */
Display_Ref* ddc_find_display_by_dispno(int dispno) {
   bool debug = false;
   DBGMSF(debug, "Starting.  dispno=%d", dispno);

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


/* Returns a Display_Ref for a display identified by its model name and serial number.
 *
 * Arguments:
 *    model    model name
 *    sn       serial number (character string)
 *
 * Returns:
 *    Display_Ref for the specified monitor
 *    NULL if not found
 */
Display_Ref*
ddc_find_display_by_model_and_sn(
   const char * model,
   const char * sn)
{
   // DBGMSG("Starting.  model=%s, sn=%s   ", model, sn );
   // printf("(%s) WARNING: Support for USB devices unimplemented\n", __func__);

   Display_Ref * result = NULL;
   Bus_Info * businfo = i2c_find_bus_info_by_model_sn(model, sn);
   if (businfo) {
      result = create_bus_display_ref(businfo->busno);
   }

   if (!result)
      result = adlshim_find_display_by_model_sn(model, sn);

   if (!result)
      result = usb_find_display_by_model_sn(model, sn);

   // DBGMSG("Returning: %p  ", result );
   return result;
}

Display_Ref *
ddc_find_display_by_usb_busnum_devnum(
   int   busnum,
   int   devnum)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  busnum=%d, devnum=%d", busnum, devnum);
   // printf("(%s) WARNING: Support for USB devices unimplemented\n", __func__);

   Display_Ref * result = usb_find_display_by_busnum_devnum(busnum, devnum);

   DBGMSF(debug, "Returning: %p  ", result );
   return result;
}


/* Returns a Display_Ref for a display identified by its EDID
 *
 * Arguments:
 *    edid     pointer to 128 byte edid
 *
 * Returns:
 *    Display_Ref for the specified monitor
 *    NULL if not found
 */
Display_Ref*
ddc_find_display_by_edid(const Byte * pEdidBytes) {
   // DBGMSG("Starting.  model=%s, sn=%s   ", model, sn );
   Display_Ref * result = NULL;
   Bus_Info * businfo = i2c_find_bus_info_by_edid((pEdidBytes));
   if (businfo) {
      result = create_bus_display_ref(businfo->busno);
   }

   if (!result)
      result = adlshim_find_display_by_edid(pEdidBytes);

   if (!result)
      // printf("(%s) WARNING: Support for USB edid unimplemented\n", __func__);
      result = usb_find_display_by_edid(pEdidBytes);

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
void
ddc_report_active_display(Display_Info * curinfo, int depth) {
   switch(curinfo->dref->io_mode) {
   case DDC_IO_DEVI2C:
      i2c_report_active_display_by_busno(curinfo->dref->busno, depth);
      break;
   case DDC_IO_ADL:
      adlshim_report_active_display_by_display_ref(curinfo->dref, depth);
      break;
   case USB_IO:
      // printf("(%s) Case USB_IO unimplemented\n", __func__);
      usb_report_active_display_by_display_ref(curinfo->dref, depth);
   }


   Output_Level output_level = get_output_level();
   if (output_level >= OL_NORMAL  && ddc_is_valid_display_ref(curinfo->dref, false)) {
      Display_Handle * dh = ddc_open_display(curinfo->dref, EXIT_IF_FAILURE);
          // char * short_name = dref_short_name(curinfo->dref);
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
         char mfg_name_buf[100];
         char * mfg_name         = "Unspecified";
         // char * firmware_version = "Unspecified";
         // old way: Parsed_Nontable_Vcp_Response* code_info;
         /* works only for non-USB
         Global_Status_Code gsc = get_nontable_vcp_value(
                dh,
                0xc8,         // controller manufacturer
                &code_info);
         */
         // bump it up to get_nontable_vcp_value()'s caller, which does know how to handle USB
         Single_Vcp_Value *   valrec;
         Global_Status_Code  gsc = get_vcp_value(dh, 0xc8, NON_TABLE_VCP_VALUE, &valrec);

         if (gsc != 0) {
            if (gsc != DDCRC_REPORTED_UNSUPPORTED)
                DBGMSG("get_nontable_vcp_value(0xc8) returned %s", gsc_desc(gsc));
            rpt_vstring(depth, "Controller mfg:      Unspecified");
         }
         else {
            Feature_Value_Entry * vals = pxc8_display_controller_type_values;
            mfg_name =  get_feature_value_name(
                                  vals,
                                  valrec->val.nc.sl);
 //                               code_info->sl);
            if (!mfg_name) {
               // vsnprintf(mfg_name_buf, 100, "Unrecognized manufacturer code 0x%02x", code_info->sl);
               rpt_vstring(depth, "Controller mfg:       Unrecognized manufacturer code 0x%02x",
                                  valrec->val.nc.sl);
                         //       code_info->sl);
               mfg_name = mfg_name_buf;
            }
            else {
               // rpt_vstring(depth, "Controller mfg:      %s", (mfg_name) ? mfg_name : "not set");
               rpt_vstring(depth,    "Controller mfg:      %s", mfg_name);
            }
         }
#ifdef OLD
         gsc = get_nontable_vcp_value(
                     dh,
                     0xc9,         // firmware version
                     &code_info);
#endif
         gsc = get_vcp_value(dh, 0xc8, NON_TABLE_VCP_VALUE, &valrec);  // new way
         if (gsc != 0) {
            if (gsc != DDCRC_REPORTED_UNSUPPORTED)
               DBGMSG("get_nontable_vcp_value(0xc9) returned %s", gsc_desc(gsc));
            rpt_vstring(depth, "Firmware version:    Unspecified");
         }
         else if (gsc == 0) {
            rpt_vstring(depth, "Firmware version:    %d.%d",
                  // code_info->sh, code_info->sl);
                  valrec->val.nc.sh, valrec->val.nc.sl);

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
int
ddc_report_active_displays(int depth) {
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
      rpt_title("",0);
   }
   if (valid_display_ct == 0)
      rpt_vstring(depth, "No active displays found");
   return valid_display_ct;
}

