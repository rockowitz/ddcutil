/** @file app_watch.c
 *  Implement the WATCH command
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifdef ENABLE_USB
#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
#endif

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/vcp_version.h"

#include "cmdline/parsed_cmd.h"

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
      DBGMSG("set_nontable_vcp_value_by_dh() returned %s", errinfo_summary(ddc_excp) );
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
         result = errinfo_new(x52_error->status_code, __func__,
               "Feature x02 (New Control Value) reports that changed VCP feature values exist, but feature x52 (Active Control) is unsupported");
         errinfo_free(x52_error);
      }
      else {
         // DBGMSG("get_nontable_vcp_value() for VCP feature x52 returned %s", errinfo_summary(x52_error) );
         result = errinfo_new_with_cause(
                  x52_error->status_code, x52_error, __func__, "Error reading feature x02");
      }
   }

  else {  // getvcp x52 succeeded
     *p_changed_feature = nontable_response_loc->sl;
     free(nontable_response_loc);
     DBGMSF(debug, "getvcp(x52) returned value 0x%02x", *p_changed_feature);
     if (*p_changed_feature)
        app_show_single_vcp_value_by_feature_id(dh, *p_changed_feature, false);
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, force_no_fifo = %s", dh_repr(dh), SBOOL(force_no_fifo));
   int MAX_CHANGES = 20;
   *changes_reported = false;

   /* Per the 3.0 and 2.2 specs, feature x52 is a FIFO to be read until value x00 indicates empty
    * What apparently happens on 2.1 (U3011) is that each time feature x02 is reset with value x01
    * the subsequent read of feature x02 returns x02 (new control values exists) until the queue
    * of changes is flushed
    */

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
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
      result = errinfo_new_with_cause(x02_error->status_code, x02_error, __func__,
                                       "Error reading feature x02");
   }
   else {
      Byte x02_value = p_nontable_response->sl;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "get_nontable_vcp_value() for feature 0x02 returned value 0x%02x", x02_value );
      free(p_nontable_response);

      if (x02_value == 0xff) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "No user controls exist");
         result = errinfo_new(DDCRC_DETERMINED_UNSUPPORTED, __func__,
                        "Feature x02 (New Control Value) reports No User Controls");
      }

      else if (x02_value == 0x01) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "No new control values found");
         result = NULL;
      }

      else if (x02_value != 0x02){
         DBGMSF(debug, "x02 value = 0x%02x", x02_value);
         result = errinfo_new(DDCRC_DETERMINED_UNSUPPORTED, __func__,
               "Feature x02 (New Control Value) reports unexpected value 0x%02", x02_value);
      }

      else {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "New control values exist. x02 value: 0x%02x", x02_value);
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
               Byte changed_feature_id = 0x00;
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
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "");
   return result;
}


#ifdef ENABLE_USB
static void
app_read_changes_usb(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr(dh));
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
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "tick");
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
   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
   DBGMSF(debug, "VCP version: %d.%d", vspec.major, vspec.minor);
   reset_vcp_x02(dh);
   while(true) {
      bool changes_reported = false;
#ifdef ENABLE_USB
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


void init_app_watch() {
   RTTI_ADD_FUNC(app_read_changes);
#ifdef USB
   RTTI_ADD_FUNC(app_read_changes_usb);
#endif
}
