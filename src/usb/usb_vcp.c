/* usb_vcp.c
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <linux/hiddev.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "util/hiddev_reports.h"
#include "util/hiddev_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"

#include "usb/usb_core.h"

#include "usb/usb_vcp.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_USB;


//
// *** Get and set VCP feature code values ***
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
Global_Status_Code
usb_get_usage(int fd, Usb_Monitor_Vcp_Rec * vcprec, __s32 * maxval, __s32 * curval) {
   bool debug = true;
   DBGMSF(debug, "Starting. fd=%d, vcprec=%p", fd, vcprec);
   Global_Status_Code gsc = 0;
   int rc;

   assert(vcprec->rinfo->report_type == vcprec->report_type);
   assert(vcprec->rinfo->report_type == HID_REPORT_TYPE_FEATURE ||
          vcprec->rinfo->report_type == HID_REPORT_TYPE_INPUT);   // *** CG19 ***
   assert(vcprec->rinfo->report_id   == vcprec->report_id);

   DBGMSF(debug, "report_type=%d (%s), report_id=%d, field_index=%d, usage_index=%d",
                 vcprec->report_type,
                 report_type_name(vcprec->report_type),
                 vcprec->report_id,
                 vcprec->field_index,
                 vcprec->usage_index);
   hid_get_report(fd, vcprec->rinfo, CALLOPT_ERR_MSG|CALLOPT_ERR_ABORT);

#ifdef OLD
   rc = ioctl(fd, HIDIOCGREPORT, vcprec->rinfo);
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
      // printf("(%s) Unable to get Feature report %d\n", __func__, vcprec->report_id);
      PROGRAM_LOGIC_ERROR("HIDIOCGREPORT returned %d for report %d", vcprec->report_id); // terminates execution
      gsc = DDCRC_REPORTED_UNSUPPORTED;   // *** TEMP **
      goto bye;
   }
   // DBGMSF(debug, "HIDIOCGREPORT succeeded");
#endif

   __s32 maxval1 = vcprec->finfo->logical_maximum;
   __s32 maxval2 = vcprec->finfo->physical_maximum;
   DBGMSF(debug, "logical_maximum: %d", maxval1);
   DBGMSF(debug, "physical_maximum: %d", maxval2);
   *maxval = maxval1;

   struct hiddev_usage_ref * uref = vcprec->uref;
#ifdef DISABLE
   uref->report_type = vcprec->report_type;
   uref->report_id   = vcprec->report_id;
   uref->field_index = vcprec->field_index;
   uref->usage_index = vcprec->usage_index;
#endif
   if (debug)
      report_hiddev_usage_ref(uref, 1);

#ifdef DISABLE
   rc = ioctl(fd, HIDIOCGUCODE, uref);  // Fills in usage code
   if (rc != 0) {
      int errsv = errno;
      REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);
      // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
      gsc = modulate_rc(errsv, RR_ERRNO);
      goto bye;
   }
#endif

   rc  = hid_get_usage_value(fd, uref, CALLOPT_ERR_MSG);
   // rc = ioctl(fd, HIDIOCGUSAGE, uref);  // Fills in usage value
   if (rc != 0) {
      int errsv = errno;
      // REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
      // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
      gsc = modulate_rc(-errsv, RR_ERRNO);
   }
   else {
      DBGMSF(debug, "usage_index=%d, value = 0x%08x",uref->usage_index, uref->value);
      *curval = uref->value;
      gsc = 0;
   }


   DBGMSF(debug, "Returning: %d", gsc);
   return gsc;
}


Global_Status_Code
usb_get_usage_alt(int fd, __u32 report_type, __u32 usage_code, __s32 * maxval, __s32 * curval) {
   bool debug = true;
   DBGMSF(debug, "Starting. fd=%d, report_type=%d, usage_code=0x%08x", fd, report_type, usage_code);
   Global_Status_Code gsc = 0;
   int rc;

   assert(report_type == HID_REPORT_TYPE_FEATURE ||
          report_type == HID_REPORT_TYPE_INPUT);   // *** CG19 ***

   struct hiddev_usage_ref uref = {0};
   uref.report_type = report_type;
   uref.report_id   = HID_REPORT_ID_UNKNOWN;
   uref.usage_code  = usage_code;

   rc = hid_get_usage_value(fd, &uref, CALLOPT_NONE);
   // rc = ioctl(fd, HIDIOCGUSAGE, &uref);  // Fills in usage value
   if (rc != 0) {
      int errsv = errno;
      // Problem: errno=22 (invalid argument) can mean the usage code is invalid,
      // i.e. invalid feature code, or another arg error which indicates a programming error
      if (errsv == EINVAL) {
         if (debug)
            REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
         gsc = DDCRC_DETERMINED_UNSUPPORTED;
      }
      else {
         REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
         // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
         gsc = modulate_rc(-errsv, RR_ERRNO);
      }
      // printf("(%s) errsv=%d, gsc=%d\n", __func__, errsv, gsc);
      goto bye;
   }
   *curval = uref.value;

   report_hiddev_usage_ref(&uref, 1);

   struct hiddev_field_info finfo = {0};
   finfo.report_type = uref.report_type;
   finfo.report_id   = uref.report_id;
   finfo.field_index = uref.field_index;    // ?

   rc = ioctl(fd, HIDIOCGFIELDINFO, &finfo);  // Fills in usage value
   if (rc != 0) {
      int errsv = errno;
      REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", rc);
      // occasionally see -1, errno = 22 invalid argument - for Battery System Page: Run Time to Empty
      gsc = modulate_rc(-errsv, RR_ERRNO);
      // printf("(%s) errsv=%d, gsc=%d\n", __func__, errsv, gsc);
      goto bye;
   }

   report_hiddev_field_info(&finfo, 1);

   __s32 maxval1 = finfo.logical_maximum;
   __s32 maxval2 = finfo.physical_maximum;
   DBGMSF(debug, "logical_maximum: %d", maxval1);
   DBGMSF(debug, "physical_maximum: %d", maxval2);
   *maxval = maxval1;
   gsc = 0;

bye:
   DBGMSF(debug, "Returning: %d (%s)", gsc, gsc_name(gsc));
   return gsc;
}

void usb_get_vesa_version(int fd, __u32 report_type) {
   __s32 maxval;
   __s32 curval;
   Global_Status_Code gsc = usb_get_usage_alt(fd, report_type, 0x00800004, &maxval, &curval);
   if (gsc != 0) {
      DBGMSG("usb_get_usage_alt() status code %d (%s)", gsc, gsc_name(gsc) );
   }
   else {
      DBGMSG("vesa version: 0x%08x", curval);
   }
}




Buffer *
simple_get_multibyte(int fd, __u32 report_type, __u32 usage_code, __u32 num_values) {
   bool debug = true;
   DBGMSF(debug, "Starting. fd=%d, report_type=%d", fd, report_type);
   // Global_Status_Code gsc = 0;
   int rc;
   Buffer * result = NULL;

   assert(report_type == HID_REPORT_TYPE_FEATURE ||
          report_type == HID_REPORT_TYPE_INPUT);   // *** CG19 ***

   struct hiddev_usage_ref_multi uref_multi;
   memset(&uref_multi, 0, sizeof(uref_multi));  // initialize all fields to make valgrind happy
   uref_multi.uref.report_type = report_type;
   uref_multi.uref.report_id   = HID_REPORT_ID_UNKNOWN;
   uref_multi.uref.usage_code  = usage_code;
   uref_multi.num_values = num_values; // needed? yes!

   rc = ioctl(fd, HIDIOCGUSAGES, &uref_multi);  // Fills in usage value
   if (rc != 0) {
      REPORT_IOCTL_ERROR("HIDIOCGUSAGES", rc);
      goto bye;
   }
   Byte edidbuf2[128];
   for (int ndx=0; ndx<128; ndx++)
      edidbuf2[ndx] = uref_multi.values[ndx] & 0xff;
   result = buffer_new_with_value(edidbuf2, 128, __func__);

bye:
   if (debug) {
      printf("(%s) Returning: %p\n", __func__, result);
      if (result) {
         buffer_dump(result);
      }
   }
   return result;
}


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
Global_Status_Code usb_get_nontable_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       Parsed_Nontable_Vcp_Response** ppInterpretedCode)
{
   bool debug = false;
   // Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   // TRCMSGTG(tg, "Reading feature 0x%02x, dh=%p, dh->dref=%p", feature_code, dh, dh->dref);
   DBGTRC(debug, TRACE_GROUP,
             "Reading feature 0x%02x, dh=%p, dh->dref=%p", feature_code, dh, dh->dref);

   assert(dh->io_mode == USB_IO);
   // if (!dh->dref) {
   //    DGBMSF(debug, "HACK: getting value for uninitialized dh->dref");
   //    ---
   // }

   Global_Status_Code gsc =  DDCRC_REPORTED_UNSUPPORTED;  // = 0;
   // Output_Level output_level = get_output_level();
   Parsed_Nontable_Vcp_Response * parsed_response = NULL;

   // DBGMSF(debug, "wolf 2. dh=%p, dh->dref=%p", dh, dh->dref);
   // Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_ref(dh->dref);
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   assert(moninfo);

   __s32 maxval;
   __s32 curval;
   bool use_alt_method = true;

   if (use_alt_method) {
      __u32 usage_code = 0x0082 << 16 | feature_code;
      gsc = usb_get_usage_alt(dh->fh, HID_REPORT_TYPE_FEATURE, usage_code, &maxval, &curval);
      if (gsc != 0)
         gsc = usb_get_usage_alt(dh->fh, HID_REPORT_TYPE_INPUT, usage_code, &maxval, &curval);
   }
   else {

      // find the field record
      GPtrArray * vcp_recs = moninfo->vcp_codes[feature_code];

      if (!vcp_recs) {
         DBGMSF(debug, "Unrecognized feature code 0x%02x", feature_code);
         gsc = DDCRC_REPORTED_UNSUPPORTED;
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
            gsc = usb_get_usage(dh->fh,  vcprec, &maxval, &curval);
            DBGMSF(debug, "usb_get_usage() usage index: %d returned %d, maxval=%d, curval=%d",
                          vcprec->usage_index, gsc, maxval, curval);
            if (gsc == 0)
               break;
         }
      }

   }

   if (gsc == 0) {
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

   // TRCMSGTG(tg, "Returning %s, *ppinterpreted_code=%p", gsc_name(gsc), parsed_response);
   DBGTRC(debug, TRACE_GROUP,
             "Returning %s, *ppinterpreted_code=%p", gsc_name(gsc), parsed_response);
   *ppInterpretedCode = parsed_response;
   return gsc;
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
Global_Status_Code usb_get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       Vcp_Value_Type            call_type,
       Single_Vcp_Value **       pvalrec)
{
   bool debug = false;
   // Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   // TRCMSGTG(tg, "Starting. Reading feature 0x%02x", feature_code);
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x", feature_code);

   Global_Status_Code gsc = 0;

#ifdef FUTURE
   Buffer * buffer = NULL;
#endif
   Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;
   Single_Vcp_Value * valrec = NULL;
   switch (call_type) {

   case (NON_TABLE_VCP_VALUE):
         gsc = usb_get_nontable_vcp_value(
                  dh,
                  feature_code,
                  &parsed_nontable_response);
         if (gsc == 0) {
            valrec = create_nontable_vcp_value(
                        feature_code,
                        parsed_nontable_response->mh,
                        parsed_nontable_response->ml,
                        parsed_nontable_response->sh,
                        parsed_nontable_response->sl);
            free(parsed_nontable_response);
         }
         break;

   case (TABLE_VCP_VALUE):
#ifdef FUTURE
         gsc = usb_get_table_vcp_value(
                 dh,
                 feature_code,
                 &buffer);
         if (gsc == 0) {
            valrec = create_table_vcp_value_by_buffer(feature_code, buffer);
            buffer_free(buffer, __func__);
         }
#endif
         gsc = DDCRC_REPORTED_UNSUPPORTED;  // TEMP - should test known features first
         break;
   }

   *pvalrec = valrec;

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", gsc_desc(gsc) );
   if (gsc == 0 && debug)
      report_single_vcp_value(valrec,1);
   assert( (gsc == 0 && *pvalrec) || (gsc != 0 && !*pvalrec) );
   return gsc;
}


/* Sets the value of usage, as identified by hiddev
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

// adapted from usbmonctl
Base_Status_Errno
set_control_value(int fd,
                  int report_type,
                  int report_id,
                  int field_idx,
                  int usage_idx,
                  int value)
{
   bool debug = true;
   int rc;
   Base_Status_Errno result = 0;

#ifdef NO
   if (control_hidden(report_id)) {
      fprintf(stderr, "Control 0x%x is hidden because it's probably broken.\n", report_id);
      return false;
   }
#endif
   struct hiddev_report_info rinfo = {
      .report_type = report_type,
      .report_id   = report_id,
   };
   struct hiddev_usage_ref uref = {
      .report_type = report_type,
      .report_id   = report_id,
      .field_index = field_idx,
      .usage_index = usage_idx,
      .value       = value,
   };
   if ((rc=ioctl(fd, HIDIOCSUSAGE, &uref)) < 0) {
      result = -errno;
      REPORT_IOCTL_ERROR("HIDIOSUSAGE", rc);
      goto bye;
   }
   if ((rc=ioctl(fd, HIDIOCSREPORT, &rinfo)) < 0) {
      result = -errno;
      REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);
      goto bye;
   }
   result = 0;

bye:
   DBGMSF(debug, "Returning: %d", result);
   return result;
}



// not working - HIDIOCSUSAGE fails
Global_Status_Code
set_usage_alt(int fd,
                  __u32 report_type,
                  __u32 usage_code,
                  __s32 value)
{
   bool debug = true;
   int rc;
   Base_Status_Errno result = 0;
   Global_Status_Code gsc = 0;

   struct hiddev_usage_ref uref = {
      .report_type = report_type,
      .report_id   = HID_REPORT_ID_UNKNOWN,
      .usage_code  = usage_code,
      .value       = value,
   };
   if ((rc=ioctl(fd, HIDIOCSUSAGE, &uref)) < 0) {
      result = -errno;
      REPORT_IOCTL_ERROR("HIDIOSUSAGE", rc);
      goto bye;
   }
   if (debug) {
      DBGMSG("HIDIOSUAGE succeeded");
      report_hiddev_usage_ref(&uref, 1);
   }
   struct hiddev_report_info rinfo = {
         .report_type = report_type,
         .report_id   = uref.report_id
   };

   if ((rc=ioctl(fd, HIDIOCSREPORT, &rinfo)) < 0) {
      result = -errno;
      REPORT_IOCTL_ERROR("HIDIOCSREPORT", rc);
      goto bye;
   }
   result = 0;

bye:
   if (result != 0)
      gsc = modulate_rc(result, RR_ERRNO);
   DBGMSF(debug, "Returning: %d", gsc);
   return gsc;
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
Global_Status_Code
usb_set_usage(int fd, Usb_Monitor_Vcp_Rec * vcprec, __s32 new_value) {
   bool debug = true;
   DBGMSF(debug, "Starting. fd=%d, vcprec=%p", fd, vcprec);
   Global_Status_Code gsc = 0;

   assert(vcprec->rinfo->report_type == vcprec->report_type);
   assert(vcprec->report_type == HID_REPORT_TYPE_FEATURE ||
          vcprec->report_type == HID_REPORT_TYPE_OUTPUT);    // CG19
   assert(vcprec->rinfo->report_id   == vcprec->report_id);

   DBGMSF(debug, "report_type=%d (%s), report_id=%d, field_index=%d, usage_index=%d, new_value=%d",
                 vcprec->report_type,
                 report_type_name(vcprec->report_type),
                 vcprec->report_id,
                 vcprec->field_index,
                 vcprec->usage_index,
                 new_value);

   Base_Status_Errno rc = set_control_value(fd,
                                            vcprec->report_type,
                                            vcprec->report_id,
                                            vcprec->field_index,
                                            vcprec->usage_index,
                                            new_value);
   if (rc < 0)
      gsc = modulate_rc(rc, RR_ERRNO);

   DBGMSF(debug, "Returning: %d", gsc);
   return gsc;
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
Global_Status_Code usb_set_nontable_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       int                    new_value)
{
   bool debug = true;
   // Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   // TRCMSGTG(tg, "Setting feature 0x%02x, dh=%p, dh->dref=%p, new_value=%d",
   //              feature_code, dh, dh->dref, new_value);
   DBGTRC(debug, TRACE_GROUP,
          "Setting feature 0x%02x, dh=%p, dh->dref=%p, new_value=%d",
          feature_code, dh, dh->dref, new_value);

   Global_Status_Code gsc =  DDCRC_REPORTED_UNSUPPORTED;  // = 0;
   assert(dh->io_mode == USB_IO);
   Usb_Monitor_Info * moninfo = usb_find_monitor_by_display_handle(dh);
   assert(moninfo);

   bool use_alt = false;
   if (use_alt) {
      __u32 usage_code = 0x0082 << 16 | feature_code;
      gsc = set_usage_alt(dh->fh, HID_REPORT_TYPE_FEATURE, usage_code, new_value);
      if (gsc != 0)
         gsc = set_usage_alt(dh->fh, HID_REPORT_TYPE_OUTPUT, usage_code, new_value);
      if (gsc == modulate_rc(EINVAL, RR_ERRNO))
         gsc = DDCRC_REPORTED_UNSUPPORTED;

   }
   else {

      // find the field record
      GPtrArray * vcp_recs = moninfo->vcp_codes[feature_code];
      if (!vcp_recs) {
         DBGMSF(debug, "Unrecognized feature code 0x%02x", feature_code);
         gsc = DDCRC_REPORTED_UNSUPPORTED;
      }
      else {
         DBGMSF(debug, "setting value");
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

            gsc = usb_set_usage(dh->fh,  vcprec, new_value);
            DBGMSF(debug, "usb_set_usage() usage index: %d returned %d",
                          vcprec->usage_index, gsc);
            if (gsc == 0)
               break;
         }
      }
   }

   // TRCMSGTG(tg, "Returning %s", gsc_name(gsc));
   DBGTRC(debug, TRACE_GROUP, "Returning %s", gsc_name(gsc));
   return gsc;
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
Global_Status_Code
usb_set_vcp_value(                               // changed from set_vcp_value()
      Display_Handle *   dh,
      Single_Vcp_Value * vrec)
{
   Global_Status_Code gsc = 0;
   if (vrec->value_type == NON_TABLE_VCP_VALUE) {
      gsc = usb_set_nontable_vcp_value(dh, vrec->opcode, vrec->val.c.cur_val);  // function name changed
   }
   else {
      assert(vrec->value_type == TABLE_VCP_VALUE);
      // gsc = usb_set_table_vcp_value(dh, vrec->opcode, vrec->val.t.bytes, vrec->val.t.bytect);
      gsc = DDCL_UNIMPLEMENTED;
   }

   return gsc;
}

