/* app_getvcp.c
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

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
/** \endcond */

#include "util/string_util.h"
#include "util/report_util.h"

#ifdef USE_USB
#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
#endif

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/sleep.h"
#include "base/vcp_version.h"

#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "app_ddcutil/app_getvcp.h"


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
Public_Status_Code
app_show_single_vcp_value_by_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  entry)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting feature 0x%02x for %s",
                 entry->code, display_handle_repr(dh) );

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
   Public_Status_Code     psc        = 0;
   Byte                   feature_id = entry->code;

   if (!is_feature_readable_by_vcp_version(entry, vspec)) {
      char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
      DDCA_Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vspec);
      if (vflags & DDCA_DEPRECATED)
         printf("Feature %02x (%s) is deprecated in MCCS %d.%d\n",
                feature_id, feature_name, vspec.major, vspec.minor);
      else
         printf("Feature %02x (%s) is not readable\n", feature_id, feature_name);
      psc = DDCL_INVALID_OPERATION;
   }

   if (psc == 0) {
      char * formatted_value = NULL;
      psc = get_formatted_value_for_feature_table_entry(
               dh,
               entry,
               false,      /* suppress_unsupported */
               true,       /* prefix_value_with_feature_code */
               &formatted_value,
               stdout);    /* msg_fh */
      if (formatted_value) {
         printf("%s\n", formatted_value);
         free(formatted_value);
      }
   }

   DBGMSF(debug, "Done.  Returning: %s", psc_desc(psc));
   return psc;
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
Public_Status_Code
app_show_single_vcp_value_by_feature_id(
      Display_Handle * dh,
      Byte feature_id,
      bool force)
{
   bool debug = false;
   DBGMSF(debug, "Starting. Getting feature 0x%02x for %s, force=%s",
                 feature_id, display_handle_repr(dh), bool_repr(force) );

   Public_Status_Code         psc = 0;
   VCP_Feature_Table_Entry *  entry = NULL;

   entry = vcp_find_feature_by_hexid(feature_id);
   if (!entry && (force || feature_id >= 0xe0)) {    // don't require force if manufacturer specific code
      entry = vcp_create_dummy_feature_for_hexid(feature_id);
   }
   if (!entry) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      // gsc = modulate_rc(-EINVAL, RR_ERRNO);
      psc = DDCL_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_feature_table_entry(dh, entry);
   }

   DBGMSF(debug, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh                display handle
 *    subset_id         feature subset
 *    show_unsupported  report unsupported values
 *    features_seen     if non-null, collect list of features found
 *
 * Returns:
 *    status code       from show_vcp_values()
 */
Public_Status_Code
app_show_vcp_subset_values_by_display_handle(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset_id,
        bool                show_unsupported,
        Byte_Bit_Flags      features_seen)
{
   // DBGMSG("Starting.  subset=%d   ", subset );

   GPtrArray * collector = NULL;
   Public_Status_Code psc = show_vcp_values(dh, subset_id, collector, show_unsupported, features_seen);
   return psc;
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


/* Shows the VCP values for all features indicated by a Feature_Set_Ref
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
Public_Status_Code
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

   Public_Status_Code psc = 0;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      psc = app_show_single_vcp_value_by_feature_id(
            dh, fsref->specific_feature, force);
   }
   else {
      psc = app_show_vcp_subset_values_by_display_handle(
            dh,
            fsref->subset,
            show_unsupported,
            NULL);
   }
   return psc;
}


/* Checks for VCP feature changes by:
 *   - reading feature x02 to check if changes exist,
 *   - querying feature x52 for the id of a changed feature
 *   - reading the value of the changed feature.
 *
 * If the VCP version is 2.1 or less a single feature is
 * read from x52.  For VCP version 3.0 and 2.2, x52 is a
 * FIFO is a queue of changed features.
 *
 * Finally, 1 is written to feature x02 as a reset.
 *
 * Arguments:
 *    dh      display handle
 */
void
app_read_changes(Display_Handle * dh) {
   bool debug = false;
   // DBGMSF(debug, "Starting");
   int MAX_CHANGES = 20;
   // bool new_values_found = false;

   Public_Status_Code psc = 0;

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


   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   psc = get_nontable_vcp_value(
            dh,
            0x02,
       //     false,  // retry_null_response
            &p_nontable_response);
   if (psc != 0) {
      DBGMSG("get_nontable_vcp_value() returned %s", psc_desc(psc));
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
         psc = get_nontable_vcp_value(
                  dh,
                  0x52,
             //     false,   // retry_null_response  ??
                  &p_nontable_response);
         if (psc != 0) {
             DBGMSG("get_nontable_vcp_value() returned %s", psc_desc(psc));
             return;
          }
          Byte changed_feature = p_nontable_response->sl;
          app_show_single_vcp_value_by_feature_id(dh, changed_feature, false);
      }
      else {  // x52 is a FIFO
         int ctr = 0;
         for (;ctr < MAX_CHANGES; ctr++) {
            psc = get_nontable_vcp_value(
                     dh,
                     0x52,
               //      false,     // retry_null_response ???
                     &p_nontable_response);
            if (psc != 0) {
                DBGMSG("get_nontable_vcp_value() returned %s", psc_desc(psc));
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

      if (psc == 0) {
         psc = set_nontable_vcp_value(dh, 0x02, 0x01);
         if (psc != 0)
            DBGMSG("set_nontable_vcp_value_by_display_handle() returned %s", psc_desc(psc));
         else
            DBGMSG("reset new control value successful");
      }
   }

   if (p_nontable_response) {
      free(p_nontable_response);
      p_nontable_response = NULL;
   }
}


#ifdef USE_USB
void
app_read_changes_usb(Display_Handle * dh) {
   bool debug = true;
   DBGMSF(debug, "Starting");
   // bool new_values_found = false;

   assert(dh->dref->io_mode == DDCA_IO_USB);
   int fd = dh->fh;
   int flaguref = HIDDEV_FLAG_UREF;
   struct hiddev_usage_ref uref;
   int rc = ioctl(fd, HIDIOCSFLAG, &flaguref);
   if (rc < 0) {
      REPORT_IOCTL_ERROR("HIDIOCSFLAG", rc);
      return;
   }

   ssize_t ct = read(fd, &uref, sizeof(uref));
   if (ct < 0) {
      int errsv = errno;
      // report the error
      printf("(%s) read failed, errno=%d\n", __func__, errsv);
   }
   else if (ct > 0) {
      rpt_vstring(1, "Read new value:");
      if (ct < sizeof(uref)) {
         rpt_vstring(1, "Short read");
      }
      else {
         report_hiddev_usage_ref(&uref, 1);
         rpt_vstring(1, "New value: 0x%04x (%d)", uref.value, uref.value);
      }

   }
   else {
      DBGMSF(debug, "tick");
   }
}
#endif



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
#ifdef USE_USB
      if (dh->dref->io_mode == DDCA_IO_USB)
         app_read_changes_usb(dh);
      else
#endif
         app_read_changes(dh);

      sleep_millis( 2500);
   }
}
