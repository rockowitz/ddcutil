/** \file ddc_packet_io.c
 *
 *  Functions for performing DDC packet IO, using the I2C bus.
 *  Handles I2C bus retry
 *
 *  Historically, this file also handled packet io for the old ADL API.
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "base/parms.h"

/** \cond */
#include <assert.h>
#include <base/display_lock.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_types.h"

#include "util/debug_util.h"
#include "util/edid.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#include "util/utilrpt.h"
/** \endcond */

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/dsa2.h"
#include "base/execution_stats.h"
#include "base/i2c_bus_base.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"
#include "base/per_display_data.h"

#include "sysfs/sysfs_simple.h"
#include "sysfs/sysfs_base.h"
#include "sysfs/sysfs_dpms.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_bus_open_close.h"
#include "i2c/i2c_strategy_dispatcher.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_displays.h"
#include "ddc/ddc_try_data.h"

#include "ddc/ddc_packet_io.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

bool DDC_Read_Bytewise  = DEFAULT_DDC_READ_BYTEWISE;
bool simulate_null_msg_means_unsupported = false;


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
// not static so that function can appear in backtrace
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, read_bytewise=%s, max_read_bytes=%d, readbuf=%p",
                              dh_repr(dh), SBOOL(read_bytewise), max_read_bytes, readbuf );
   // DBGMSG("request_packet_ptr=%p", request_packet_ptr);
   // dump_packet(request_packet_ptr);

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "request_packet_ptr->raw_bytes: %s",
                              hexstring3_t(request_packet_ptr->raw_bytes->bytes,
                                           request_packet_ptr->raw_bytes->len,
                                           " ", 1, false) );
   TRACED_ASSERT(dh);
   TRACED_ASSERT(dh->dref);
   TRACED_ASSERT(dh && dh->dref && dh->dref->io_path.io_mode == DDCA_IO_I2C);
   // This function should not be called for USB

#ifdef TEST_THAT_DIDNT_WORK
   bool single_byte_reads = false;   // doesn't work
#endif

#ifndef NDEBUG
   Byte slave_addr = request_packet_ptr->raw_bytes->bytes[0];      // 0x6e
   TRACED_ASSERT(slave_addr >> 1 == 0x37);
#endif

   CHECK_DEFERRED_SLEEP(dh);
   Status_Errno_DDC rc =
         invoke_i2c_writer(
                           dh->fd,
                           0x37,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   DBGMSF(debug, "invoke_i2c_writer() returned %d", rc);
   if (rc == 0) {
      TUNED_SLEEP_WITH_TRACE(dh, SE_WRITE_TO_READ, "Called from ddc_i2c_write_read_raw");

      // ALTERNATIVE_THAT_DIDNT_WORK:
      // if (single_byte_reads)  // fails
      //    rc = invoke_single_byte_i2c_reader(dh->fd, max_read_bytes, readbuf);
      // else

      CHECK_DEFERRED_SLEEP(dh);
      rc = invoke_i2c_reader(dh->fd, 0x37, read_bytewise, max_read_bytes, readbuf);

      // try adding sleep to see if improves capabilities read for P2411H
      // tuned_sleep_i2c_with_trace(SE_POST_READ, __func__, NULL);
      TUNED_SLEEP_WITH_TRACE(dh, SE_POST_READ, "Called from ddc_i2c_write_read_raw");

      if (rc == 0)
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Response bytes: %s",
                                hexstring3_t(readbuf, max_read_bytes, " ", 1, false) );

      if (rc == 0 && all_bytes_zero(readbuf, max_read_bytes)) {
         DDCMSG(debug, "All zero response detected in %s", __func__);
         rc = DDCRC_READ_ALL_ZERO;
         // printf("(%s) All zero response.", __func__ );
         // DBGMSG("Request was: %s",
         // hexstring(get_packet_start(request_packet_ptr)+1,get_packet_len(request_packet_ptr)-1));
      }
   }
   if (rc < 0) {
      COUNT_STATUS_CODE(rc);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
   return rc;
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
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "dh=%s, read_bytewise=%s, max_read_bytes=%d,"
         " expected_response_type=0x%02x, expected_subtype=0x%02x",
          dh_repr(dh), SBOOL(read_bytewise), max_read_bytes,
          expected_response_type, expected_subtype  );

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "Adding 1 to max_read_bytes to allow for initial double 0x63 quirk");
   max_read_bytes++;   //allow for quirk of double 0x6e at start
   Byte * readbuf = calloc(1, max_read_bytes);
   int    bytes_received = max_read_bytes;
   DDCA_Status    psc;
   *response_packet_ptr_loc = NULL;

   psc = ddc_i2c_write_read_raw(
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
       DBGTRC_NOPREFIX(debug, TRACE_GROUP,
              "create_ddc_typed_response_packet() returned %s, *response_packet_ptr_loc=%p",
              ddcrc_desc_t(psc), *response_packet_ptr_loc );

       if (psc == DDCRC_OK && simulate_null_msg_means_unsupported) {
          DDC_Packet * pkt = *response_packet_ptr_loc;
          if ( pkt && pkt->type == DDC_PACKET_TYPE_QUERY_VCP_RESPONSE) {
             Parsed_Nontable_Vcp_Response * resp = pkt->parsed.nontable_response;
             if (resp->valid_response && !resp->supported_opcode) {
                DBGMSG("Setting DDCRC_NULL_RESPONSE for unsupported feature 0x%02x",resp->vcp_code);
                psc = DDCRC_NULL_RESPONSE;
             }
          }
       }

       if (psc != 0 && *response_packet_ptr_loc) {  // paranoid,  should never occur
          free(*response_packet_ptr_loc);
          *response_packet_ptr_loc = NULL;
       }
   }
   free(readbuf);    // or does response_packet_ptr_loc point into here?

   Error_Info * excp = (psc < 0) ? ERRINFO_NEW(psc,NULL) : NULL;
   DBGTRC_RET_ERRINFO_STRUCT(debug, TRACE_GROUP, excp, response_packet_ptr_loc, dbgrpt_packet);
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
 *  \param explicit_max_tries       if > 0, overrides the otherwise determined maximum tries
 *
 *  \return pointer to #Error_Info struct if failure, NULL if success
 */
Error_Info *
ddc_write_read_with_retry(
         Display_Handle * dh,
         DDC_Packet *     request_packet_ptr,
         int              max_read_bytes,
         Byte             expected_response_type,
         Byte             expected_subtype,
         DDC_Write_Read_Flags flags,
         DDC_Packet **    response_packet_ptr_loc,
         int              explicit_max_tries
        )
{
   bool debug = false;
   bool all_zero_response_ok = flags & Write_Read_Flag_All_Zero_Response_Ok;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "dh=%s, max_read_bytes=%d, expected_response_type=0x%02x, expected_subtype=0x%02x,"
         " all_zero_response_ok=%s, Write_Read_Flag_All_Zero_Response_Ok: %s",
         dh_repr(dh), max_read_bytes, expected_response_type, expected_subtype,
         sbool(all_zero_response_ok), sbool(flags&Write_Read_Flag_All_Zero_Response_Ok) );
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref flags: %s", interpret_dref_flags_t(dh->dref->flags));
   Per_Display_Data * pdd = dh->dref->pdd;
   TRACED_ASSERT(dh->dref->io_path.io_mode != DDCA_IO_USB);
   // show_backtrace(1);
   // if (debug)
   //     dbgrpt_display_ref(dh->dref, 1);

   bool read_bytewise = DDC_Read_Bytewise;   // normally set to DEFAULT_I2C_READ_BYTEWISE
   DDCA_Status  psc;
   int  tryctr;
   bool retryable;
   int  ddcrc_read_all_zero_ct = 0;
   int  ddcrc_null_response_ct = 0;
   int  max_tries = (explicit_max_tries > 0) ? explicit_max_tries
                                             : try_data_get_maxtries(WRITE_READ_TRIES_OP);
   int  ddcrc_null_response_max = 3;
   Error_Info * master_error = NULL;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"ddcrc_null_response_max=%d, read_bytewise=%s",
                                        ddcrc_null_response_max, sbool(read_bytewise));
   Error_Info * try_errors[MAX_MAX_TRIES] = {NULL};

   TRACED_ASSERT(max_tries >= 1);
   for (tryctr=0, psc=-999, retryable=true;
        tryctr < max_tries && psc < 0 && retryable;
        tryctr++)
   {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "Start of try loop, tryctr=%d, max_tries=%d, psc=%s, retryable=%s, "
         "read_bytewise=%s, sleep-multiplier=%5.2f",
         tryctr, max_tries, psc_name_code(psc), sbool(retryable),
         sbool(read_bytewise), pdd_get_adjusted_sleep_multiplier(pdd) );

      Error_Info * cur_excp = ddc_write_read(
                dh,
                request_packet_ptr,
                read_bytewise,
                max_read_bytes,
                expected_response_type,
                expected_subtype,
                response_packet_ptr_loc);

      ASSERT_IFF(!cur_excp, *response_packet_ptr_loc);
      // TESTCASES:
      // if (tryctr < 2)
      //    cur_excp = ERRINFO_NEW(DDCRC_NULL_RESPONSE, "dummy");
      // cur_excp = errinfo_new(-EIO, "dummy");

      psc = (cur_excp) ? cur_excp->status_code : 0;
      try_errors[tryctr] = cur_excp;

      if (psc == 0 && ddcrc_null_response_ct > 0) {
         DUAL_MSGXV(debug, DDCA_SYSLOG_DEBUG, TRACE_GROUP | DDCA_TRC_RETRY,
               "%s, expected_subtype=0x%02x, sleep-multiplier=%5.2f, ddc_write_read() succeeded"
               " after %d sleep and retry for DDC Null Response",
               dh_repr(dh),
               expected_subtype,
               pdd_get_adjusted_sleep_multiplier(pdd),
               ddcrc_null_response_ct);
       }

      bool adjust_remaining_tries_for_null = false;

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
                  // testing_unsupported_feature_active really is redundant,
                  // DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED is set upon completion of
                  // testing for unsupported feature
                  assert( !(dh->testing_unsupported_feature_active &&
                            dh->dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED) );
                  if (!dh->testing_unsupported_feature_active) {
                     bool may_mean_unsupported_feature =
                           (expected_response_type == DDC_PACKET_TYPE_QUERY_VCP_RESPONSE &&
                            (dh->dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED)) ||
                           expected_response_type == DDC_PACKET_TYPE_TABLE_READ_RESPONSE;
                     if (may_mean_unsupported_feature) {
                        adjust_remaining_tries_for_null = true;
                        retryable = (++ddcrc_null_response_ct <= ddcrc_null_response_max);
                        DBGTRC(debug, DDCA_TRC_NONE,
                              "DDCRC_NULL_RESPONSE, retryable=%s", sbool(retryable));
                        if (!retryable) {
                           MSG_W_SYSLOG(DDCA_SYSLOG_WARNING,
                                 "Feature 0x%02x, maximum retries (%d) for DDC Null Response exceeded",
                                 expected_subtype, ddcrc_null_response_max);
                        }
                     }
                  }
                  else
                     retryable = true;
               }
               break;

         case (DDCRC_READ_ALL_ZERO):
              // Sometimes an all-zero response indicates an unsupported feature
              // instead of an error.  On Dell P2411 and U3011 the all zero response occurs
              // when reading an unsupported table feature.
              retryable = (all_zero_response_ok) ? false : true;
              ddcrc_read_all_zero_ct++;
              break;

         case (-EIO):
              retryable = false;     // ??
              break;

         case (-EBADF):
              // DBGMSG("EBADF");
              retryable = false;
              break;

         case (-ENXIO):    // no such device or address, i915 driver
              // But have seen success after 7 retries of errors including ENXIO, DDCRC_DATA, make retryable?
              retryable = false;
              break;

         case (-EBUSY):
               retryable = false;
               break;

         default:
              retryable = true;     // for now
         }

         if ((psc == -EIO || psc == -ENXIO) && execution_mode == MODE_LIBDDCUTIL) {
            Error_Info * err = i2c_check_open_bus_alive(dh);
            if (err) {
               if (err->status_code == DDCRC_DISCONNECTED) {
                  mark_display_ref_disconnected(dh->dref);
               }
               master_error = err;
               goto bye;
            }
         }

         // try exponential backoff on all errors, not just SE_DDC_NULL
         // if (retryable)
         //    call_dynamic_tuned_sleep_i2c(SE_DDC_NULL, tryctr+1);
      }    // rc < 0

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Bottom of try loop. psc=%s, tryctr=%d,  ddcrc_null_response_ct=%d, retryable=%s",
            psc_name_code(psc), tryctr, ddcrc_null_response_ct, sbool(retryable));
      int remaining_tries = (max_tries-1) - tryctr;
      int adjusted_remaining_tries = remaining_tries;
      if (adjust_remaining_tries_for_null) {
         adjusted_remaining_tries =  (ddcrc_null_response_max-1) - tryctr;
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
               "tryctr = %d, unadjusted remaining_tries=%d, adjusted_remaining_tries=%d",
               tryctr, remaining_tries, adjusted_remaining_tries);
      }
      if (psc != 0  && retryable && remaining_tries > 0) {
         pdd_note_retryable_failure_by_dh(dh, psc, adjusted_remaining_tries);

      }
   }  // for loop

   // tryctr = number of times through loop, i.e. 1..max_tries
   assert(tryctr >= 1 && tryctr <= max_tries);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "After try loop. tryctr=%d, psc=%s, ddcrc_null_response_ct=%d, retryable=%s",
         tryctr, psc_name_code(psc), ddcrc_null_response_ct, sbool(retryable) );

   bool all_responses_null_meant_unsupported = false;
   int adjusted_tryctr = tryctr;
   if ( ( (dh->dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED) ||
          expected_response_type == DDC_PACKET_TYPE_TABLE_READ_RESPONSE )
         && !(flags & Write_Read_Flag_Capabilities)
         && ddcrc_null_response_ct == tryctr)
   {
      all_responses_null_meant_unsupported = true;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED or table read and all responses null");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "adjusting try count passed to pdd_record_final_by_dh() to 1");
      // don't pollute the stats with try counts that don't reflect real errors
      adjusted_tryctr = 1;
   }
   pdd_record_final_by_dh(dh, psc, adjusted_tryctr);

   Error_Info * errors_found[MAX_MAX_TRIES];
   int errct = 0;
   for (int ndx = 0; ndx < MAX_MAX_TRIES; ndx++) {
      if (try_errors[ndx])
         errors_found[errct++] = try_errors[ndx];
   }
   char * s = errinfo_array_summary(errors_found, errct);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP | DDCA_TRC_RETRY,
                   "%s,%s after %d error(s): %s",
                   dh_repr(dh),
                   (psc == 0) ? "Succeeded" : "Failed",
                   errct, s);
   free(s);

   if (psc < 0) {
      // int last_try_index = tryctr-1;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                      "After try loop. tryctr=%d, retryable=%s", tryctr, sbool(retryable));

      if (retryable)
         psc = DDCRC_RETRIES;
      else if (ddcrc_read_all_zero_ct == max_tries)
         psc = DDCRC_ALL_TRIES_ZERO;
      else if (all_responses_null_meant_unsupported) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
               "Converting DDCRC_ALL_RESPONSES_NULL to DDCRC_DETERMINED_UNSUPPORTED");
         psc = DDCRC_DETERMINED_UNSUPPORTED;
      }
      else if (ddcrc_null_response_ct > ddcrc_null_response_max) {
         psc = DDCRC_ALL_RESPONSES_NULL;
      }

      master_error = errinfo_new_with_causes(psc, errors_found, errct, __func__, NULL);

      if (psc != try_errors[tryctr-1]->status_code)
         COUNT_STATUS_CODE(psc);     // new status code, count it
   }
   else {
      for (int ndx = 0; ndx < tryctr-1; ndx++) {
         BASE_ERRINFO_FREE_WITH_REPORT(try_errors[ndx], IS_DBGTRC(debug, TRACE_GROUP));
      }
   }

   try_data_record_tries(dh, WRITE_READ_TRIES_OP, psc, tryctr);

bye:
   DBGTRC_DONE(debug, TRACE_GROUP, "Total Tries (tryctr): %d. *response_packet_pointer_loc=%p,  Returning: %s",
                                   tryctr, *response_packet_ptr_loc, errinfo_summary(master_error));
   ASSERT_IFF(!master_error, *response_packet_ptr_loc);
   return master_error;
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (debug)
      dbgrpt_packet(request_packet_ptr, 2);

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "request_packet_ptr->raw_bytes: %s",
                              hexstring3_t(request_packet_ptr->raw_bytes->bytes,
                                           request_packet_ptr->raw_bytes->len,
                                           " ", 1, false) );
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
   TUNED_SLEEP_WITH_TRACE(dh, sleep_type, "Called from ddc_i2c_write_only");
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "");
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

   DDCA_Status psc = ddc_i2c_write_only(dh, request_packet_ptr);
   Error_Info *  ddc_excp = (psc) ? ERRINFO_NEW(psc,NULL) : NULL;

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", errinfo_summary(ddc_excp));
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "" );

   TRACED_ASSERT(dh->dref->io_path.io_mode == DDCA_IO_I2C);

   DDCA_Status        psc;
   int                tryctr;
   bool               retryable;
   Error_Info *       try_errors[MAX_MAX_TRIES];
   Error_Info *       ddc_excp = NULL;

   int max_tries = try_data_get_maxtries(WRITE_ONLY_TRIES_OP);
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
      if (psc == -EBUSY)
         retryable = false;

      if ((psc == -EIO || psc == -ENXIO) && execution_mode == MODE_LIBDDCUTIL) {
         Error_Info * err = i2c_check_open_bus_alive(dh);
         if (err) {
            ddc_excp = err;
            goto bye;
         }
      }
   }

   if (psc < 0) {
      // now:
      //   tryctr = number of tries
      //   tryctr-1 = index of last try
      //   tryctr == max_tries &&  retryable
      //   tryctr <  max_tries && !retryable
      //   tryctr == max_tries && !retryable

      // int last_try_index = tryctr-1;
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "After try loop. tryctr=%d, retryable=%s",
                                           tryctr, sbool(retryable));
      if (retryable) {
         psc = DDCRC_RETRIES;
         ddc_excp = errinfo_new_with_causes(psc, try_errors, tryctr, __func__, NULL);
         if (psc != try_errors[tryctr-1]->status_code)
            COUNT_STATUS_CODE(psc);     // new status code, count it
      }
      else {
         assert (tryctr == 1);
         ddc_excp = try_errors[0];
      }
   }
   else {
      // 2 possibilities:
      //   succeeded after retries, there will be some errors (tryctr > 1)
      //   no errors (tryctr == 1)
      // int last_bad_try_index = tryctr-2;
      for (int ndx = 0; ndx < tryctr-1; ndx++) {
         BASE_ERRINFO_FREE_WITH_REPORT(try_errors[ndx], IS_DBGTRC(debug, TRACE_GROUP) );
      }
   }

bye:
   try_data_record_tries(dh, WRITE_ONLY_TRIES_OP, psc, tryctr);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "");
   return ddc_excp;
}


static void
init_ddc_packet_io_func_name_table() {
   RTTI_ADD_FUNC(ddc_i2c_write_read_raw);
   RTTI_ADD_FUNC(ddc_i2c_write_only);
// RTTI_ADD_FUNC(ddc_write_read_raw);
   RTTI_ADD_FUNC(ddc_write_read);
   RTTI_ADD_FUNC(ddc_write_read_with_retry);
   RTTI_ADD_FUNC(ddc_write_only);
   RTTI_ADD_FUNC(ddc_write_only_with_retry);
}


void
init_ddc_packet_io() {
   init_ddc_packet_io_func_name_table();
}

void
terminate_ddc_packet_io() {
   // ddc_close_all_displays();
}

