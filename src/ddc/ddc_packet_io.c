/** \file ddc_packet_io.c
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.  Handles I2C bus retry.
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// N. ddc_open_display() and ddc_close_display() handle case USB, but the
// packet functions are for I2C and ADL only.  Consider splitting.

/** \cond */
#include <config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_types.h"

#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/utilrpt.h"
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/displays.h"
#include "base/dynamic_sleep.h"
#include "base/execution_stats.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"
#include "base/thread_sleep_data.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_strategy_dispatcher.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_display_lock.h"
#include "ddc/ddc_try_stats.h"

#include "ddc/ddc_packet_io.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

static GHashTable * open_displays = NULL;

#ifdef DEPRECATED
// Deprecated - use all_bytes_zero() in string_util.c
// Tests if a range of bytes is entirely 0
bool all_zero(Byte * bytes, int bytect) {
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
#endif

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


bool
ddc_is_valid_display_handle(Display_Handle * dh) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%p", dh);
   assert(open_displays);
   bool result = g_hash_table_contains(open_displays, dh);
   DBGTRC(debug, TRACE_GROUP, "Done.     dh=%p, returning %s", dh, sbool(result));
   return result;
}


void ddc_dbgrpt_valid_display_handles(int depth) {
   rpt_vstring(depth, "Valid display handle = open_displays:");
   assert(open_displays);
   GList * display_handles = g_hash_table_get_keys(open_displays);
   if (g_list_length(display_handles) > 0) {
      for (GList * cur = display_handles; cur; cur = cur->next) {
         Display_Handle * dh = cur->data;
         rpt_vstring(depth+1, "%p -> %s", dh, dh_repr_t(dh));
      }
   }
   else {
      rpt_vstring(depth+1, "None");
   }
   g_list_free(display_handles);
}


//
// Open/Close Display
//

/** Opens a DDC display.
 *
 *  \param  dref            display reference
 *  \param  callopts        call option flags
 *  \param  dh_loc          address at which to return display handle
 *  \return status code     as from #i2c_open_bus(), #usb_open_hiddev_device()
 *  \retval DDCRC_LOCKED    display open in another thread
 *  \retval DDCRC_ALREADY_OPEN display already open in current thread
 *  \retval -EBUSY          from i2c_set_addr()
 *
 *  **Call_Option** flags recognized:
 *  - CALLOPT_WAIT
 *  - CALLOPT_ERR_MSG
 */
DDCA_Status
ddc_open_display(
      Display_Ref *    dref,
      Call_Options     callopts,
      Display_Handle** dh_loc)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Opening display %s, callopts=%s, dh_loc=%p",
                 dref_repr_t(dref), interpret_call_options_t(callopts), dh_loc );
   TRACED_ASSERT(dh_loc);
   // TRACED_ASSERT(1==5);    // for testing

   Display_Handle * dh = NULL;
   DDCA_Status ddcrc = 0;

   Distinct_Display_Ref ddisp_ref = get_distinct_display_ref(dref);
   Distinct_Display_Flags ddisp_flags = DDISP_NONE;
   if (callopts & CALLOPT_WAIT)
      ddisp_flags |= DDISP_WAIT;

   DDCA_Status lockrc = lock_distinct_display(ddisp_ref, ddisp_flags);
   if (lockrc == DDCRC_LOCKED) {     // locked in another thread
      ddcrc = DDCRC_LOCKED;          // is there an appropriate errno value?  EBUSY? EACCES?
      goto bye;
   }
   // DBGMSF(debug, "lockrc = %s, DREF_OPEN = %s", psc_desc(lockrc), sbool(dref->flags&DREF_OPEN));
   // assumes there is only one Display_Ref for a display
   // DREF_OPEN flag will not be set if caller used a different Display_Ref on this open call
   // TRACED_ASSERT_IFF( ddcrc == DDCRC_ALREADY_OPEN, dref->flags & DREF_OPEN);

  //  if (dref->flags & DREF_OPEN) {
  //     ddcrc = DDCRC_ALREADY_OPEN;
  //     goto bye;
  //  }

   if (lockrc == DDCRC_ALREADY_OPEN) {
      ddcrc = DDCRC_ALREADY_OPEN;
      goto bye;
   }

   switch (dref->io_path.io_mode) {

   case DDCA_IO_I2C:
      {
         int fd = i2c_open_bus(dref->io_path.path.i2c_busno, callopts);
         if (fd < 0) {
            ddcrc = fd;
         }
         else {
            // DBGMSF(debug, "Calling set_addr(0x37) for %s", dref_repr_t(dref));
            ddcrc =  i2c_set_addr(fd, 0x37, callopts);
            if (ddcrc != 0) {
               TRACED_ASSERT(ddcrc < 0);
               close(fd);
            }

            else {
               // Is this needed?
               // 10/24/15, try disabling:
               // sleepMillisWithTrace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);

               dh = create_bus_display_handle_from_display_ref(fd, dref);    // n. sets dh->dref = dref

               I2C_Bus_Info * bus_info = dref->detail;
               TRACED_ASSERT(bus_info);   // need to convert to a test?
               TRACED_ASSERT( memcmp(bus_info, I2C_BUS_INFO_MARKER, 4) == 0);

               dref->pedid = bus_info->edid;
               if (!dref->pedid) {
                  // How is this even possible?
                  // 1/2017:  Observed with x260 laptop and Ultradock, See ddcutil user report.
                  //          close(fd) fails
                  DBGMSG("No EDID for device on bus /dev/"I2C"-%d", dref->io_path.path.i2c_busno);
                  close(fd);
                  ddcrc = DDCRC_EDID;
                  free_display_handle(dh);
                  dh = NULL;
               }
            }
         }
      }
      break;

   case DDCA_IO_ADL:
      PROGRAM_LOGIC_ERROR("Case DDCA_IO_ADL");
      break;

   case DDCA_IO_USB:
#ifdef USE_USB
      {
         DBGTRC(debug, TRACE_GROUP, "Opening USB device: %s", dref->usb_hiddev_name);
         TRACED_ASSERT(dref->usb_hiddev_name);
         // if (!dref->usb_hiddev_name) { // HACK
         //    DBGMSG("HACK FIXUP.  dref->usb_hiddev_name");
         //    dref->usb_hiddev_name = get_hiddev_devname_by_dref(dref);
         // }
         int fd = usb_open_hiddev_device(dref->usb_hiddev_name, callopts);
         if (fd < 0) {
            ddcrc = fd;
         }
         else {
            dh = create_usb_display_handle_from_display_ref(fd, dref);
            dref->pedid = usb_get_parsed_edid_by_dh(dh);
         }
      }
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
      assert(false);   // avoid coverity error re null dreference
#endif
      break;
   } // switch
   TRACED_ASSERT(!dh || dh->dref->pedid);

   if (ddcrc == 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         TUNED_SLEEP_WITH_TRACE(dh, SE_POST_OPEN, NULL);
      dref->flags |= DREF_OPEN;
      // protect with lock?
      TRACED_ASSERT(open_displays);
      g_hash_table_add(open_displays, dh);
   }
   else {
      unlock_distinct_display(ddisp_ref);
   }

bye:
   if (ddcrc != 0) {
      COUNT_STATUS_CODE(ddcrc);
   }
   *dh_loc = dh;
   TRACED_ASSERT(ddcrc <= 0);
   TRACED_ASSERT( (ddcrc == 0 && *dh_loc) || (ddcrc < 0 && !*dh_loc) );
   // dbgrpt_distinct_display_descriptors(0);
   DBGTRC(debug, TRACE_GROUP, "Done.     Returning: %s, *dh_loc=%s", psc_desc(ddcrc), dh_repr_t(*dh_loc));
   return ddcrc;
}


/** Closes a DDC display.
 *
 *  \param  dh            display handle
 *  \return 0 if success, or -errno if error
 *
 *  \remark
 *  Logs underlying status code if error.
 */
Status_Errno
ddc_close_display(Display_Handle * dh) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, dref=%s, fd=%d, dpath=%s",
              dh_repr_t(dh), dref_repr_t(dh->dref), dh->fd, dpath_short_name_t(&dh->dref->io_path) ) ;
   Display_Ref * dref = dh->dref;
   Status_Errno rc = 0;
   if (dh->fd == -1) {
      rc = DDCRC_INVALID_OPERATION;    // or DDCRC_ARG?
   }
   else {
      switch(dh->dref->io_path.io_mode) {
      case DDCA_IO_I2C:
         {
            rc = i2c_close_bus(dh->fd, CALLOPT_NONE);
            if (rc != 0) {
               TRACED_ASSERT(rc < 0);
               DBGMSG("i2c_close_bus returned %d, errno=%s", rc, psc_desc(errno) );
               COUNT_STATUS_CODE(rc);
            }
            dh->fd = -1;    // indicate invalid, in case we try to continue using dh
            break;
         }
      case DDCA_IO_ADL:
         break;           // nothing to do

      case DDCA_IO_USB:
#ifdef USE_USB
         {
            rc = usb_close_device(dh->fd, dh->dref->usb_hiddev_name, CALLOPT_NONE); // return error if failure
            if (rc != 0) {
               TRACED_ASSERT(rc < 0);
               DBGMSG("usb_close_device returned %d", rc);
               COUNT_STATUS_CODE(rc);
            }
            dh->fd = -1;
            break;
         }
#else
         PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      } //switch
   }

   dh->dref->flags &= (~DREF_OPEN);
   Distinct_Display_Ref display_id = get_distinct_display_ref(dh->dref);
   unlock_distinct_display(display_id);
   assert(open_displays);
   g_hash_table_remove(open_displays, dh);

   free_display_handle(dh);
   DBGTRC(debug, TRACE_GROUP, "Done.     dref=%s  Returning: %s", dref_repr_t(dref), psc_desc(rc));
   return rc;
}

void ddc_close_all_displays() {
   assert(open_displays);
   GList * display_handles = g_hash_table_get_keys(open_displays);
   for (GList * cur = display_handles; cur; cur = cur->next) {
      Display_Handle * dh = cur->data;
      ddc_close_display(dh);
   }
   // open_displays should be empty at this point
   TRACED_ASSERT(g_hash_table_size(open_displays) == 0);
}



// work in progress

// typedef for ddc_i2c_write_read_raw, ddc_adl_write_read_raw, ddc_write_read_raw


typedef
DDCA_Status (*Write_Read_Raw_Function)(
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
 *   -errno if error in write
 *   DDCRC_READ_ALL_ZERO
 */
// static  // allow function to appear in backtrace
DDCA_Status ddc_i2c_write_read_raw(
         Display_Handle * dh,
         DDC_Packet *     request_packet_ptr,
         bool             read_bytewise,
         int              max_read_bytes,
         Byte *           readbuf,
         int *            pbytes_received
        )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, readbuf=%p",dh_repr_t(dh), readbuf);
   // DBGMSG("request_packet_ptr=%p", request_packet_ptr);
   // dump_packet(request_packet_ptr);

   DBGTRC(debug, TRACE_GROUP, "request_packet_ptr->raw_bytes: %s",
                              hexstring3_t(request_packet_ptr->raw_bytes->bytes,
                                           request_packet_ptr->raw_bytes->len,
                                           " ", 1, false) );
   TRACED_ASSERT(dh);
   TRACED_ASSERT(dh->dref);
   TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

#ifdef TEST_THAT_DIDNT_WORK
   bool single_byte_reads = false;   // doesn't work
#endif

   Byte slave_addr = request_packet_ptr->raw_bytes->bytes[0];      // 0x6e
   TRACED_ASSERT(slave_addr >> 1 == 0x37);

   CHECK_DEFERRED_SLEEP(dh);
   Status_Errno_DDC rc =
         invoke_i2c_writer(
                           dh->fd,
                           0x37,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   DBGMSF(debug, "invoke_i2c_writer() returned %d", rc);
   if (rc == 0) {
      TUNED_SLEEP_WITH_TRACE(dh, SE_WRITE_TO_READ, NULL);
      // tuned_sleep_i2c_with_trace(SE_WRITE_TO_READ, __func__, NULL);

      // ALTERNATIVE_THAT_DIDNT_WORK:
      // if (single_byte_reads)  // fails
      //    rc = invoke_single_byte_i2c_reader(dh->fd, max_read_bytes, readbuf);
      // else

      CHECK_DEFERRED_SLEEP(dh);
      rc = invoke_i2c_reader(dh->fd, 0x37, read_bytewise, max_read_bytes, readbuf);
      // try adding sleep to see if improves capabilities read for P2411H
      // tuned_sleep_i2c_with_trace(SE_POST_READ, __func__, NULL);
      TUNED_SLEEP_WITH_TRACE(dh, SE_POST_READ, NULL);
      if (rc == 0)
         DBGTRC(debug, TRACE_GROUP, "Response bytes: %s",
                                hexstring3_t(readbuf, max_read_bytes, " ", 1, false) );

      if (rc == 0 && all_bytes_zero(readbuf, max_read_bytes)) {
         DDCMSG(debug, "All zero response detected in %s", __func__);
         rc = DDCRC_READ_ALL_ZERO;
         // printf("(%s) All zero response.", __func__ );
         // DBGMSG("Request was: %s",
         //        hexstring(get_packet_start(request_packet_ptr)+1, get_packet_len(request_packet_ptr)-1));
      }
   }
   if (rc < 0) {
      COUNT_STATUS_CODE(rc);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.    psc=%s", psc_desc(rc));
   return rc;
}

// TODO: eliminate this function, used to route I2C vs ADL calls
// static  // allow function to appear in backtrace
DDCA_Status ddc_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      bool             read_bytewise,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            p_rcvd_bytes_ct
     )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, readbuf=%p, max_read_bytes=%d",
                              dh_repr_t(dh), readbuf, max_read_bytes);
   if (debug) {
      // DBGMSG("request_packet_ptr->raw_bytes:");
      // dbgrpt_buffer(request_packet_ptr->raw_bytes, 1);
      char * s =  hexstring3_t(request_packet_ptr->raw_bytes->bytes,
                              request_packet_ptr->raw_bytes->len, " ", 1, false );
      DBGMSG("request_packet_ptr->raw_bytes: %s", s);
   }

   // This function should not be called for USB
   TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

   DDCA_Status psc =  ddc_i2c_write_read_raw(
                 dh,
                 request_packet_ptr,
                 read_bytewise,
                 max_read_bytes,
                 readbuf,
                 p_rcvd_bytes_ct
              );

   DBGTRC(debug, TRACE_GROUP, "Done.     Returning: %s", psc_desc(psc));
   if (psc == 0) {
      DBGTRC(debug, TRACE_GROUP,
             "      readbuf: %s",
             hexstring3_t(readbuf, *p_rcvd_bytes_ct, " ", 4, false));
   }
   return psc;
}


/** Writes a DDC request packet to a monitor and provides basic response parsing
 *  based whether the response type is continuous, non-continuous, or table.
 *
 *  \param dh                  display handle (for either I2C or ADL device)
 *  \param request_packet_ptr  DDC packet to write
 *  \param max_read_bytes      maximum number of bytes to read
 *  \param expected_response_type expected response type to check for
 *  \param expected_subtype    expected subtype to check for
 *  \param response_packet_ptr_loc  where to write address of response packet received
 *
 *  \return pointer to #Error_Info struct if failure, NULL if success
 *  \remark
 *  Issue: positive ADL codes, need to handle?
 */
Error_Info *
ddc_write_read(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      bool             read_bytewise,
      int              max_read_bytes,
      Byte             expected_response_type,
      Byte             expected_subtype,
      DDC_Packet **    response_packet_ptr_loc
     )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, read_bytewise=%s, max_read_bytes=%d",
                 dh_repr_t(dh), sbool(read_bytewise), max_read_bytes );

   Byte * readbuf = calloc(1, max_read_bytes);
   int    bytes_received = max_read_bytes;
   DDCA_Status    psc;
   *response_packet_ptr_loc = NULL;

   psc =  ddc_write_read_raw(
            dh,
            request_packet_ptr,
            read_bytewise,
            max_read_bytes,
            readbuf,
            &bytes_received
     );
   if (psc >= 0) {
       // readbuf[0] = 0x6e;
       // hex_dump(readbuf, bytes_received+1);
       psc = create_ddc_typed_response_packet(
              readbuf,
              bytes_received,
              expected_response_type,
              expected_subtype,
              __func__,
              response_packet_ptr_loc);
       DBGTRC(debug, TRACE_GROUP,
              "create_ddc_typed_response_packet() returned %s, *response_packet_ptr_loc=%p",
              ddcrc_desc_t(psc), *response_packet_ptr_loc );

       if (psc != 0 && *response_packet_ptr_loc) {  // paranoid,  should never occur
          free(*response_packet_ptr_loc);
          *response_packet_ptr_loc = NULL;
       }
   }
   dsa_record_ddcrw_status_code(psc);

   free(readbuf);    // or does response_packet_ptr_loc point into here?

   // already done:
   // if (rc != 0)
   //    COUNT_STATUS_CODE(psc);

   // Convert status code to Error_Info *
   Error_Info * excp = NULL;
   if (psc < 0)
      excp = errinfo_new(psc, __func__);

   if (debug || IS_TRACING()) {
      if (excp) {
         DBGMSG("Done.     Returning: %s", errinfo_summary(excp)  );
      }
      else {
         DBGMSG("Done.     Returning: NULL, *response_packet_ptr_loc ->");
         dbgrpt_packet(*response_packet_ptr_loc, 3);
      }
   }

   return excp;
}


/** Wraps #ddc_write_read() in retry logic.
 *
 *  \param dh                  display handle (for either I2C or ADL device)
 *  \param request_packet_ptr  DDC packet to write
 *  \param max_read_bytes      maximum number of bytes to read
 *  \param expected_response_type expected response type to check for
 *  \param expected_subtype    expected subtype to check for
 *  \param all_zero_response_ok treat a response of all 0s as valid
 *  \param response_packet_ptr_loc  where to write address of response packet received
 *
 *  \return pointer to #Error_Info struct if failure, NULL if success
 *
 *  \remark
 *  status code from #ddc_write_read() may be positive for positive ADL status code ??
 *            status code from #ddc_write_read() if exactly 1 pass through try loop\n
 *            DDCRC_RETRIES, DDCRC_ALL_TRIES_ZERO, DDCRC_ALL_RESPONES_NULL if maximum tries exceeded
 *
 *\remark
 * Issue: positive ADL codes, need to handle?
 * \remark
 * The maximum number of tries is set in global variable max_write_read_exchange_tries.
 */
Error_Info *
ddc_write_read_with_retry(
         Display_Handle * dh,
         DDC_Packet *     request_packet_ptr,
         int              max_read_bytes,
         Byte             expected_response_type,
         Byte             expected_subtype,
         bool             all_zero_response_ok,
         DDC_Packet **    response_packet_ptr_loc
        )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, all_zero_response_ok=%s",
          dh_repr_t(dh), sbool(all_zero_response_ok)  );
   TRACED_ASSERT(dh->dref->io_path.io_mode != DDCA_IO_USB);
   // show_backtrace(1);

   // if (debug)
   //     dbgrpt_display_ref(dh->dref, 1);

   bool retry_null_response = !(dh->dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED);

   DDCA_Status  psc;
   bool read_bytewise = I2C_Read_Bytewise;   // normally set to DEFAULT_I2C_READ_BYTEWISE
   int  tryctr;
   bool retryable;
   int  ddcrc_read_all_zero_ct = 0;
   int  ddcrc_null_response_ct = 0;
   int  ddcrc_null_response_max = (retry_null_response) ? 3 : 0;
   bool sleep_multiplier_incremented = false;
   // ddcrc_null_response_max = 6;  // *** TEMP *** for testing
   DBGMSF(debug, "retry_null_response = %s, ddcrc_null_response_max = %d",
          sbool(retry_null_response), ddcrc_null_response_max);
   Error_Info * try_errors[MAX_MAX_TRIES];

   // TRACED_ASSERT(max_write_read_exchange_tries > 0);   // to avoid clang warning
   int max_tries = try_data_get_maxtries2(WRITE_READ_TRIES_OP);
   TRACED_ASSERT(max_tries >= 0);
   for (tryctr=0, psc=-999, retryable=true;
        tryctr < max_tries && psc < 0 && retryable;
        tryctr++)
   {
      DBGMSF(debug,
           "Start of try loop, tryctr=%d, max_tries=%d, rc=%d, retryable=%s, read_bytewise=%s",
           tryctr, max_tries, psc, sbool(retryable), sbool(read_bytewise) );

      Error_Info * cur_excp = ddc_write_read(
                dh,
                request_packet_ptr,
                read_bytewise,
                max_read_bytes,
                expected_response_type,
                expected_subtype,
                response_packet_ptr_loc);

      // TESTCASES:
      // if (tryctr < 2)
      // cur_excp = errinfo_new(DDCRC_NULL_RESPONSE, "dummy");
      // cur_excp = errinfo_new(-EIO, "dummy");

      psc = (cur_excp) ? cur_excp->status_code : 0;
      try_errors[tryctr] = cur_excp;

      if (psc == 0 && ddcrc_null_response_ct > 0) {
         DBGTRC(debug, TRACE_GROUP | DDCA_TRC_RETRY,
                "%s, ddc_write_read() succeeded after %d sleep and retry for DDC Null Response",
                dh_repr_t(dh),
                ddcrc_null_response_ct);
      }

      if (psc < 0) {     // n. ADL status codes have been modulated
         DBGMSF(debug, "ddc_write_read() returned %s", psc_desc(psc) );
         COUNT_RETRYABLE_STATUS_CODE(psc);

         TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

            // The problem: Does NULL response indicate an error condition, or
            // is the monitor using NULL response to indicate unsupported?
            // Acer monitor uses NULL response instead of setting the unsupported
            // flag in a valid response
            switch (psc) {
            case DDCRC_NULL_RESPONSE:
                  {
                     retryable = (++ddcrc_null_response_ct < ddcrc_null_response_max);
                     DBGMSF(debug, "DDCRC_NULL_RESPONSE, retryable=%s", sbool(retryable));
                     if (retryable) {
                        if (ddcrc_null_response_ct == 1 && get_output_level() >= DDCA_OL_VERBOSE)
                           f0printf(fout(), "Extended delay as recovery from DDC Null Response...\n");
                        tsd_set_sleep_multiplier_ct(ddcrc_null_response_ct+1);
                        sleep_multiplier_incremented = true;
                        // replaces: call_dynamic_tuned_sleep_i2c(SE_DDC_NULL, ddcrc_null_response_ct);
                     }
                  }
                  break;

            case (DDCRC_READ_ALL_ZERO):
                 // when is DDCRC_READ_ALL_ZERO actually an error vs the response of the monitor instead of NULL response?
                 // On Dell monitors (P2411, U3011) all zero response occurs on unsupported Table features
                 // But also seen as a bad response
                 retryable = (all_zero_response_ok) ? false : true;
                 break;

            case (-EIO):
                 // retryable = false;     // ??
                 break;

            case (-EBADF):
                 // DBGMSG("EBADF");
                 retryable = false;
                 break;

            case (-ENXIO):    // no such device or address, i915 driver
                 retryable = false;  // have seen success after 7 retries of errors including ENXIO, DDCRC_DATA, make retryable?
                 break;

            default:
                 retryable = true;     // for now

            // try exponential backoff on all errors, not just SE_DDC_NULL
            // if (retryable)
            //    call_dynamic_tuned_sleep_i2c(SE_DDC_NULL, tryctr+1);
                  }


         if (psc == DDCRC_READ_ALL_ZERO)
            ddcrc_read_all_zero_ct++;
      }    // rc < 0
      // DBGMSG("Bottom of try loop. psc=%d, tryctr=%d, retryable=%s", psc, tryctr, sbool(retryable));
   }
   DBGTRC(debug, DDCA_TRC_NONE, "After try loop. tryctr=%d, psc=%d, retryable=%s, read_bytewise=%s",
         tryctr, psc, sbool(retryable), sbool(read_bytewise));

   int errct = (psc == 0) ? tryctr-1 : tryctr;
   // DBGMSG("psc=%d, tryctr=%d, errct=%d", psc, tryctr, errct);

   // read_bytewise = !read_bytewise;
#ifdef OLD
   if (debug) {
      for (int ndx = 0; ndx < tryctr; ndx++) {
         DBGMSG("try_errors[ndx] = %p", try_errors[ndx]);
         DBGMSG("try_errors[%d] = %s", ndx, errinfo_summary(try_errors[ndx]));
      }
   }
#endif
   // DBGMSG("try_errors = %p, &try_errors=%p", try_errors, &try_errors);
   if (errct > 0) {
         char * s0 = (psc == 0) ? "Succeeded" : "Failed";
         char * s1 = (errct == 1) ? "" : "s";
         char * s = errinfo_array_summary(try_errors, errct);
         DBGTRC(debug, TRACE_GROUP | DDCA_TRC_RETRY, "%s after %d error%s: %s", s0, errct, s1, s);
         free(s);

   }
   if (sleep_multiplier_incremented) {
      tsd_set_sleep_multiplier_ct(1);   // in case we changed it
      tsd_bump_sleep_multiplier_changer_ct();
   }

   Error_Info * ddc_excp = NULL;

   if (psc < 0) {
      // int last_try_index = tryctr-1;
      DBGTRC(debug, TRACE_GROUP, "After try loop. tryctr=%d, retryable=%s", tryctr, sbool(retryable));

      if (retryable)
         psc = DDCRC_RETRIES;
      else if (ddcrc_read_all_zero_ct == max_tries)
         psc = DDCRC_ALL_TRIES_ZERO;
      else if (ddcrc_null_response_ct > ddcrc_null_response_max)
         psc = DDCRC_ALL_RESPONSES_NULL;

      ddc_excp = errinfo_new_with_causes(psc, try_errors, tryctr, __func__);

      if (psc != try_errors[tryctr-1]->status_code)
         COUNT_STATUS_CODE(psc);     // new status code, count it
   }
   else {
      for (int ndx = 0; ndx < tryctr-1; ndx++) {
         // errinfo_free(try_errors[ndx]);
         ERRINFO_FREE_WITH_REPORT(try_errors[ndx], debug || IS_TRACING() || report_freed_exceptions);
      }
   }

   try_data_record_tries2(WRITE_READ_TRIES_OP, psc, tryctr);

   DBGTRC(debug, TRACE_GROUP, "Done. Total Tries (tryctr): %d. Returning: %s", tryctr, errinfo_summary(ddc_excp));
   return ddc_excp;
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
static Status_Errno_DDC
ddc_i2c_write_only(
         Display_Handle * dh,
         DDC_Packet *  request_packet_ptr
        )
{
   bool debug = false;
   int fh = dh->fd;
   DBGTRC(debug, TRACE_GROUP, "Starting.");
   if (debug)
      dbgrpt_packet(request_packet_ptr, 1);

   DBGTRC(debug, TRACE_GROUP, "request_packet_ptr->raw_bytes: %s",
                              hexstring3_t(request_packet_ptr->raw_bytes->bytes,
                                           request_packet_ptr->raw_bytes->len,
                                           " ", 1, false) );

   // Byte slave_address = request_packet_ptr[0];
   // assert(slave_address == 0x37);
   Byte slave_address = 0x37;

   CHECK_DEFERRED_SLEEP(dh);
   Status_Errno_DDC rc =
         invoke_i2c_writer(fh,
                           slave_address,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   if (rc < 0)
      log_status_code(rc, __func__);
   Sleep_Event_Type sleep_type =
         (request_packet_ptr->type == DDC_PACKET_TYPE_SAVE_CURRENT_SETTINGS )
            ? SE_POST_SAVE_SETTINGS
            : SE_POST_WRITE;
   // tuned_sleep_i2c_with_trace(sleep_type, __func__, NULL);
   TUNED_SLEEP_WITH_TRACE(dh, sleep_type, NULL);
   DBGTRC(debug, TRACE_GROUP, "Done.     rc=%s", psc_desc(rc) );
   return rc;
}


/* Writes a DDC request packet to a monitor
 *
 * \param  dh                  Display_Handle for open I2C or ADL device
 * \param  request_packet_ptr  DDC packet to write
 * \return NULL if success, #Error_Info struct if error
 *
 * @todo
 * Eliminate this function, it used to route to the ADL version as
 * well as ddc_i2c_write_only()
 */
Error_Info *
ddc_write_only(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting.");

   TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

   DDCA_Status psc = ddc_i2c_write_only(dh, request_packet_ptr);
   Error_Info *  ddc_excp = NULL;
   if (psc)
      ddc_excp = errinfo_new(psc, __func__);

   DBGTRC(debug, TRACE_GROUP, "Done.     Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}


/* Wraps ddc_write_only() in retry logic.
 *
 *  \param  dh                  display handle (for either I2C or ADL device)
 *  \param  request_packet_ptr  DDC packet to write
 *  \return pointer to #Error_Info struct if failure, NULL if success
 *
 *  The maximum number of tries allowed has been set in global variable
 *  max_write_only_exchange_tries.
 */
Error_Info *
ddc_write_only_with_retry(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting." );

   TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

   DDCA_Status        psc;
   int                tryctr;
   bool               retryable;
   Error_Info *       try_errors[MAX_MAX_TRIES];

   int max_tries = try_data_get_maxtries2(WRITE_ONLY_TRIES_OP);
   TRACED_ASSERT(max_tries > 0);
   for (tryctr=0, psc=-999, retryable=true;
       tryctr < max_tries && psc < 0 && retryable;
       tryctr++)
   {
      DBGMSF(debug,
             "Start of try loop, tryctr=%d, max_tries=%d, rc=%d, retryable=%d",
             tryctr, max_tries, psc, retryable );

      Error_Info * cur_excp = ddc_write_only(dh, request_packet_ptr);
      psc = (cur_excp) ? cur_excp->status_code : 0;
      try_errors[tryctr] = cur_excp;

      // try_status_codes[tryctr] = psc;   // for future Ddc_Error mechanism
   }

   Error_Info * ddc_excp = NULL;

   if (psc < 0) {
      // now:
      //   tryctr = number of tries
      //   tryctr-1 = index of last try
      //   tryctr == max_tries &&  retryable
      //   tryctr <  max_tries && !retryable
      //   tryctr == max_tries && !retryable

      // int last_try_index = tryctr-1;
      DBGTRC(debug, TRACE_GROUP, "After try loop. tryctr=%d, retryable=%s", tryctr, sbool(retryable));

      if (retryable)
         psc = DDCRC_RETRIES;

      ddc_excp = errinfo_new_with_causes(psc, try_errors, tryctr, __func__);

      if (psc != try_errors[tryctr-1]->status_code)
         COUNT_STATUS_CODE(psc);     // new status code, count it
   }
   else {
      // 2 possibilities:
      //   succeeded after retries, there will be some errors (tryctr > 1)
      //   no errors (tryctr == 1)
      // int last_bad_try_index = tryctr-2;
      for (int ndx = 0; ndx < tryctr-1; ndx++) {
         ERRINFO_FREE_WITH_REPORT(try_errors[ndx], debug || IS_TRACING() || report_freed_exceptions);
      }
   }

   try_data_record_tries2(WRITE_ONLY_TRIES_OP, psc, tryctr);

   DBGTRC(debug, TRACE_GROUP, "Done.     Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}


static void
init_ddc_packet_io_func_name_table() {
   RTTI_ADD_FUNC(ddc_open_display);
   RTTI_ADD_FUNC(ddc_close_display);
   RTTI_ADD_FUNC(ddc_i2c_write_read_raw);
   RTTI_ADD_FUNC(ddc_i2c_write_only);
   RTTI_ADD_FUNC(ddc_write_read_raw);
   RTTI_ADD_FUNC(ddc_write_read);
   RTTI_ADD_FUNC(ddc_write_read_with_retry);
   RTTI_ADD_FUNC(ddc_write_only);
   RTTI_ADD_FUNC(ddc_write_only_with_retry);
   RTTI_ADD_FUNC(ddc_is_valid_display_handle);
}


void
init_ddc_packet_io() {
   init_ddc_packet_io_func_name_table();

   open_displays = g_hash_table_new(g_direct_hash, NULL);
}
