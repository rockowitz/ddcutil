/** \file ddc_packet_io.c
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.  Handles I2C bus retry.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/debug_util.h"
#include "util/string_util.h"
#include "util/utilrpt.h"
/** \endcond */

#include "base/ddc_errno.h"
#include "base/displays.h"
#include "base/execution_stats.h"
#include "base/parms.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_display_lock.h"
#include "ddc/ddc_try_stats.h"

#include "ddc/ddc_packet_io.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

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


//
// Open/Close Display
//

/** Opens a DDC display.
 *
 *  \param  dref            display reference
 *  \param  callopts        call option flags
 *  \param  pdh             address at which to return display handle
 *  \return status code     as from #i2c_open_bus(), #usb_open_hiddev_device()
 *  \retval DDCRC_LOCKED    display open in another thread
 *
 *  **Call_Option** flags recognized:
 *  - CALLOPT_WAIT
 *  - CALLOPT_ERR_MSG
 */
Public_Status_Code ddc_open_display(
      Display_Ref *    dref,
      Call_Options     callopts,
      Display_Handle** pdh)
{
   bool debug = false;
   DBGMSF(debug, "Opening display %s, callopts=%s",
                 dref_repr_t(dref), interpret_call_options_t(callopts) );

   Display_Handle * dh = NULL;
   Public_Status_Code psc = 0;

   Distinct_Display_Ref display_id = get_distinct_display_ref(dref);
   Distinct_Display_Flags ddisp_flags = DDISP_NONE;
   if (callopts & CALLOPT_WAIT)
      ddisp_flags |= DDISP_WAIT;

   bool locked = lock_distinct_display(display_id, ddisp_flags);
   if (!locked) {
      psc = DDCRC_LOCKED;          // is there an appropriate errno value?  EBUSY? EACCES?
      goto bye;
   }

   switch (dref->io_path.io_mode) {

   case DDCA_IO_I2C:
      {
         int fd = i2c_open_bus(dref->io_path.path.i2c_busno, callopts);
         if (fd < 0) {
            psc = fd;
            goto bye;
         }

         DBGMSF(debug, "Calling set_addr(0x37) for %s", dref_repr_t(dref));
         Status_Errno base_rc =  i2c_set_addr(fd, 0x37, callopts);
         if (base_rc != 0) {
            assert(base_rc < 0);
            close(fd);
            psc = base_rc;
            goto bye;
         }

         // Is this needed?
         // 10/24/15, try disabling:
         // sleepMillisWithTrace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);

         dh = create_bus_display_handle_from_display_ref(fd, dref);    // n. sets dh->dref = dref

         I2C_Bus_Info * bus_info = dref->detail;
         assert(bus_info);   // need to convert to a test?
         assert( memcmp(bus_info, I2C_BUS_INFO_MARKER, 4) == 0);

         dref->pedid = bus_info->edid;
         if (!dref->pedid) {
            // How is this even possible?
            // 1/2017:  Observed with x260 laptop and Ultradock, See ddcutil user report.
            //          close(fd) fails
            DBGMSG("No EDID for device on bus /dev/i2c-%d", dref->io_path.path.i2c_busno);
            // if (!(callopts & CALLOPT_FORCE)) {
               close(fd);
               psc = DDCRC_EDID;
               goto bye;
            // }
            // else
            //    DBGMSG0("Continuing");
         }
      }
      break;

   case DDCA_IO_ADL:
      dh = create_adl_display_handle_from_display_ref(dref);  // n. sets dh->dref = dref
      dref->pedid = adlshim_get_parsed_edid_by_display_handle(dh);
      break;

   case DDCA_IO_USB:
#ifdef USE_USB
      {
         DBGMSF(debug, "Opening USB device: %s", dref->usb_hiddev_name);
         assert(dref->usb_hiddev_name);
         // if (!dref->usb_hiddev_name) { // HACK
         //    DBGMSG("HACK FIXUP.  dref->usb_hiddev_name");
         //    dref->usb_hiddev_name = get_hiddev_devname_by_display_ref(dref);
         // }
         int fd = usb_open_hiddev_device(dref->usb_hiddev_name, callopts);
         if (fd < 0) {
            psc = fd;
            goto bye;
         }
         dh = create_usb_display_handle_from_display_ref(fd, dref);
         dref->pedid = usb_get_parsed_edid_by_display_handle(dh);
      }
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      break;
   } // switch
   assert(!dh || dh->dref->pedid);
   // needed?  for both or just I2C?
   // sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);
   if (dref->io_path.io_mode != DDCA_IO_USB)
      call_tuned_sleep_i2c(SE_POST_OPEN);
   // dbgrpt_display_handle(dh, __func__, 1);

bye:
   if (psc != 0) {
      if (locked)
         unlock_distinct_display(display_id);
      COUNT_STATUS_CODE(psc);
   }
   *pdh = dh;
   assert(psc <= 0);
   // dbgrpt_distinct_display_descriptors(0);
   DBGMSF(debug, "Done.  Returning: %s, *pdh=%d", psc_desc(psc), *pdh);
   return psc;
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
      DBGMSG0("Starting.");
      dbgrpt_display_handle(dh, __func__, 1);
   }

   switch(dh->dref->io_path.io_mode) {
   case DDCA_IO_I2C:
      {
         Status_Errno rc = i2c_close_bus(dh->fh, dh->dref->io_path.path.i2c_busno,  CALLOPT_NONE);    // return error if failure
         if (rc != 0) {
            assert(rc < 0);
            DBGMSG("close_i2c_bus returned %d", rc);
            COUNT_STATUS_CODE(rc);
         }
         dh->fh = -1;    // indicate invalid, in case we try to continue using dh
         break;
      }
   case DDCA_IO_ADL:
      break;           // nothing to do

   case DDCA_IO_USB:
#ifdef USE_USB
      {
         Status_Errno rc = usb_close_device(dh->fh, dh->dref->usb_hiddev_name, CALLOPT_NONE); // return error if failure
         if (rc != 0) {
            assert(rc < 0);
            DBGMSG("usb_close_device returned %d", rc);
            COUNT_STATUS_CODE(rc);
         }
         dh->fh = -1;
         break;
      }
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   } //switch

   Distinct_Display_Ref display_id = get_distinct_display_ref(dh->dref);
   unlock_distinct_display(display_id);

   free_display_handle(dh);
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


void ddc_report_write_read_stats(int depth) {
   assert(write_read_stats_rec);
   try_data_report(write_read_stats_rec, depth);
}


void ddc_reset_write_only_stats() {
   if (write_only_stats_rec)
      try_data_reset(write_only_stats_rec);
   else
      write_only_stats_rec = try_data_create("ddc write only", max_write_only_exchange_tries);
}


void ddc_report_write_only_stats(int depth) {
   assert(write_only_stats_rec);
   try_data_report(write_only_stats_rec, depth);
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
Public_Status_Code (*Write_Read_Raw_Function)(
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
static Public_Status_Code ddc_i2c_write_read_raw(
         Display_Handle * dh,
         DDC_Packet *     request_packet_ptr,
         int              max_read_bytes,
         Byte *           readbuf,
         int *            pbytes_received
        )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, readbuf=%p",dh_repr_t(dh), readbuf);
   // DBGMSG("request_packet_ptr=%p", request_packet_ptr);
   // dump_packet(request_packet_ptr);

   assert(dh);
   assert(dh->dref);
   assert(dh->dref->io_path.io_mode == DDCA_IO_I2C);
   // ASSERT_DISPLAY_IO_MODE(dh, DDCA_IO_DEVI2C);

#ifdef TEST_THAT_DIDNT_WORK
   bool single_byte_reads = false;   // doesn't work
#endif

   Status_Errno_DDC rc =
         invoke_i2c_writer(
                           dh->fh,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   DBGMSF(debug, "invoke_i2c_writer() returned %d\n", rc);
   if (rc == 0) {
      call_tuned_sleep_i2c(SE_WRITE_TO_READ);

      // ALTERNATIVE_THAT_DIDNT_WORK:
      // if (single_byte_reads)  // fails
      //    rc = invoke_single_byte_i2c_reader(dh->fh, max_read_bytes, readbuf);
      // else

      rc = invoke_i2c_reader(dh->fh, max_read_bytes, readbuf);
      // try adding sleep to see if improves capabilities read for P2411H
      call_tuned_sleep_i2c(SE_POST_READ);

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

   DBGTRC(debug, TRACE_GROUP, "Done. psc=%s", psc_desc(rc));
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

static Public_Status_Code ddc_adl_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            pbytes_received
     )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting. Using adl_ddc_write_only() and adl_ddc_read_only() dh=%s",
          dh_repr_t(dh));
   assert(dh && dh->dref && dh->dref->io_path.io_mode == DDCA_IO_ADL);
   // ASSERT_DISPLAY_IO_MODE(dh, DDCA_IO_ADL);

   Public_Status_Code psc = adlshim_ddc_write_only(
                               dh,
                               get_packet_start(request_packet_ptr),   // n. no adjustment, unlike i2c version
                               get_packet_len(request_packet_ptr)
                              );
   if (psc < 0) {
      DBGTRC(debug, TRACE_GROUP, "adl_ddc_write_only() returned gsc=%d\n", psc);
   }
   else {
      call_tuned_sleep_adl(SE_WRITE_TO_READ);
      psc = adlshim_ddc_read_only(
            dh,
            readbuf,
            pbytes_received);
      // note_io_event(IE_READ_AFTER_WRITE, __func__);
      if (psc < 0) {
         DBGTRC(debug, TRACE_GROUP, "adl_ddc_read_only() returned %d\n", psc);
      }
      else {
         if ( all_bytes_zero(readbuf+1, max_read_bytes-1)) {
                 psc = DDCRC_READ_ALL_ZERO;
                 DDCMSG(debug, "All zero response.", NULL);
                 COUNT_STATUS_CODE(psc);
         }
         else if (memcmp(get_packet_start(request_packet_ptr), readbuf, get_packet_len(request_packet_ptr)) == 0) {
            // is this a DDC error or a programming bug?
            DDCMSG(debug, "Bytes read same as bytes written.", __func__ );
            psc = COUNT_STATUS_CODE(DDCRC_DDC_DATA);   // was DDCRC_READ_EQUALS_WRITE
         }
         else {
            psc = 0;
         }
      }
   }

   if (psc < 0)
      log_status_code(psc, __func__);
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%s\n", psc_desc(psc));
   return psc;
}


static Public_Status_Code ddc_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            p_rcvd_bytes_ct
     )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, readbuf=%p, max_read_bytes=%d",
                              dh_repr_t(dh), readbuf, max_read_bytes);
   if (debug) {
      DBGMSG0("request_packer_ptr->raw_bytes:");
      dbgrpt_buffer(request_packet_ptr->raw_bytes, 1);
   }
   Public_Status_Code psc;

   // This function should not be called for USB
   assert(dh->dref->io_path.io_mode == DDCA_IO_I2C || dh->dref->io_path.io_mode == DDCA_IO_ADL);

   if (dh->dref->io_path.io_mode == DDCA_IO_I2C) {
        psc =  ddc_i2c_write_read_raw(
              dh,
              request_packet_ptr,
              max_read_bytes,
              readbuf,
              p_rcvd_bytes_ct
       );
   }
   else {
      psc =  ddc_adl_write_read_raw(
              dh,
              request_packet_ptr,
              max_read_bytes,
              readbuf,
              p_rcvd_bytes_ct
       );
   }

   DBGTRC(debug, TRACE_GROUP, "Done. Returning: %s", psc_desc(psc));
   if (psc == 0) {
      DBGTRC(debug, TRACE_GROUP,
             "      readbuf: %s",
             hexstring3_t(readbuf, *p_rcvd_bytes_ct, " ", 4, false));

            // hexstring_t(readbuf, *pbytes_received));
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
 *  \return >= 0 if success (positive values possible for ADL?)\n
 *          modulated ADL status code if ADL error or special success case\n
 *          -errno for Linux errors\n
 *          as from #create_ddc_typed_response_packet()
 * \remark
 *  Issue: positive ADL codes, need to handle?
 */
Error_Info *
ddc_write_read(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte             expected_response_type,
      Byte             expected_subtype,
      DDC_Packet **    response_packet_ptr_loc
     )
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s", dh_repr_t(dh) );

   Byte * readbuf = calloc(1, max_read_bytes);
   int    bytes_received = max_read_bytes;
   Public_Status_Code    psc;
   *response_packet_ptr_loc = NULL;

   psc =  ddc_write_read_raw(
            dh,
            request_packet_ptr,
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
              ddcrc_desc(psc), *response_packet_ptr_loc );

       if (psc != 0 && *response_packet_ptr_loc) {  // paranoid,  should never occur
          free(*response_packet_ptr_loc);
          *response_packet_ptr_loc = NULL;
       }
   }

   free(readbuf);    // or does response_packet_ptr_loc point into here?

   // already done:
   // if (rc != 0)
   //    COUNT_STATUS_CODE(rc);

   // If a DDC status code, has already been counted when set.  what about RR_ADL?
   // if (psc < 0  && get_modulation(psc) != RR_DDC)
   //    COUNT_STATUS_CODE(psc);

   Error_Info * excp = NULL;
   if (psc < 0)
      excp = errinfo_new(psc, __func__);

   DBGTRC(debug, TRACE_GROUP, "Done. Returning: %s", errinfo_summary(excp)  );
   if (psc == 0 && (IS_TRACING() || debug) )
      dbgrpt_packet(*response_packet_ptr_loc, 1);

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
 *  \return pointer to #Ddc_Error if failure, NULL if success
 *
 *  \remark
 *  status code from #ddc_write_read() may be positive for positive ADL status code ??
 *            status code from #ddc_write_read() if exactly 1 pass through try loop\n
 *            DDCRC_RETRIES, DDCRC_ALL_TRIES_ZERO, DDCRC_ALL_RESPONES_NULL if maximum tries exceeded
 *
 *\remark
 *   Issue: positive ADL codes, need to handle?
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
          dh_repr_t(dh), bool_repr(all_zero_response_ok)  );
   assert(dh->dref->io_path.io_mode != DDCA_IO_USB);
   // show_backtrace(1);

   // will be false on initial call to verify DDC communication
   // bool null_response_checked = dh->dref->flags & DREF_DDC_NULL_RESPONSE_CHECKED;   // unused
   bool retry_null_response = !(dh->dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED);

   Public_Status_Code  psc;
   int  tryctr;
   bool retryable;
   int  ddcrc_read_all_zero_ct = 0;
   int  ddcrc_null_response_ct = 0;
   int  ddcrc_null_response_max = (retry_null_response) ? 3 : 0;
   Error_Info * try_errors[MAX_MAX_TRIES];

   assert(max_write_read_exchange_tries > 0);   // to avoid clang warning
   for (tryctr=0, psc=-999, retryable=true;
        tryctr < max_write_read_exchange_tries && psc < 0 && retryable;
        tryctr++)
   {
      DBGMSF(debug,
           "Start of try loop, tryctr=%d, max_write_read_echange_tries=%d, rc=%d, retryable=%d",
           tryctr, max_write_read_exchange_tries, psc, retryable );

      Error_Info * cur_excp = ddc_write_read(
                dh,
                request_packet_ptr,
                max_read_bytes,
                expected_response_type,
                expected_subtype,
                response_packet_ptr_loc);
      psc = (cur_excp) ? cur_excp->status_code : 0;
      try_errors[tryctr] = cur_excp;

      if (psc == 0 && ddcrc_null_response_ct > 0) {
         DBGMSG("%s, ddc_write_read() succeeded after %d sleep and retry for DDC Null Response",
                dh_repr_t(dh),
                ddcrc_null_response_ct);
      }

      if (psc < 0) {     // n. ADL status codes have been modulated
         DBGMSF(debug, "ddc_write_read() returned %s", psc_desc(psc) );
         COUNT_RETRYABLE_STATUS_CODE(psc);

         if (dh->dref->io_path.io_mode == DDCA_IO_I2C) {
            // The problem: Does NULL response indicate an error condition, or
            // is the monitor using NULL response to indicate unsupported?
            // Acer monitor uses NULL response instead of setting the unsupported
            // flag in a valid response

            if (psc == DDCRC_NULL_RESPONSE) {
               retryable = (ddcrc_null_response_ct++ < ddcrc_null_response_max);
               DBGMSF(debug, "DDCRC_NULL_RESPONSE, retryable=%s", bool_repr(retryable));
               if (retryable) {
                  if (ddcrc_null_response_ct == 1 && get_output_level() >= DDCA_OL_VERBOSE)
                     f0printf(fout(), "Extended delay as recovery from DDC Null Response...\n");
                  call_dynamic_tuned_sleep_i2c(SE_DDC_NULL, ddcrc_null_response_ct);
               }
            }
            // when is DDCRC_READ_ALL_ZERO actually an error vs the response of the monitor instead of NULL response?
            // On Dell monitors (P2411, U3011) all zero response occurs on unsupported Table features
            // But also seen as a bad response
            else if ( psc == DDCRC_READ_ALL_ZERO)
               retryable = (all_zero_response_ok) ? false : true;

            else if (psc == -EIO)
                retryable = false;     // ??

            else if (psc == -EBADF)
               retryable = false;

            else if (psc == -ENXIO)    // no such device or address, i915 driver
               retryable = false;

            else
               retryable = true;     // for now


            // try exponential backoff on all errors, not just SE_DDC_NULL
            // if (retryable)
            //    call_dynamic_tuned_sleep_i2c(SE_DDC_NULL, tryctr+1);
         }

         else {   // DDC_IO_ADL
            // TODO more detailed tests
            if (psc == DDCRC_NULL_RESPONSE)
               retryable = false;
            else if (psc == DDCRC_READ_ALL_ZERO)
               retryable = true;
            else
               retryable = false;
         }

         if (psc == DDCRC_READ_ALL_ZERO)
            ddcrc_read_all_zero_ct++;
      }    // rc < 0
   }
   DBGTRC(debug, DDCA_TRC_NONE, "After try loop. tryctr=%d, psc=%d, retryable=%s",
         tryctr, psc, bool_repr(retryable));
   if (debug) {
      for (int ndx = 0; ndx < tryctr; ndx++) {
         DBGMSG("try_errors[%d] = %p", ndx, try_errors[ndx]);
      }
   }

   // Using new Ddc_Error mechanism
   Error_Info * ddc_excp = NULL;

   if (psc < 0) {
      // int last_try_index = tryctr-1;
      DBGTRC(debug, TRACE_GROUP, "After try loop. tryctr=%d, retryable=%s", tryctr, bool_repr(retryable));

      if (retryable)
         psc = DDCRC_RETRIES;
      else if (ddcrc_read_all_zero_ct == max_write_read_exchange_tries)
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

   try_data_record_tries(write_read_stats_rec, psc, tryctr);

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", errinfo_summary(ddc_excp));
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
Public_Status_Code
ddc_i2c_write_only(
         int           fh,
         DDC_Packet *  request_packet_ptr
        )
{
   bool debug = false;
   DBGTRC0(debug, TRACE_GROUP, "Starting.");
   if (debug)
      dbgrpt_packet(request_packet_ptr, 1);

   Status_Errno_DDC rc =
         invoke_i2c_writer(fh,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   if (rc < 0)
      log_status_code(rc, __func__);
   Sleep_Event_Type sleep_type =
         (request_packet_ptr->type == DDC_PACKET_TYPE_SAVE_CURRENT_SETTINGS )
            ? SE_POST_SAVE_SETTINGS
            : SE_POST_WRITE;
   call_tuned_sleep_i2c(sleep_type);
   DBGTRC(debug, TRACE_GROUP, "Done. rc=%s", psc_desc(rc) );
   return rc;
}


/* Writes a DDC request packet to a monitor
 *
 * \param  dh                  Display_Handle for open I2C or ADL device
 * \param  request_packet_ptr  DDC packet to write
 * \return NULL if success, #Ddc_Error if error
 */
Error_Info *
ddc_write_only(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr)
{
   bool debug = false;
   DBGTRC0(debug, TRACE_GROUP, "Starting.");

   Public_Status_Code psc = 0;
   assert(dh->dref->io_path.io_mode != DDCA_IO_USB);
   if (dh->dref->io_path.io_mode == DDCA_IO_I2C) {
      psc = ddc_i2c_write_only(dh->fh, request_packet_ptr);
   }
   else {
      psc = adlshim_ddc_write_only(
              dh,
              get_packet_start(request_packet_ptr),
              get_packet_len(request_packet_ptr)
              // get_packet_start(request_packet_ptr)+1,
              // get_packet_len(request_packet_ptr)-1
             );
      if (psc > 0) {
         DBGMSG("Unusual positive return code from ADL: %d", psc);
         psc = 0;
      }
   }

   Error_Info *  ddc_excp = NULL;
   if (psc)
      ddc_excp = errinfo_new(psc, __func__);

   DBGTRC(debug, TRACE_GROUP, "Done. Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}


/* Wraps ddc_write_only() in retry logic.
 *
 *  \param  dh                  display handle (for either I2C or ADL device)
 *  \param  request_packet_ptr  DDC packet to write
 *  \return pointer to #Ddc_Error if failure, NULL if success
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
   DBGTRC0(debug, TRACE_GROUP, "Starting." );

   assert(dh->dref->io_path.io_mode != DDCA_IO_USB);

   Public_Status_Code psc;
   int                tryctr;
   bool               retryable;
   Error_Info *       try_errors[MAX_MAX_TRIES];

   assert(max_write_only_exchange_tries > 0);
   for (tryctr=0, psc=-999, retryable=true;
       tryctr < max_write_only_exchange_tries && psc < 0 && retryable;
       tryctr++)
   {
      DBGMSF(debug,
             "Start of try loop, tryctr=%d, max_write_only_exchange_tries=%d, rc=%d, retryable=%d",
             tryctr, max_write_only_exchange_tries, psc, retryable );

      Error_Info * cur_excp = ddc_write_only(dh, request_packet_ptr);
      psc = (cur_excp) ? cur_excp->status_code : 0;
      try_errors[tryctr] = cur_excp;

      if (psc < 0) {
         COUNT_RETRYABLE_STATUS_CODE(psc);
         if (dh->dref->io_path.io_mode == DDCA_IO_I2C) {
            if (psc < 0) {
               if (psc != -EIO)
                   retryable = false;
            }
         }
         else {
            if (psc < 0) {
                // no logic in ADL case to test for continuing to retry, should there be ???
                // is it even meaningful to retry for ADL?
                   // retryable = true;    // *** TEMP ***
            }
         }
      }   // rc < 0
      // try_status_codes[tryctr] = psc;   // for future Ddc_Error mechanism
   }


   Error_Info * ddc_excp = NULL;

   if (psc < 0) {
      // now:
      //   tryctr = number of tries
      //   tryctr-1 = index of last try
      //   tryctr == max_write_only_exchange_tries &&  retryable
      //   tryctr <  max_write_only_exchange_tries && !retryable
      //   tryctr == max_write_only_exchange_tries && !retryable

      // int last_try_index = tryctr-1;
      DBGTRC(debug, TRACE_GROUP, "After try loop. tryctr=%d, retryable=%s", tryctr, bool_repr(retryable));

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

   try_data_record_tries(write_only_stats_rec, psc, tryctr);

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}

