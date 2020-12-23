/** @file app_getvcp.c
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/error_info.h"
#include "util/string_util.h"
#include "util/report_util.h"
/** \endcond */

#ifdef USE_USB
#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
#endif

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/sleep.h"
#include "base/vcp_version.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_codes.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "app_ddcutil/app_getvcp.h"


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;

#ifdef OLD
/* Shows a single VCP value specified by its feature table entry.
 *
 * Arguments:
 *    dh          handle of open display
 *    entry       hex feature id
 *
 * Returns:
 *    status code 0 = normal
 *                DDCRC_INVALID_OPERATION - feature is deprecated or write-only
 *                from get_formatted_value_for_feature_table_entry()
 */
Public_Status_Code
app_show_single_vcp_value_by_feature_table_entry(
      Display_Handle *           dh,
      VCP_Feature_Table_Entry *  entry)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
         "Starting. Getting feature 0x%02x for %s", entry->code, dh_repr(dh) );

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
   Public_Status_Code     psc        = 0;
   DDCA_Vcp_Feature_Code  feature_id = entry->code;

   if (!is_feature_readable_by_vcp_version(entry, vspec)) {
      char * feature_name =  get_version_sensitive_feature_name(entry, vspec);
      DDCA_Version_Feature_Flags vflags = get_version_sensitive_feature_flags(entry, vspec);
      if (vflags & DDCA_DEPRECATED)
         printf("Feature %02x (%s) is deprecated in MCCS %d.%d\n",
                feature_id, feature_name, vspec.major, vspec.minor);
      else
         printf("Feature %02x (%s) is not readable\n", feature_id, feature_name);
      psc = DDCRC_INVALID_OPERATION;
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

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}
#endif


/* Shows a single VCP value specified by its #Display_Feature_Metadata
 *
 * Arguments:
 *    dh          handle of open display
 *    meta        feature metadata
 *
 * Returns:
 *    status code 0 = normal
 *                DDCRC_INVALID_OPERATION - feature is deprecated or write-only
 *                from get_formatted_value_for_feature_table_entry()
 */
DDCA_Status
app_show_single_vcp_value_by_dfm(
      Display_Handle *             dh,
      Display_Feature_Metadata *  dfm)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s",
                               dfm->feature_code, dh_repr(dh) );

   // DDCA_Feature_Metadata * extmeta = dfm_to_ddca_feature_metadata(meta);

   DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
   DDCA_Status            ddcrc      = 0;
   DDCA_Vcp_Feature_Code  feature_id = dfm->feature_code;

   if (!(dfm->feature_flags & DDCA_READABLE)) {
      char * feature_name =  dfm->feature_name;

      DDCA_Feature_Flags vflags = dfm->feature_flags;
      // should get vcp version from metadata
      if (vflags & DDCA_DEPRECATED)
         printf("Feature %02x (%s) is deprecated in MCCS %d.%d\n",
                feature_id, feature_name, vspec.major, vspec.minor);
      else
         printf("Feature %02x (%s) is not readable\n", feature_id, feature_name);
      ddcrc = DDCRC_INVALID_OPERATION;
   }

   if (ddcrc == 0) {
      char * formatted_value = NULL;
      ddcrc = ddc_get_formatted_value_for_display_feature_metadata(
               dh,
               dfm,
               false,      /* suppress_unsupported */
               true,       /* prefix_value_with_feature_code */
               &formatted_value,
               stdout);    /* msg_fh */
      if (formatted_value) {
         printf("%s\n", formatted_value);
         free(formatted_value);
      }
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(ddcrc));
   return ddcrc;
}


Public_Status_Code
app_show_single_vcp_value_by_feature_id_new_dfm(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Getting feature 0x%02x for %s, force=%s",
                              feature_id, dh_repr(dh), sbool(force) );

   Public_Status_Code         psc = 0;

   Display_Feature_Metadata * dfm =
   dyn_get_feature_metadata_by_dh(
         feature_id,
         dh,
         force || feature_id >= 0xe0    // with_default
         );

   // VCP_Feature_Table_Entry *  entry = NULL;


   if (!dfm) {
      printf("Unrecognized VCP feature code: 0x%02x\n", feature_id);
      psc = DDCRC_UNKNOWN_FEATURE;
   }
   else {
      psc = app_show_single_vcp_value_by_dfm(dh, dfm);
      dfm_free(dfm);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc));
   return psc;
}


/* Shows the VCP values for all features in a VCP feature subset.
 *
 * Arguments:
 *    dh                display handle
 *    subset_id         feature subset
 *    flags             option flags
 *    features_seen     if non-null, collect list of features found
 *
 * Returns:
 *    status code       from show_vcp_values()
 */
Public_Status_Code
app_show_vcp_subset_values_by_display_handle(
        Display_Handle *    dh,
        VCP_Feature_Subset  subset_id,
        Feature_Set_Flags   flags,
        Byte_Bit_Flags      features_seen)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting. dh=%s, subset_id=%s, flags=%s, features_seen=%p",
          dh_repr(dh), feature_subset_name(subset_id), feature_set_flag_names_t(flags), features_seen );

   GPtrArray * collector = NULL;
   Public_Status_Code psc = ddc_show_vcp_values(dh, subset_id, collector, flags, features_seen);

   if (debug || IS_TRACING()) {
      if (features_seen) {
        char * s = bbf_to_string(features_seen, NULL, 0);
        DBGMSG("Returning: %s. features_seen=%s",  psc_desc(psc), s);
        free(s);
      }
      else {
         DBGMSG("Returning: %s", psc_desc(psc));
      }
   }
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
      if (!bus_info ||  !(bus_info->feature_flags & I2C_BUS_ADDR_0X37) ) {
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
      ddc_show_vcp_values(pDispHandle, subset_id, collector, show_unsupported);
      ddc_close_display(pDispHandle);
   }
}
#endif


/* Shows the VCP values for all features indicated by a Feature_Set_Ref
 *
 * Arguments:
 *    dh                display handle
 *    fsref             feature set reference
 *    flags             option flags
 *
 * Returns:
 *    status code       from app_show_single_vcp_value_by_feature_id() or
 *                           app_show_subset_values_by_display_handle()
 */
Public_Status_Code
app_show_feature_set_values_by_display_handle(
      Display_Handle *     dh,
      Feature_Set_Ref *    fsref,
      Feature_Set_Flags    flags)
{
   bool debug = false;
   if (debug || IS_TRACING()) {
      char * s0 = feature_set_flag_names_t(flags);
      DBGMSG("Starting. dh: %s. fsref: %s, flags: %s", dh_repr(dh), fsref_repr_t(fsref), s0);
      // dbgrpt_feature_set_ref(fsref,1);
   }

   Public_Status_Code psc = 0;
   if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE) {
      psc = app_show_single_vcp_value_by_feature_id_new_dfm(
            dh, fsref->specific_feature, true);
   }
   else {
      psc = app_show_vcp_subset_values_by_display_handle(
            dh,
            fsref->subset,
            flags,
            NULL);
   }

   DBGTRC(debug, TRACE_GROUP, "Returning: %s", psc_desc(psc));
   return psc;
}


//
// Watch for changed VCP values
//

// Depending on monitor, writing 1 to feature x02 may throw the user
// out of the on-stream display.  Use carefully.

static void
reset_vcp_x02(Display_Handle * dh) {
   bool debug = false;
   Error_Info * ddc_excp = ddc_set_nontable_vcp_value(dh, 0x02, 0x01);
   if (ddc_excp) {
      DBGMSG("set_nontable_vcp_value_by_display_handle() returned %s", errinfo_summary(ddc_excp) );
      errinfo_free(ddc_excp);
   }
   else
      DBGMSF(debug, "reset feature x02 (new control value) successful");
}


/** Gets the ID of the next changed feature from VCP feature x52. If the feature
 *  code is other than x00, reads and displays the value of that feature.
 *
 *  \param   dh  #Display_Handle
 *  \param   p_changed_feature   return feature id read from feature x52
 *  \return  error reading feature x52
 *
 *  \remark
 *  The return value reflects only x52 errors, not any errors reading
 *  the feature id that is displayed
 */
static Error_Info *
show_changed_feature(Display_Handle * dh, Byte * p_changed_feature) {
   bool debug = false;
   Parsed_Nontable_Vcp_Response * nontable_response_loc = NULL;
   Error_Info * result = NULL;
   Error_Info * x52_error = ddc_get_nontable_vcp_value(dh, 0x52, &nontable_response_loc);
   DBGMSF(debug, "ddc_get_nontable_vcp_value( x52 ) returned %s", errinfo_summary(x52_error));
   if (x52_error) {
      if (x52_error->status_code == DDCRC_REPORTED_UNSUPPORTED ||
          x52_error->status_code == DDCRC_DETERMINED_UNSUPPORTED)
      {
         // printf("Feature x02 (New Control Value) reports new control values exist, but feature x52 (Active Control) unsupported\n");
         result = errinfo_new2(x52_error->status_code, __func__,
               "Feature x02 (New Control Value) reports that changed VCP feature values exist, but feature x52 (Active Control) is unsupported");
         errinfo_free(x52_error);
      }
      else {
         // DBGMSG("get_nontable_vcp_value() for VCP feature x52 returned %s", errinfo_summary(x52_error) );
         result = errinfo_new_with_cause2(
                  x52_error->status_code, x52_error, __func__, "Error reading feature x02");
      }
   }

  else {  // getvcp x52 succeeded
     *p_changed_feature = nontable_response_loc->sl;
     free(nontable_response_loc);
     DBGMSF(debug, "getvcp(x52) returned value 0x%02x", *p_changed_feature);
     if (*p_changed_feature)
        app_show_single_vcp_value_by_feature_id_new_dfm(dh, *p_changed_feature, false);
  }
  return result;
}


/* Checks for VCP feature changes by:
 *   - reading feature x02 to check if changes exist,
 *   - querying feature x52 for the id of a changed feature
 *   - reading and showing the value of the changed feature.
 *
 * If the VCP version is 2.1 or less a single feature is
 * read from x52.  For VCP version 3.0 and 2.2, x52 is a
 * FIFO queue of changed features.
 *
 * Finally, 1 is written to feature x02 as a reset.
 *
 * \param  dh    #Display_Handle
 * \param  force_no_fifo never treat feature x52 as a FIFO
 * \param  changes_reported set true if any changes were detected
 * \return error report, NULL if none
 */
static Error_Info *
app_read_changes(Display_Handle * dh, bool force_no_fifo, bool* changes_reported) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   int MAX_CHANGES = 20;
   *changes_reported = false;

   /* Per the 3.0 and 2.2 specs, feature x52 is a FIFO to be read until value x00 indicates empty
    * What apparently happens on 2.1 (U3011) is that each time feature x02 is reset with value x01
    * the subsequent read of feature x02 returns x02 (new control values exists) until the queue
    * of changes is flushed
    */

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   // DBGMSF(debug, "VCP version: %d.%d", vspec.major, vspec.minor);

   // Read feature x02 to determine if any features have changed
   //   xff: no user controls
   //   x01: no new control values
   //   x02: new control values exist
   Parsed_Nontable_Vcp_Response * p_nontable_response = NULL;

   Error_Info * result = NULL;
   Error_Info * x02_error = ddc_get_nontable_vcp_value(dh,0x02,&p_nontable_response);
   if (x02_error) {
      DBGMSG("get_nontable_vcp_value() for feature 0x02 returned error %s", errinfo_summary(x02_error) );
      // errinfo_free(ddc_excp);
      result = errinfo_new_with_cause2(x02_error->status_code, x02_error, __func__,
                                       "Error reading feature x02");
   }
   else {
      Byte x02_value = p_nontable_response->sl;
      DBGMSF(debug, "get_nontable_vcp_value() for feature 0x02 returned value 0x%02x", x02_value );
      free(p_nontable_response);

      if (x02_value == 0xff) {
         DBGMSF(debug, "No user controls exist");
         result = errinfo_new2(DDCRC_DETERMINED_UNSUPPORTED, __func__,
                        "Feature x02 (New Control Value) reports No User Controls");
      }

      else if (x02_value == 0x01) {
         DBGMSF(debug, "No new control values found");
         result = NULL;
      }

      else if (x02_value != 0x02){
         DBGMSF(debug, "x02 value = 0x%02x", x02_value);
         result = errinfo_new2(DDCRC_DETERMINED_UNSUPPORTED, __func__,
               "Feature x02 (New Control Value) reports unexpected value 0x%02", x02_value);
      }

      else {
         DBGMSF(debug, "New control values exist. x02 value: 0x%02x", x02_value);
         Byte changed_feature_id;

         if ( vcp_version_le(vspec, DDCA_VSPEC_V21)  || force_no_fifo) {
            Error_Info * x52_error = show_changed_feature(dh, &changed_feature_id);
            // MCCS spec requires that feature x02 be reset, otherwise it remains at x02
            // and the same value is read again
            // But on some displays it also turns off the OSD: HPZ22i
            // For other displays it does not turn off the OSD, so the user can make
            // additional changes:  Dell U3011
            reset_vcp_x02(dh);
            result = x52_error;
            if (!x52_error)
               *changes_reported = true;
         }

         else {  // x52 is a FIFO
            int ctr = 0;
            for (;ctr < MAX_CHANGES; ctr++) {
               Byte changed_feature_id;
               Error_Info * x52_error = show_changed_feature(dh, &changed_feature_id);
               if (x52_error) {
                  result = x52_error;
                  goto bye;
               }
               *changes_reported = true;
               if (changed_feature_id == 0x00) {
                  DBGMSG("No more changed features found");
                  reset_vcp_x02(dh);
                  result =  NULL;
                  break;
               }
            }
            if (ctr == MAX_CHANGES) {
               DBGMSG("Reached loop guard value MAX_CHANGES (%d)", MAX_CHANGES);
               reset_vcp_x02(dh);
               result = NULL;
            }
         }
      }
   }

 bye:
   return result;
}


#ifdef USE_USB
static void
app_read_changes_usb(Display_Handle * dh) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   // bool new_values_found = false;

   assert(dh->dref->io_path.io_mode == DDCA_IO_USB);
   int fd = dh->fd;
   int flaguref = HIDDEV_FLAG_UREF;
   struct hiddev_usage_ref uref;
   int rc = ioctl(fd, HIDIOCSFLAG, &flaguref);
   if (rc < 0) {
      REPORT_IOCTL_ERROR("HIDIOCSFLAG", errno);
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
         dbgrpt_hiddev_usage_ref(&uref, 1);
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
 * \param  dh  #Display_Handle
 * \param  force_no_fifo if true, do not regard feature x52 aa a FIFO queue,
 *                       even if VCP code is >= 2.2
 *
 * Returns only if an error occurs, otherwise runs forever
 */
void
app_read_changes_forever(Display_Handle * dh, bool force_no_fifo) {
   bool debug = false;

   printf("Watching for VCP feature changes on display %s\n", dh_repr(dh));
   printf("Type ^C to exit...\n");
   // show version here instead of in called function to declutter debug output:
   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
   DBGMSF(debug, "VCP version: %d.%d", vspec.major, vspec.minor);
   reset_vcp_x02(dh);
   while(true) {
      bool changes_reported = false;
#ifdef USE_USB
      if (dh->dref->io_path.io_mode == DDCA_IO_USB)
         app_read_changes_usb(dh);
      else
#endif
      {
         Error_Info * erec = app_read_changes(dh, force_no_fifo, &changes_reported);
         if (erec) {
            if (debug)
               DBGMSG("Fatal error reading changes: %s", errinfo_summary(erec));

            printf("%s\n", erec->detail);
            DDCA_Status rc = erec->status_code;
            errinfo_free(erec);
            if (rc == DDCRC_NULL_RESPONSE) {
               printf("Continuing WATCH execution\n");
            }
            else {
               printf("Terminating WATCH\n");
               return;
            }
         }
      }

      if (!changes_reported)
         sleep_millis( 2500);
   }
}
