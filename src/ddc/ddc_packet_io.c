/* ddc_packet_io.c
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.
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

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/string_util.h"

#include "base/ddc_errno.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/try_stats.h"

#include "ddc/ddc_packet_io.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;

// Tests if a range of bytes is entirely 0
static bool all_zero(Byte * bytes, int bytect) {
   bool result = true;
   int ndx = 0;
   for (; ndx < bytect; ndx++) {
      if (bytes[ndx] != 0x00) {
         result = false;
         break;
      }
   }
   return result;
}

// Test for DDC null message
#ifdef UNUSED
bool is_ddc_null_message(Byte * packet) {
   return (packet[0] == 0x6f &&
           packet[1] == 0x6e &&
           packet[2] == 0x80 &&
           packet[3] == 0xbe
          );
}
#endif


//
// Open/Close Display
//

/* Opens a DDC display.
 *
 * Arguments:
 *    dref            display reference
 *    failure_action  if open fails, return error or exit program?
 *
 * Returns:
 *    Display_Handle of opened display, or NULL if open failed and
 *       failure_action == RETURN_ERROR_IF_FAILURE
 */
// NB: calls i2c_set_addr() if I2C bus.  this call is valid
// only for opening bus for VCP, not EDID
Display_Handle* ddc_open_display(Display_Ref * dref,  Byte open_flags) {
   bool debug = false;
   DBGMSF(debug,"Opening display %s",dref_short_name(dref));
   Display_Handle * pDispHandle = NULL;
   // Byte open_flags = 0x00;
   // if (EXIT_IF_FAILURE)
   //    open_flags |= CALLOPT_ERR_ABORT;
   switch (dref->io_mode) {

   case DDC_IO_DEVI2C:
      {
         int fd = i2c_open_bus(dref->busno, open_flags);
         // Parsed_Edid * pedid2 = i2c_get_parsed_edid_by_fd(fd);
         // TODO: handle open failure, when failure_action = return error
         // all callers currently EXIT_IF_FAILURE
         if (fd >= 0) {    // will be < 0 if open_i2c_bus failed and failure_action = RETURN_ERROR_IF_FAILURE
            DBGMSF(debug, "Calling set_addr(,0x37) for dref=%p %s", dref, dref_repr(dref));
            i2c_set_addr(fd, 0x37);

            // Is this needed?
            // 10/24/15, try disabling:
            // sleepMillisWithTrace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);

            pDispHandle = create_bus_display_handle_from_display_ref(fd, dref);
            Bus_Info * bus_info = i2c_get_bus_info(dref->busno, DISPSEL_VALID_ONLY);   // or DISPSEL_NONE?
            pDispHandle->pedid = bus_info->edid;
         }
      else {
         log_status_code(modulate_rc(fd, RR_ERRNO), __func__);
      }
      break;
   }

   case DDC_IO_ADL:
      pDispHandle = create_adl_display_handle_from_display_ref(dref);
      pDispHandle->pedid = adlshim_get_parsed_edid_by_display_handle(pDispHandle);
      break;

   case USB_IO:
#ifdef USE_USB
      {
         // bool emit_error_msg = true;
         DBGMSF(debug, "Opening USB device: %s", dref->usb_hiddev_name);
         assert(dref->usb_hiddev_name);
         // if (!dref->usb_hiddev_name) { // HACK
         //    DBGMSG("HACK FIXUP.  dref->usb_hiddev_name");
         //    dref->usb_hiddev_name = get_hiddev_devname_by_display_ref(dref);
         // }
         int fd = usb_open_hiddev_device(dref->usb_hiddev_name, open_flags);
         if (fd < 0) {
            log_status_code(modulate_rc(fd,RR_ERRNO),__func__);
         }
         else {
            pDispHandle = create_usb_display_handle_from_display_ref(fd, dref);
            pDispHandle->pedid = usb_get_parsed_edid_by_display_handle(pDispHandle);
         }
      }
#else
      PROGRAM_LOGIC_ERROR("ddctool not built with USB support");
#endif
      break;
   } // switch
   assert(pDispHandle->pedid);
   // needed?  for both or just I2C?
   // sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);
   if (dref->io_mode != USB_IO)
      call_tuned_sleep_i2c(SE_POST_OPEN);
   // report_display_handle(pDispHandle, __func__);
   return pDispHandle;
}


/* Closes a DDC display.
 *
 * Arguments:
 *    dh            display handle
 *
 * Logs status code but continues execution if error.
 */
void ddc_close_display(Display_Handle * dh) {
   bool debug = false;
   if (debug) {
      DBGMSG("Starting.");
      report_display_handle(dh, __func__, 1);
   }
   // bool failure_action = RETURN_ERROR_IF_FAILURE;
   switch(dh->io_mode) {
   case DDC_IO_DEVI2C:
      {
         int rc = i2c_close_bus(dh->fh, dh->busno,  CALLOPT_NONE);    // return error if failure
         if (rc != 0) {
            DBGMSG("close_i2c_bus returned %d", rc);
            log_status_code(modulate_rc(rc, RR_ERRNO), __func__);
         }
         dh->fh = -1;    // indicate invalid, in case we try to continue using dh
         break;
      }
   case DDC_IO_ADL:
      break;           // nothing to do

   case USB_IO:
#ifdef USE_USB
      {
         int rc = usb_close_device(dh->fh, dh->hiddev_device_name, CALLOPT_NONE); // return error if failure
         if (rc != 0) {
            DBGMSG("usb_closedevice returned %d", rc);
            log_status_code(modulate_rc(rc, RR_ERRNO), __func__);
         }
         dh->fh = -1;
         break;
      }
#else
      PROGRAM_LOGIC_ERROR("ddctool not build with USB support");
#endif
   } //switch
}


//
// Retry Management and Statistics
//

// constants in parms.h:
static int max_write_only_exchange_tries =  MAX_WRITE_ONLY_EXCHANGE_TRIES;
static int max_write_read_exchange_tries =  MAX_WRITE_READ_EXCHANGE_TRIES;


static void * write_read_stats_rec = NULL;
static void * write_only_stats_rec = NULL;


void ddc_reset_write_read_stats() {
   if (write_read_stats_rec)
      try_data_reset(write_read_stats_rec);
   else
      write_read_stats_rec = try_data_create("ddc write/read", max_write_read_exchange_tries);
}


void ddc_report_write_read_stats() {
   assert(write_read_stats_rec);
   try_data_report(write_read_stats_rec);
}


void ddc_reset_write_only_stats() {
   if (write_only_stats_rec)
      try_data_reset(write_only_stats_rec);
   else
      write_only_stats_rec = try_data_create("ddc write only", max_write_only_exchange_tries);
}


void ddc_report_write_only_stats() {
   assert(write_only_stats_rec);
   try_data_report(write_only_stats_rec);
}


void ddc_set_max_write_only_exchange_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_TRIES);
   max_write_only_exchange_tries = ct;
   if (write_only_stats_rec)
      try_data_set_max_tries(write_only_stats_rec, ct);
}


int ddc_get_max_write_only_exchange_tries() {
   return max_write_only_exchange_tries;
}


void ddc_set_max_write_read_exchange_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_TRIES);
   max_write_read_exchange_tries = ct;
   if (write_read_stats_rec)
      try_data_set_max_tries(write_read_stats_rec, ct);
}

int ddc_get_max_write_read_exchange_tries() {
   return max_write_read_exchange_tries;
}



// work in progress

// typedef for ddc_i2c_write_read_raw, ddc_adl_write_read_raw, ddc_write_read_raw


typedef
Global_Status_Code (*Write_Read_Raw_Function)(
         Display_Handle * dh,
         DDC_Packet *     request_packet_ptr,
         int              max_read_bytes,
         Byte *           readbuf,
         int *            pbytes_received
        );


//
// Write and read operations that take DDC_Packets
//

/* Writes a DDC request packet to an open I2C bus
 * and returns the raw response.
 *
 * Arguments:
 *   dh               display handle for open I2C bus
 *   request_packet_ptr   DDC packet to write
 *   max_read_bytes   maximum number of bytes to read
 *   readbuf          where to return response
 *   pbytes_received  where to write count of bytes received
 *                    (always equal to max_read_bytes
 *
 * Returns:
 *   0 if success
 *   modulated(-errno) if error in write
 *   DDCRC_READ_ALL_ZERO
 */
Global_Status_Code ddc_i2c_write_read_raw(
         Display_Handle * dh,
         DDC_Packet *     request_packet_ptr,
         int              max_read_bytes,
         Byte *           readbuf,
         int *            pbytes_received
        )
{
   bool debug = false;
   // Trace_Group tg = TRACE_GROUP;
   // if (debug)
   //    tg = 0xff;
   // TRCMSGTG(tg, "Starting. dh=%s, readbuf=%p", display_handle_repr(dh), readbuf);
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, readbuf=%p",
                              display_handle_repr(dh), readbuf);
   // DBGMSG("request_packet_ptr=%p", request_packet_ptr);
   // dump_packet(request_packet_ptr);
   ASSERT_DISPLAY_IO_MODE(dh, DDC_IO_DEVI2C);

#ifdef TEST_THAT_DIDNT_WORK
   bool single_byte_reads = false;   // doesn't work
#endif

   Global_Status_Code rc =
         invoke_i2c_writer(
                           dh->fh,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   DBGMSF(debug, "invoke_i2c_writer() returned %d\n", rc);
   if (rc == 0) {
      call_tuned_sleep_i2c(SE_WRITE_TO_READ);
#if TEST_THAT_DIDNT_WORK
      if (single_byte_reads)  // fails
         rc = invoke_single_byte_i2c_reader(dh->fh, max_read_bytes, readbuf);
      else
#endif
         rc = invoke_i2c_reader(dh->fh, max_read_bytes, readbuf);
      // try adding to see if improves capabilities read for P2411H
      call_tuned_sleep_i2c(SE_POST_READ);
      // note_io_event(IE_READ_AFTER_WRITE, __func__);
      if (rc == 0 && all_zero(readbuf, max_read_bytes)) {
         rc = DDCRC_READ_ALL_ZERO;
         // printf("(%s) All zero response.", __func__ );
         // DBGMSG("Request was: %s",
         //        hexstring(get_packet_start(request_packet_ptr)+1, get_packet_len(request_packet_ptr)-1));
         // COUNT_STATUS_CODE(rc);
         DDCMSG("All zero response detected in %s", __func__);
      }
   }
   if (rc < 0) {
      COUNT_STATUS_CODE(rc);
   }

   // TRCMSGTG(tg, "Done. gsc=%s", gsc_desc(rc));
   DBGTRC(debug, TRACE_GROUP, "Done. gsc=%s", gsc_desc(rc));
   return rc;
}


/* Writes a DDC request packet to an ADL display,
 * and returns the raw response.
 *
 * Arguments:
 *   dh               display handle ADL device
 *   request_packet_ptr   DDC packet to write
 *   max_read_bytes   maximum number of bytes to read
 *   readbuf          where to return response
 *   pbytes_received  where to write count of bytes received
 *
 * Returns:
 *   0 if success
 *   modulated ADL status code otherwise
 *
 *   Negative ADL status codes indicate errors
 *   Positive values indicate success but with
 *   additional information.  Never seen.  How to handle?
 */

Global_Status_Code ddc_adl_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            pbytes_received
     )
{
   bool debug = false;
   // bool tf = IS_TRACING();
   // if (debug) tf = true;
   // TRCMSGTF(tf, "Starting. Using adl_ddc_write_only() and adl_ddc_read_only() dh=%s",
   //          display_handle_repr(dh));
   DBGTRC(debug, TRACE_GROUP,
          "Starting. Using adl_ddc_write_only() and adl_ddc_read_only() dh=%s",
          display_handle_repr(dh));
   ASSERT_DISPLAY_IO_MODE(dh, DDC_IO_ADL);

   Global_Status_Code gsc = adlshim_ddc_write_only(
                               dh,
                               get_packet_start(request_packet_ptr),   // n. no adjustment, unlike i2c version
                               get_packet_len(request_packet_ptr)
                              );
   if (gsc < 0) {
      // TRCMSGTF(tf, "adl_ddc_write_only() returned gsc=%d\n", gsc);
      DBGTRC(debug, TRACE_GROUP, "adl_ddc_write_only() returned gsc=%d\n", gsc);
   }
   else {
      call_tuned_sleep_adl(SE_WRITE_TO_READ);
      gsc = adlshim_ddc_read_only(
            dh,
            readbuf,
            pbytes_received);
      // note_io_event(IE_READ_AFTER_WRITE, __func__);
      if (gsc < 0) {
         // TRCMSGTF(tf, "adl_ddc_read_only() returned adlrc=%d\n", gsc);
         DBGTRC(debug, TRACE_GROUP, "adl_ddc_read_only() returned adlrc=%d\n", gsc);
      }
      else {
         if ( all_zero(readbuf+1, max_read_bytes-1)) {
                 gsc = DDCRC_READ_ALL_ZERO;
                 DBGTRC(debug, TRACE_GROUP, "All zero response.");
                 DDCMSG("All zero response.");
                 COUNT_STATUS_CODE(gsc);
         }
         else if (memcmp(get_packet_start(request_packet_ptr), readbuf, get_packet_len(request_packet_ptr)) == 0) {
            // DBGMSG("Bytes read same as bytes written." );
            // is this a DDC error or a programming bug?
            DDCMSG("Bytes read same as bytes written.", __func__ );
            gsc = DDCRC_READ_EQUALS_WRITE;
            COUNT_STATUS_CODE(gsc);
         }
         else {
            gsc = 0;
         }
      }
   }

   if (gsc < 0)
      log_status_code(gsc, __func__);
   // TRCMSGTF(tf, "Done. rc=%s\n", gsc_desc(gsc));
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%s\n", gsc_desc(gsc));
   return gsc;
}


Global_Status_Code ddc_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            pbytes_received
     )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, readbuf=%p",
                              display_handle_repr(dh), readbuf);
   Global_Status_Code rc;

   assert(dh->io_mode == DDC_IO_DEVI2C || dh->io_mode == DDC_IO_ADL);
   if (dh->io_mode == DDC_IO_DEVI2C) {
        rc =  ddc_i2c_write_read_raw(
              dh,
              request_packet_ptr,
              max_read_bytes,
              readbuf,
              pbytes_received
       );
   }
   else {
      rc =  ddc_adl_write_read_raw(
              dh,
              request_packet_ptr,
              max_read_bytes,
              readbuf,
              pbytes_received
       );
   }

   DBGMSF(debug, "Done, returning: %s", gsc_desc(rc));
   return rc;
}


/* Writes a DDC request packet to a monitor and provides basic response
 * parsing based whether the response type is continuous, non-continuous,
 * or table.
 *
 * Arguments:
 *   dh                  display handle (for either I2C or ADL device)
 *   request_packet_ptr  DDC packet to write
 *   max_read_bytes      maximum number of bytes to read
 *   expected_response_type expected response type to check for
 *   expected_subtype    expected subtype to check for
 *   readbuf          where to return response
 *   response_packet_ptr_loc  where to write address of response packet received
 *
 * Returns:
 *   0 if success (or >= 0?)
 *   < 0 if error
 *   modulated ADL status code otherwise
 *
 *   Issue: positive ADL codes, need to handle?
 */
Global_Status_Code ddc_write_read(
      Display_Handle * dh,
      DDC_Packet *  request_packet_ptr,
      int           max_read_bytes,
      Byte          expected_response_type,
      Byte          expected_subtype,
      DDC_Packet ** response_packet_ptr_loc
     )
{
   bool debug = false;  // override
   // bool tf = IS_TRACING();
   // if (debug) tf = 0xff;
   // TRCMSGTF(tf, "Starting. io dh=%s", display_handle_repr(dh) );
   DBGTRC(debug, TRACE_GROUP, "Starting. io dh=%s", display_handle_repr(dh) );

   Byte * readbuf = calloc(1, max_read_bytes);
   int    bytes_received = max_read_bytes;
   Global_Status_Code    rc;
   *response_packet_ptr_loc = NULL;

   rc =  ddc_write_read_raw(
            dh,
            request_packet_ptr,
            max_read_bytes,
            readbuf,
            &bytes_received
     );

   if (rc >= 0) {
       // readbuf[0] = 0x6e;
       // hex_dump(readbuf, bytes_received+1);
       rc = create_ddc_typed_response_packet(
              readbuf,
              bytes_received,
              expected_response_type,
              expected_subtype,
              __func__,
              response_packet_ptr_loc);
       DBGTRC(debug, TRACE_GROUP,
              "create_ddc_typed_response_packet() returned %s, *response_packet_ptr_loc=%p",
              ddcrc_desc(rc), *response_packet_ptr_loc );
       // TRCMSGTF(tf, "create_ddc_typed_response_packet() returned %s, *response_packet_ptr_loc=%p",
       //          ddcrc_desc(rc), *response_packet_ptr_loc );

       if (rc != 0 && *response_packet_ptr_loc) {  // paranoid,  should never occur
          free(*response_packet_ptr_loc);
          *response_packet_ptr_loc = NULL;
       }
   }

   free(readbuf);    // or does response_packet_ptr_loc point into here?

   // already done
   // if (rc != 0) {
   //    COUNT_STATUS_CODE(rc);
   // }
   // TRCMSGTF(tf, "Done. rc=%d: %s\n", rc, gsc_desc(rc) );
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%d: %s\n", rc, gsc_desc(rc) );
   // if (rc == 0 && tf)
   if (rc == 0 && (IS_TRACING() || debug) )
      dump_packet(*response_packet_ptr_loc);

   return rc;
}


/* Wraps ddc_write_read() in retry logic.
 *
 * Arguments:
 *   dh                  display handle (for either I2C or ADL device)
 *   request_packet_ptr  DDC packet to write
 *   max_read_bytes      maximum number of bytes to read
 *   expected_response_type expected response type to check for
 *   expected_subtype    expected subtype to check for
 *   response_packet_ptr_loc  where to write address of response packet received
 *
 * Returns:
 *   0 if success (or >= 0?)
 *   < 0 if error
 *
 *   Issue: positive ADL codes, need to handle?
 *
 * The maximum number of tries is set in global variable max_write_read_exchange_tries.
 */
Global_Status_Code ddc_write_read_with_retry(
         Display_Handle * dh,
         DDC_Packet *  request_packet_ptr,
         int           max_read_bytes,
         Byte          expected_response_type,
         Byte          expected_subtype,
         bool          all_zero_response_ok,
         DDC_Packet ** response_packet_ptr_loc
        )
{
   bool debug = false;
   // bool tf = IS_TRACING();
   // if (debug) tf = 0xff;
   // TRCMSGTF(tf, "Starting. dh=%s", display_handle_repr(dh)  );
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s", display_handle_repr(dh)  );
   assert(dh->io_mode != USB_IO);

   Global_Status_Code  gsc;
   int  tryctr;
   bool retryable;
   int  ddcrc_read_all_zero_ct = 0;

   for (tryctr=0, gsc=-999, retryable=true;
        tryctr < max_write_read_exchange_tries && gsc < 0 && retryable;
        tryctr++)
   {
      DBGMSF(debug,
           "Start of try loop, tryctr=%d, max_write_read_echange_tries=%d, rc=%d, retryable=%d",
           tryctr, max_write_read_exchange_tries, gsc, retryable );

      gsc = ddc_write_read(
                dh,
                request_packet_ptr,
                max_read_bytes,
                expected_response_type,
                expected_subtype,
                response_packet_ptr_loc);

      if (gsc < 0) {     // n. ADL status codes have been modulated
         DBGMSF(debug, "perform_ddc_write_read() returned %d", gsc );
         if (dh->io_mode == DDC_IO_DEVI2C) {
            if (gsc == DDCRC_NULL_RESPONSE)
               retryable = false;
            // when is DDCRC_READ_ALL_ZERO actually an error vs the response of the monitor instead of NULL response?
            // On Dell monitors (P2411, U3011) all zero response occurs on unsupported Table features
            // But also seen as a bad response
            else if ( gsc == DDCRC_READ_ALL_ZERO)
               retryable = (all_zero_response_ok) ? false : true;

            else if (gsc == modulate_rc(-EIO, RR_ERRNO))
                retryable = true;

            else if (gsc == modulate_rc(-EBADF, RR_ERRNO))
               retryable = false;

            else
               retryable = true;     // for now
         }
         else {   // DDC_IO_ADL
            // TODO more detailed tests
            if (gsc == DDCRC_NULL_RESPONSE)
               retryable = false;
            else if (gsc == DDCRC_READ_ALL_ZERO)
               retryable = true;
            else
               retryable = false;
         }
         if (gsc == DDCRC_READ_ALL_ZERO)
            ddcrc_read_all_zero_ct++;
      }    // rc < 0
   }
   // n. rc is now the value from the last pass through the loop
   // set it to a DDC status code indicating max tries exceeded
   if ( gsc < 0 && retryable ) {
      gsc = DDCRC_RETRIES;
      if (ddcrc_read_all_zero_ct == max_write_read_exchange_tries) {
         gsc = DDCRC_ALL_TRIES_ZERO;
         // printf("(%s) All tries zero ddcrc_read_all_zero_ct=%d, max_write_read_exchange_tries=%d, tryctr=%d\n",
         //        __func__, ddcrc_read_all_zero_ct, max_write_read_exchange_tries, tryctr);
      }
      COUNT_STATUS_CODE(gsc);
   }
   try_data_record_tries(write_read_stats_rec, gsc, tryctr);
   // TRCMSGTF(tf, "Done. rc=%s\n", gsc_desc(gsc));
   DBGTRC(debug, TRACE_GROUP, "Done. gsc=%s\n", gsc_desc(gsc));
   return gsc;
}


/* Writes a DDC request packet to an open I2C bus.
 *
 * Arguments:
 *   fh                  Linux file handle for open I2C bus
 *   request_packet_ptr  DDC packet to write
 *
 * Returns:
 *   0 if success
 *   -errno if error
 */
Global_Status_Code ddc_i2c_write_only(
         int           fh,
         DDC_Packet *  request_packet_ptr
        )
{
   bool debug = false;
   // bool tf = IS_TRACING();
   // tf = true;
   // TRCMSGTF(tf, "Starting.");
   DBGTRC(debug, TRACE_GROUP, "Starting.");

   Global_Status_Code rc = invoke_i2c_writer(fh,
                              get_packet_len(request_packet_ptr)-1,
                              get_packet_start(request_packet_ptr)+1 );
   if (rc < 0)
      log_status_code(rc, __func__);
   call_tuned_sleep_i2c(SE_POST_WRITE);
   // TRCMSGTF(tf, "Done. rc=%d\n", rc);
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%d\n", rc);
   return rc;
}


/* Writes a DDC request packet to a monitor
 *
 * Arguments:
 *   dh                  Display_Handle for open I2C or ADL device
 *   request_packet_ptr  DDC packet to write
 *
 * Returns:
 *   0 if success
 *   < 0 if error
 */
Global_Status_Code ddc_write_only( Display_Handle * dh, DDC_Packet *   request_packet_ptr) {
   bool debug = false;
   // bool tf = IS_TRACING();
   // tf = true;
   // TRCMSGTF(tf, "Starting.");
   DBGTRC(debug, TRACE_GROUP, "Starting.");

   Global_Status_Code rc = 0;
   assert(dh->io_mode != USB_IO);
   if (dh->io_mode == DDC_IO_DEVI2C) {
      rc = ddc_i2c_write_only(dh->fh, request_packet_ptr);
   }
   else {
      rc = adlshim_ddc_write_only(
              dh,
              get_packet_start(request_packet_ptr),
              get_packet_len(request_packet_ptr)
              // get_packet_start(request_packet_ptr)+1,
              // get_packet_len(request_packet_ptr)-1
             );
   }

   // TRCMSGTF(tf, "Done. rc=%d\n", rc);
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%d\n", rc);
   return rc;
}


/* Wraps ddc_write_only() in retry logic.
 *
 * Arguments:
 *   dh                  display handle (for either I2C or ADL device)
 *   request_packet_ptr  DDC packet to write
 *
 * Returns:
 *   0 if success
 *   DDCRC_RETRIES if maximum try count exceeded
 *
 *  The maximum number of tries allowed has been set in global variable
 *  max_write_only_exchange_tries.
 */
Global_Status_Code
ddc_write_only_with_retry( Display_Handle * dh, DDC_Packet *   request_packet_ptr) {
   bool debug = false;
   // bool tf = IS_TRACING();
   // tf = false;
   // TRCMSGTF(tf, "Starting.");
   DBGTRC(debug, TRACE_GROUP, "Starting.");

   assert(dh->io_mode != USB_IO);

   Global_Status_Code rc;
   int  tryctr;
   bool retryable;

   for (tryctr=0, rc=-999, retryable=true;
       tryctr < max_write_only_exchange_tries && rc < 0 && retryable;
       tryctr++)
   {
      DBGMSF(debug,
             "Start of try loop, tryctr=%d, max_write_only_exchange_tries=%d, rc=%d, retryable=%d",
             tryctr, max_write_only_exchange_tries, rc, retryable );

      rc = ddc_write_only(dh, request_packet_ptr);

      if (rc < 0) {
         if (dh->io_mode == DDC_IO_DEVI2C) {
            if (rc < 0) {
               if (rc != modulate_rc(-EIO, RR_ERRNO) )
                   retryable = false;
            }
         }
         else {
            if (rc < 0) {
                // no logic in ADL case to test for continuing to retry, should there be ???
                // is it even meaningful to retry for ADL?
                   // retryable = true;    // *** TEMP ***
            }
         }
      }   // rc < 0
   }
   if (rc < 0 && retryable)
      rc = DDCRC_RETRIES;
   try_data_record_tries(write_only_stats_rec, rc, tryctr);

   // TRCMSGTF(tf, "Done. rc=%d", rc);
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%d", rc);
   return rc;
}

