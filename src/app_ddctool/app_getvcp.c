/* app_getvcp.c
 *
 * Created on: Jan 1, 2016
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

#ifndef SRC_APP_DDCTOOL_APP_GETVCP_C_
#define SRC_APP_DDCTOOL_APP_GETVCP_C_

#include <errno.h>
#include <stdio.h>

#include "util/string_util.h"

#include "base/common.h"
#include "base/ddc_errno.h"
#include "base/msg_control.h"
#include "base/status_code_mgt.h"
#include "base/msg_control.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/vcp_feature_codes.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_output.h"

#include "app_ddctool/app_getvcp.h"


/* Shows a single VCP value specified by its feature table entry.
 *
 * Arguments:
 *    dh          handle of open display
 *    entry       hex feature id
 *
 * Returns:
 *    status code 0 = normal
 *                DDCL_INVALID_OPERATION - feature is deprecated or write-only
 *                from get_formatted_value_for_feature_table_entry()
 */
Global_Status_Code
app_show_single_vcp_value_by_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  entry)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting feature 0x%02x for %s",
                 entry->code, display_handle_repr(dh) );

   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   Global_Status_Code         gsc = 0;
   Byte                       feature_id = entry->code;

   if (!is_feature_readable_by_vcp_version(entry, vspec)) {
      char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
      Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vspec);
      if (vflags & VCP2_DEPRECATED)
         printf("Feature %02x (%s) is deprecated in MCCS %d.%d\n",
                feature_id, feature_name, vspec.major, vspec.minor);
      else
         printf("Feature %02x (%s) is not readable\n", feature_id, feature_name);
      // gsc = modulate_rc(-EINVAL, RR_ERRNO);    // TEMP - what is appropriate?
      gsc = DDCL_INVALID_OPERATION;
   }

   if (gsc == 0) {
      char * formatted_value = NULL;
      gsc = get_formatted_value_for_feature_table_entry(
               dh,
               entry,
               false,      /* suppress_unsupported */
               true,       /* prefix_value_with_feature_code */
               &formatted_value,
               stdout);    /* msg_fh */
      if (formatted_value)
         printf("%s\n", formatted_value);
   }

   DBGMSF(debug, "Done.  Returning: %s", gsc_desc(gsc));
   return gsc;
}


/* Shows a single VCP value specified by its feature id.
 *
 * Arguments:
 *    dh          handle of open display
 *    feature_id  hex feature id
 *    force       attempt to show value even if feature_id not in feature table
 *
 * Returns:
 *    status code 0 = success
 *                DDCL_UNKNOWN_FEATURE  feature_id not in feature table and !force
 *                from app_show_single_vcp_value_by_feature_table_entry()
 */
Global_Status_Code
app_show_single_vcp_value_by_feature_id(
      Display_Handle * dh,
      Byte feature_id,
      bool force)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting feature 0x%02x for %s",
                 feature_id, display_handle_repr(dh) );

   Global_Status_Code         gsc = 0;
   VCP_Feature_Table_Entry *  entry = NULL;

   entry = vcp_find_feature_by_hexid(feature_id);
   if (!entry && force) {
      entry = vcp_create_dummy_feature_for_hexid(feature_id);
   }
   if (!entry) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      // gsc = modulate_rc(-EINVAL, RR_ERRNO);
      gsc = DDCL_UNKNOWN_FEATURE;
   }
   else {
      gsc = app_show_single_vcp_value_by_feature_table_entry(dh, entry);
   }

   DBGMSF(debug, "Done.  Returning: %s", gsc_desc(gsc));
   return gsc;
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh                display handle
 *    subset_id         feature subset
 *    show_unsupported  report unsupported values
 *
 * Returns:
 *    status code       from show_vcp_values()
 */
Global_Status_Code
app_show_vcp_subset_values_by_display_handle(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset_id,
        bool                show_unsupported)
{
   // DBGMSG("Starting.  subset=%d   ", subset );

   GPtrArray * collector = NULL;
   return show_vcp_values(dh, subset_id, collector, show_unsupported);
}


#ifdef UNUSED
/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    pdisp      display reference
 *    subset_id  feature subset
 *    collector  accumulates output
 *    show_unsupported
 *
 * Returns:
 *    nothing
 */
void app_show_vcp_subset_values_by_display_ref(
        Display_Ref *       dref,
        VCP_Feature_Subset  subset_id,
        bool                show_unsupported)
{
   // DBGMSG("Starting.  subset=%d   ", subset );
   // need to ensure that bus info initialized
   bool validDisp = true;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      // Is this needed?  or checked by openDisplay?
      Bus_Info * bus_info = i2c_get_bus_info(dref->busno);
      if (!bus_info ||  !(bus_info->flags & I2C_BUS_ADDR_0X37) ) {
         printf("Address 0x37 not detected on bus %d. I2C communication not available.\n", dref->busno );
         validDisp = false;
      }
   }
   else {
      validDisp = true;    // already checked
   }

   if (validDisp) {
      GPtrArray * collector = NULL;
      Display_Handle * pDispHandle = ddc_open_display(dref, EXIT_IF_FAILURE);
      show_vcp_values(pDispHandle, subset_id, collector, show_unsupported);
      ddc_close_display(pDispHandle);
   }
}
#endif


/* Shows the VCP values for all features indicated by a Feature_Set_ref
 *
 * Arguments:
 *    dh                display handle
 *    fsref             feature set reference
 *    show_unsupported  report unsupported values (applies if not a single feature feature set)
 *    force             applies if is a single feature feature set
 *
 * Returns:
 *    status code       from app_show_single_vcp_value_by_feature_id() or
 *                           app_show_subset_values_by_display_handle()
 */
Global_Status_Code
app_show_feature_set_values_by_display_handle(
      Display_Handle *     dh,
      Feature_Set_Ref *    fsref,
      bool                 show_unsupported,
      bool                 force)
{
   bool debug = false;
   if (debug) {
      DBGMSG("Starting");
      DBGMSG("dh: %s", display_handle_repr(dh) );
      report_feature_set_ref(fsref,1);
   }

   Global_Status_Code gsc = 0;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      gsc = app_show_single_vcp_value_by_feature_id(
            dh, fsref->specific_feature, force);
   }
   else {
      gsc = app_show_vcp_subset_values_by_display_handle(
            dh,
            fsref->subset,
            show_unsupported);
   }
   return gsc;
}




void
app_read_changes(Display_Handle * dh) {
   bool debug = false;
   // DBGMSF(debug, "Starting");
   int MAX_CHANGES = 20;
   // bool new_values_found = false;

   Global_Status_Code gsc = 0;

   // read 02h
   // xff: no user controls
   // x01: no new control values
   // x02: new control values exist

   /* Per the 3.0 and 2.2 specs, x52 is a FIFO to be read until x00 indicates empty
    * What apparently happens on 2.1 (U3011) is that each time x02 is reset with value x01
    * the subsequent read of x02 returns x02 (new control values exists) until the queue
    * of changes is flushed
    */

   Parsed_Nontable_Vcp_Response * p_nontable_response = NULL;


   Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   gsc = get_nontable_vcp_value(dh, 0x02, &p_nontable_response);
   if (gsc != 0) {
      DBGMSG("get_nontable_vcp_value() returned %s", gsc_desc(gsc));
   }
   else if (p_nontable_response->sl == 0x01) {
      DBGMSF(debug, "No new control values found");
   }
   else {
      DBGMSG("x02 value: 0x%02x", p_nontable_response->sl);
      free(p_nontable_response);
      p_nontable_response = NULL;

      // new_values_found = true;
      if ( vcp_version_le(vspec, VCP_SPEC_V21) ) {
         gsc = get_nontable_vcp_value(dh, 0x52, &p_nontable_response);
         if (gsc != 0) {
             DBGMSG("get_nontable_vcp_value() returned %s", gsc_desc(gsc));
             return;
          }
          Byte changed_feature = p_nontable_response->sl;
          app_show_single_vcp_value_by_feature_id(dh, changed_feature, false);
      }
      else {  // x52 is a FIFO
         int ctr = 0;
         for (;ctr < MAX_CHANGES; ctr++) {
            gsc = get_nontable_vcp_value(dh, 0x52, &p_nontable_response);
            if (gsc != 0) {
                DBGMSG("get_nontable_vcp_value() returned %s", gsc_desc(gsc));
                return;
             }
             Byte changed_feature = p_nontable_response->sl;
             free(p_nontable_response);
             p_nontable_response = NULL;
             if (changed_feature == 0x00) {
                DBGMSG("No more changed features found");
                break;
             }
             app_show_single_vcp_value_by_feature_id(dh, changed_feature, false);
         }
      }

      if (gsc == 0) {
         gsc = set_nontable_vcp_value(dh, 0x02, 0x01);
         if (gsc != 0)
            DBGMSG("set_nontable_vcp_value_by_display_handle() returned %s", gsc_desc(gsc));
         else
            DBGMSG("reset new control value successful");
      }
   }

   if (p_nontable_response) {
      free(p_nontable_response);
      p_nontable_response = NULL;
   }

}


/* Infinite loop watching for VCP feature changes reported by the display.
 *
 * Arguments:
 *    dh        display handle
 *
 * Returns:
 *    does not return - halts with program termination
 */
void
app_read_changes_forever(Display_Handle * dh) {
   printf("Watching for VCP feature changes on display %s\n", display_handle_repr(dh));
   printf("Type ^C to exit...\n");
   while(true) {
      app_read_changes(dh);

      sleep_millis( 2500);
   }
}

#endif /* SRC_APP_DDCTOOL_APP_GETVCP_C_ */
