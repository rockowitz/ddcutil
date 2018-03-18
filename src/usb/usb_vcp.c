/* usb_vcp.c
 *
 * Get and set VCP feature codes for USB connected monitors.
 *
 * <copyright>
 * Copyright (C) 2017-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \cond */
#include <assert.h>
#include <errno.h>
#include <linux/hiddev.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
/** \endcond */

#include "util/report_util.h"
#include "util/string_util.h"

#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"

#include "usb/usb_displays.h"

#include "usb/usb_vcp.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_USB;


//
// Get and set HID usage values, parameterized only by HID data structures
//

/* Gets the value of usage by specifying the usage code
 *
 * Arguments:
 *   fd           file descriptor for open hiddev device
 *   report_type  HID_REPORT_TYPE_FEATURE or HID_REPORT_TYPE_OUTPUT
 *   usage_code   usage code to get
 *   maxval       where to return maximum values
 *   curval       where to return current value
 *
 * Returns:       status code
 */
Public_Status_Code
usb_get_usage_value_by_report_type_and_ucode(
      int     fd,
      __u32   report_type,
      __u32   usage_code,
      __s32 * maxval,
      __s32 * curval) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, report_type=%d, usage_code=0x%08x", fd, report_type, usage_code);
   Public_Status_Code psc = 0;
   Status_Errno rc;
   *curval = 0;  // so there's a definite value in case of failure ...
   *maxval = 0;  // avoids complaints by clang analyzer

   assert(report_type == HID_REPORT_TYPE_FEATURE ||
          report_type == HID_REPORT_TYPE_INPUT);   // *** CG19 ***

   struct hiddev_usage_ref uref = {0};
   uref.report_type = report_type;
   uref.report_id   = HID_REPORT_ID_UNKNOWN;
   uref.usage_code  = usage_code;

   rc = hiddev_get_usage_value(fd, &uref, CALLOPT_NONE);
   // rc = ioctl(fd, HIDIOCGUSAGE, &uref);  // Fills in usage value
   if (rc != 0) {
      // Problem: errno=22 (invalid argument) can mean the usage code is invalid,
      // i.e. invalid feature code, or another arg error which indicates a programming error
      // occasionally errno = 22 invalid argument - for Battery System Page: Run Time to Empty
      if (-rc == EINVAL) {
         if (debug)
            REPORT_IOCTL_ERROR("HIDIOCGUSAGE", -rc);
         psc = DDCRC_DETERMINED_UNSUPPORTED;
      }
      else {
         REPORT_IOCTL_ERROR("HIDIOCGUSAGE", -rc);
         // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
         // gsc = modulate_rc(-errsv, RR_ERRNO);
         psc = rc;
      }

      if (debug) {
         DBGMSG0("After hid_get_usage_value():");
         report_hiddev_usage_ref(&uref, 1);
      }
      // printf("(%s) errsv=%d, gsc=%d\n", __func__, errsv, gsc);
      goto bye;
   }
   *curval = uref.value;

   if (debug)
      report_hiddev_usage_ref(&uref, 1);

   struct hiddev_field_info finfo = {0};
   finfo.report_type = uref.report_type;
   finfo.report_id   = uref.report_id;
   finfo.field_index = uref.field_index;    // ?

   rc = ioctl(fd, HIDIOCGFIELDINFO, &finfo);  // Fills in usage value
   if (rc != 0) {
      int errsv = errno;
      REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", errsv);
      // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
      // gsc = modulate_rc(-errsv, RR_ERRNO);
      psc = -errsv;
      // printf("(%s) errsv=%d, gsc=%d\n", __func__, errsv, gsc);
      goto bye;
   }

   if (debug)
      report_hiddev_field_info(&finfo, 1);

   // per spec, logical max/min bound the values in the report,
   // physical min/max bound the "real world" units
   // if physical min/max = 0, set physical to logical
   // So we should use logical max as ddc maxval.
   // But logical_minimum can be < 0 per USB spec,
   // in which case the value in the report is interpreted as a
   // 2's complement number.  How to handle this?
   // Map to a range >= 0?  e.g. -128..127 -> 0..255
   __s32 maxval1 = finfo.logical_maximum;
   __s32 maxval2 = finfo.physical_maximum;
   DBGMSF(debug, "logical_maximum: %d", maxval1);
   DBGMSF(debug, "physical_maximum: %d", maxval2);
   *maxval = finfo.logical_maximum;
   if (finfo.logical_minimum < 0) {
      DBGMSG("Unexpected: logical_minimum (%d) for field is < 0", finfo.logical_minimum);
   }
   psc = 0;

bye:
   DBGMSF(debug, "Returning: %s",  psc_desc(psc));
   return psc;
}


/* Sets the value of usage, with explicit report field, and usage indexes
 *
 * Arguments:
 *   fd           file descriptor for open hiddev device
 *   report_type  HID_REPORT_TYPE_FEATURE or HID_REPORT_TYPE_OUTPUT
 *   report_id    report number
 *   field_idx    field number
 *   usage_idx    usage number
 *   value        value to set
 *
 * Returns:       status code
 *
 * Adapted from usbmonctl
 */
Status_Errno
set_control_value(int fd,
                  int report_type,
                  int report_id,
                  int field_ndx,
                  int usage_ndx,
                  int value)
{
   bool debug = false;
   DBGMSF(debug,
         "Starting. fd=%d, report_type=%d, report_id=%d, field_ndx=%d, usage_ndx=%d, value=%d",
         fd, report_type, report_type, field_ndx, usage_ndx, value);
   int rc;
   Status_Errno result = 0;

   struct hiddev_report_info rinfo = {
      .report_type = report_type,
      .report_id   = report_id,
   };
   struct hiddev_usage_ref uref = {
      .report_type = report_type,
      .report_id   = report_id,
      .field_index = field_ndx,
      .usage_index = usage_ndx,
      .value       = value,
   };
   if (debug) {
      DBGMSG0("Before HIDIOCSUSAGE");
      report_hiddev_usage_ref(&uref, 1);
   }
   if ((rc=ioctl(fd, HIDIOCSUSAGE, &uref)) < 0) {
      result = -errno;
      REPORT_IOCTL_ERROR("HIDIOCSUSAGE", errno);
      goto bye;
   }
   if ((rc=ioctl(fd, HIDIOCSREPORT, &rinfo)) < 0) {
      result = -errno;
      REPORT_IOCTL_ERROR("HIDIOCGUSAGE", errno);
      goto bye;
   }
   result = 0;

bye:
   DBGMSF(debug, "Returning: %d", result);
   return result;
}


/* Sets the value of usage based on its usage code.
 * It is left to hiddev to determine the actual report field, and usage indexes
 *
 * Arguments:
 *   fd           file descriptor for open hiddev device
 *   report_type  HID_REPORT_TYPE_FEATURE or HID_REPORT_TYPE_OUTPUT
 *   report_id    report number
 *   field_idx    field number
 *   usage_idx    usage number
 *   value        value to set
 *
 * Returns:       status code
 */
Public_Status_Code
set_usage_value_by_report_type_and_ucode(
                  int   fd,
                  __u32 report_type,
                  __u32 usage_code,
                  __s32 value)
{
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, report_type=%d, usage_code=0x%08x, value=%d",
                 fd, report_type, usage_code, value);
   int rc;
   Public_Status_Code psc = 0;

   struct hiddev_usage_ref uref = {
      .report_type = report_type,
      .report_id   = HID_REPORT_ID_UNKNOWN,
      .usage_code  = usage_code,
      .value       = value,
   };
   if (debug) {
      DBGMSG0("Before HIDIOCSUSAGE");
      report_hiddev_usage_ref(&uref, 1);
   }
   if ((rc=ioctl(fd, HIDIOCSUSAGE, &uref)) < 0) {
      psc = -errno;
      REPORT_IOCTL_ERROR("HIDIOCSUSAGE", errno);
      goto bye;
   }
   // if (debug) {
   //    DBGMSG("HIDIOCSUSAGE succeeded");
   //    report_hiddev_usage_ref(&uref, 1);
   // }

   // need to get the actual report_id - HIDIOCSREPORT fails if HID_REPORT_ID_UNKNOWN
   psc = hiddev_get_usage_value(fd, &uref, CALLOPT_ERR_MSG);   // should never fail, deleted CALLOPT_ERR_ABORT
   // if (debug) {
   //    DBGMSG("After get_hid_usage_value()");
   //    report_hiddev_usage_ref(&uref, 1);
   // }
   if (psc < 0)          // should never occur
      goto bye;


   struct hiddev_report_info rinfo = {
         .report_type = report_type,
         .report_id   = uref.report_id
   };

   if ((rc=ioctl(fd, HIDIOCSREPORT, &rinfo)) < 0) {
      psc = -errno;
      REPORT_IOCTL_ERROR("HIDIOCSREPORT", errno);
      goto bye;
   }
   psc = 0;

bye:
   DBGMSF(debug, "Returning: %s", psc_desc(psc) );
   return psc;
}


//
// Get and set based on a Usb_Monitor_Vcp_Rec
//

/* Gets the current value of a usage, as identified by a Usb_Monitor_Vcp_Rec
 *
 * Arguments:
 *    fd      file descriptor for open hiddev device
 *    vcprec  pointer to a Usb_Monitor_Vcp_Rec identifying the value to retrieve
 *    maxval  address at which to return max value of the usage
 *    curval  address at which to return the current value of the usage
 *
 * Returns:  status code
 *
 * Calls to this function are valid only for Feature or Input reports.
 */
Public_Status_Code
usb_get_usage_value_by_vcprec(
      int                   fd,
      Usb_Monitor_Vcp_Rec * vcprec,
      __s32 *               maxval,
      __s32 *               curval)
{
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, vcprec=%p", fd, vcprec);
   Public_Status_Code psc = 0;
   int rc;

   assert(vcprec->rinfo->report_type == vcprec->report_type);
   assert(vcprec->rinfo->report_type == HID_REPORT_TYPE_FEATURE ||
          vcprec->rinfo->report_type == HID_REPORT_TYPE_INPUT);   // *** CG19 ***
   assert(vcprec->rinfo->report_id   == vcprec->report_id);

   DBGMSF(debug, "report_type=%d (%s), report_id=%d, field_index=%d, usage_index=%d",
                 vcprec->report_type,
                 hiddev_report_type_name(vcprec->report_type),
                 vcprec->report_id,
                 vcprec->field_index,
                 vcprec->usage_index);
   rc = hiddev_get_report(fd, vcprec->rinfo, CALLOPT_ERR_MSG);   // |CALLOPT_ERR_ABORT);
   if (rc < 0) {
      psc = rc;
      goto bye;
   }

   __s32 maxval1 = vcprec->finfo->logical_maximum;
   __s32 maxval2 = vcprec->finfo->physical_maximum;
   DBGMSF(debug, "logical_maximum: %d", maxval1);
   DBGMSF(debug, "physical_maximum: %d", maxval2);
   *maxval = vcprec->finfo->logical_maximum;
   if (vcprec->finfo->logical_minimum < 0) {
      DBGMSG("Unexpected: logical_minmum (%d) is < 0", vcprec->finfo->logical_minimum);
   }

   struct hiddev_usage_ref * uref = vcprec->uref;
#ifdef DISABLE
   uref->report_type = vcprec->report_type;
   uref->report_id   = vcprec->report_id;
   uref->field_index = vcprec->field_index;
   uref->usage_index = vcprec->usage_index;
#endif
   if (debug)
      report_hiddev_usage_ref(uref, 1);

   psc  = hiddev_get_usage_value(fd, uref, CALLOPT_ERR_MSG);
   // rc = ioctl(fd, HIDIOCGUSAGE, uref);  // Fills in usage value
   if (psc != 0) {
      // REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
      // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
      // gsc = modulate_rc(-errsv, RR_ERRNO);
   }
   else {
      DBGMSF(debug, "usage_index=%d, value = 0x%08x",uref->usage_index, uref->value);
      *curval = uref->value;
   }

bye:
   DBGMSF(debug, "Returning: %s", psc_desc(psc) );
   return psc;
}


/* Sets the value of a usage, as identified by a Usb_Monitor_Vcp_Rec
 *
 * Arguments:
 *    fd         file descriptor for open hiddev device
 *    vcprec     pointer to a Usb_Monitor_Vcp_Rec identifying the usage to set
 *    new_value  new value
 *
 * Returns:  status code
 *
 * Calls to this function are valid only for Feature or Output reports.
 */
Public_Status_Code
usb_set_usage_value_by_vcprec(
      int                   fd,
      Usb_Monitor_Vcp_Rec * vcprec,
      __s32                 new_value)
{
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, vcprec=%p", fd, vcprec);
   Public_Status_Code psc = 0;

   assert(vcprec->rinfo->report_type == vcprec->report_type);
   assert(vcprec->report_type == HID_REPORT_TYPE_FEATURE ||
          vcprec->report_type == HID_REPORT_TYPE_OUTPUT);    // CG19
   assert(vcprec->rinfo->report_id   == vcprec->report_id);

   DBGMSF(debug, "report_type=%d (%s), report_id=%d, field_index=%d, usage_index=%d, new_value=%d",
                 vcprec->report_type,
                 hiddev_report_type_name(vcprec->report_type),
                 vcprec->report_id,
                 vcprec->field_index,
                 vcprec->usage_index,
                 new_value);

   Status_Errno rc = set_control_value(fd,
                                            vcprec->report_type,
                                            vcprec->report_id,
                                            vcprec->field_index,
                                            vcprec->usage_index,
                                            new_value);
   if (rc < 0)
      psc = rc;     // to simplify
 //     gsc = modulate_rc(rc, RR_ERRNO);

   DBGMSF(debug, "Returning: %s", psc_desc(psc));
   return psc;
}


//
//  High level getters/setters
//

/* Gets the value for a non-table feature.
 *
 * Arguments:
 *   dh                 handle for open display
 *   feature_code
 *   ppInterpretedCode  where to return result
 *
 * Returns:
 *   status code
 */
Public_Status_Code
usb_get_nontable_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       Parsed_Nontable_Vcp_Response** ppInterpretedCode)
{
   bool debug = false;
   // Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   // TRCMSGTG(tg, "Reading feature 0x%02x, dh=%p, dh->dref=%p", feature_code, dh, dh->dref);
   DBGTRC(debug, TRACE_GROUP,
             "Reading feature 0x%02x, dh=%p, dh->dref=%p", feature_code, dh, dh->dref);

   assert(dh->dref->io_path.io_mode == DDCA_IO_USB);
   // if (!dh->dref) {
   //    DGBMSF(debug, "HACK: getting value for uninitialized dh->dref");
   //    ---
   // }

   Public_Status_Code psc =  DDCRC_REPORTED_UNSUPPORTED;  // = 0;
   // Output_Level output_level = get_output_level();
   Parsed_Nontable_Vcp_Response * parsed_response = NULL;

   // DBGMSF(debug, "wolf 2. dh=%p, dh->dref=%p", dh, dh->dref);
   // Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dh->dref);
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   assert(moninfo);

   __s32 maxval = 0;    // initialization logically unnecessary, but avoids clang scan warning
   __s32 curval = 0;    // ditto
   bool use_alt_method = true;

   if (use_alt_method) {
      __u32 usage_code = 0x0082 << 16 | feature_code;
      psc = usb_get_usage_value_by_report_type_and_ucode(
                  dh->fh, HID_REPORT_TYPE_FEATURE, usage_code, &maxval, &curval);
      if (psc != 0)
         psc = usb_get_usage_value_by_report_type_and_ucode(
                  dh->fh, HID_REPORT_TYPE_INPUT,   usage_code, &maxval, &curval);
   }
   else {
      // find the field record
      GPtrArray * vcp_recs = moninfo->vcp_codes[feature_code];

      if (!vcp_recs) {
         DBGMSF(debug, "Unrecognized feature code 0x%02x", feature_code);
         psc = DDCRC_REPORTED_UNSUPPORTED;
      }
      else {
         // DBGMSF(debug, "reading value");
         // for testing purposes, try using each entry
         // usage 0 returns correct value, usage 1 returns 0
         // is usage 1 for writing?

         for (int ndx=0; ndx<vcp_recs->len; ndx++) {
         int ndx = 0;
            Usb_Monitor_Vcp_Rec * vcprec = g_ptr_array_index(vcp_recs,ndx);
            assert( memcmp(vcprec->marker, USB_MONITOR_VCP_REC_MARKER,4) == 0 );

            if (vcprec->report_type == HID_REPORT_TYPE_OUTPUT)
               continue;
            psc = usb_get_usage_value_by_vcprec(dh->fh,  vcprec, &maxval, &curval);
            DBGMSF(debug, "usb_get_usage() usage index: %d returned %d, maxval=%d, curval=%d",
                          vcprec->usage_index, psc, maxval, curval);
            if (psc == 0)
               break;
         }
      }
   }

   if (psc == 0) {
      parsed_response = calloc(1, sizeof(Parsed_Nontable_Vcp_Response));
      parsed_response->vcp_code = feature_code;
      parsed_response->valid_response = true;
      parsed_response->supported_opcode = true;
      parsed_response->cur_value = curval;
      parsed_response->max_value = maxval;
      parsed_response->mh = (maxval >> 8) & 0xff;
      parsed_response->ml = maxval & 0xff;
      parsed_response->sh = (curval >> 8) & 0xff;
      parsed_response->sl = curval & 0xff;
   }

   DBGTRC(debug, TRACE_GROUP,
             "Returning %s, *ppinterpreted_code=%p", psc_desc(psc), parsed_response);
   *ppInterpretedCode = parsed_response;
   return psc;
}


/* Gets the value of a VCP feature.
 *
 * Arguments:
 *   dh              handle for open display
 *   feature_code    feature code id
 *   call_type       indicates whether table or non-table
 *   pvalrec         location where to return newly allocated result
 *
 * Returns:
 *   status code
 *
 * The caller is responsible for freeing the value result returned.
 */
// only changes from get_vcp_value are function names
Public_Status_Code usb_get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       DDCA_Vcp_Value_Type            call_type,
       Single_Vcp_Value **       pvalrec)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x", feature_code);

   Public_Status_Code psc = 0;

#ifdef FUTURE
   Buffer * buffer = NULL;
#endif
   Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;
   Single_Vcp_Value * valrec = NULL;
   switch (call_type) {

   case (DDCA_NON_TABLE_VCP_VALUE):
         psc = usb_get_nontable_vcp_value(
                  dh,
                  feature_code,
                  &parsed_nontable_response);
         if (psc == 0) {
            valrec = create_nontable_vcp_value(
                        feature_code,
                        parsed_nontable_response->mh,
                        parsed_nontable_response->ml,
                        parsed_nontable_response->sh,
                        parsed_nontable_response->sl);
            free(parsed_nontable_response);
         }
         break;

   case (DDCA_TABLE_VCP_VALUE):
#ifdef FUTURE
         psc = usb_get_table_vcp_value(
                 dh,
                 feature_code,
                 &buffer);
         if (psc == 0) {
            valrec = create_table_vcp_value_by_buffer(feature_code, buffer);
            buffer_free(buffer, __func__);
         }
#endif
         psc = DDCRC_REPORTED_UNSUPPORTED;  // TEMP - should test known features first
         break;
   }

   *pvalrec = valrec;

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc) );
   if (psc == 0 && debug)
      report_single_vcp_value(valrec,1);
   assert( (psc == 0 && *pvalrec) || (psc != 0 && !*pvalrec) );
   return psc;
}


/* Sets the value for a non-table feature.
 *
 * Arguments:
 *   dh                 handle for open display
 *   feature_code
 *   new_value          value to set
 *
 * Returns:
 *   status code
 */
Public_Status_Code
usb_set_nontable_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       int                    new_value)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Setting feature 0x%02x, dh=%p, dh->dref=%p, new_value=%d",
          feature_code, dh, dh->dref, new_value);

   Public_Status_Code psc =  DDCRC_REPORTED_UNSUPPORTED;  // = 0;
   assert(dh->dref->io_path.io_mode == DDCA_IO_USB);
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   assert(moninfo);

   bool use_alt = true;
   if (use_alt) {
      __u32 usage_code = 0x0082 << 16 | feature_code;
      psc = set_usage_value_by_report_type_and_ucode(
               dh->fh, HID_REPORT_TYPE_FEATURE, usage_code, new_value);
      // if (gsc != 0)
      //    gsc = set_usage_value_by_report_type_and_ucode(dh->fh, HID_REPORT_TYPE_OUTPUT, usage_code, new_value);
      // if (gsc == modulate_rc(EINVAL, RR_ERRNO))
      //   gsc = DDCRC_REPORTED_UNSUPPORTED;
      if (psc == -EINVAL)
         psc = DDCRC_REPORTED_UNSUPPORTED;

   }
   else {
      // find the field record
      GPtrArray * vcp_recs = moninfo->vcp_codes[feature_code];
      if (!vcp_recs) {
         DBGMSF(debug, "Unrecognized feature code 0x%02x", feature_code);
         psc = DDCRC_REPORTED_UNSUPPORTED;
      }
      else {
         DBGMSF0(debug, "setting value");
         // for testing purposes, try using each entry
         // for reading, usage 0 returns correct value, usage 1 returns 0
         // is usage 1 for writing?
         // when writing, usage 0 works properly
         //  usage 1, at least for brightness, sets control to max value

         for (int ndx=0; ndx<vcp_recs->len; ndx++) {
            Usb_Monitor_Vcp_Rec * vcprec = g_ptr_array_index(vcp_recs,ndx);
            assert( memcmp(vcprec->marker, USB_MONITOR_VCP_REC_MARKER,4) == 0 );
            if (vcprec->report_type == HID_REPORT_TYPE_INPUT)
               continue;

            psc = usb_set_usage_value_by_vcprec(dh->fh,  vcprec, new_value);
            DBGMSF(debug, "usb_set_usage() usage index: %d returned %s",
                          vcprec->usage_index, psc_desc(psc) );
            if (psc == 0)
               break;
         }
      }
   }

   DBGTRC(debug, TRACE_GROUP, "Returning %s", psc_desc(psc));
   return psc;
}


/* Sets a VCP feature value.
 *
 * Arguments:
 *    dh            display handle for open display
 *    vrec          pointer to value record
 *
 *  Returns:
 *     status code
 */
Public_Status_Code
usb_set_vcp_value(                               // changed from set_vcp_value()
      Display_Handle *   dh,
      Single_Vcp_Value * vrec)
{
   Public_Status_Code psc = 0;
   if (vrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      psc = usb_set_nontable_vcp_value(dh, vrec->opcode, vrec->val.c.cur_val);  // function name changed
   }
   else {
      assert(vrec->value_type == DDCA_TABLE_VCP_VALUE);
      // gsc = usb_set_table_vcp_value(dh, vrec->opcode, vrec->val.t.bytes, vrec->val.t.bytect);
      psc = DDCRC_UNIMPLEMENTED;
   }

   return psc;
}


//
// Special case: get VESA version
//

// 7/2016: this code is based on USB HID Monitor spec.
// have yet to see a monitor that supports VESA Version usage code
__s32 usb_get_vesa_version_by_report_type(int fd, __u32 report_type) {
   bool debug = false;
   __s32 maxval;
   __s32 curval;
   Public_Status_Code psc = usb_get_usage_value_by_report_type_and_ucode(
                               fd, report_type, 0x00800004, &maxval, &curval);
   if (psc != 0 && debug) {
      DBGMSG("report_type=%s, usb_get_usage_alt() status code %s",
             hiddev_report_type_name(report_type), psc_desc(psc)  );
   }

   // DBGMSF(debug, "report_type=%s, returning: 0x%08x", report_type_name(report_type), curval);
   return curval;
}


__s32 usb_get_vesa_version(int fd) {
   bool debug = false;

   __s32 vesa_ver =  usb_get_vesa_version_by_report_type(fd, HID_REPORT_TYPE_FEATURE);
   if (!vesa_ver)
      vesa_ver = usb_get_vesa_version_by_report_type(fd, HID_REPORT_TYPE_INPUT);

   // DBGMSF(debug, "VESA version from usb_get_vesa_version_by_report_type(): 0x%08x", vesa_ver);

   DBGMSF(debug, "returning: 0x%08x", vesa_ver);
   return vesa_ver;
}


