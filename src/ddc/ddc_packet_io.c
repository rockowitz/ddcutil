/*
 * ddc_packet_io.c
 *
 *  Created on: Jun 13, 2014
 *      Author: rock
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <i2c-dev.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <util/string_util.h>

#include <base/ddc_errno.h>
#include <base/ddc_packets.h>
#include <base/displays.h>
#include <base/msg_control.h>
#include <base/parms.h>
#include <base/status_code_mgt.h>
#include <base/util.h>

#include <i2c/i2c_bus_core.h>
#include <i2c/i2c_do_io.h>

#include <adl/adl_intf.h>

#include <ddc/try_stats.h>

#include <ddc/ddc_packet_io.h>


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

#ifdef APPARENTLY_UNUSED
bool is_ddc_null_message(Byte * packet) {
   return (packet[0] == 0x6f &&
           packet[1] == 0x6e &&
           packet[2] == 0x80 &&
           packet[3] == 0xbe
          );
}
#endif


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
Display_Handle* ddc_open_display(Display_Ref * dref,  Failure_Action failure_action) {
   Display_Handle * pDispHandle = NULL;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      int fd = i2c_open_bus(dref->busno, failure_action);
      // TODO: handle open failure, when failure_action = return error
      // all callers currently EXIT_IF_FAILURE
      if (fd >= 0) {    // will be < 0 if open_i2c_bus failed and failure_action = RETURN_ERROR_IF_FAILURE
         i2c_set_addr(fd, 0x37);

         // Is this needed?
         // 10/24/15, try disabling:
         // sleepMillisWithTrace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);

         pDispHandle = create_bus_display_handle(fd, dref->busno);
      }
      else {
         log_status_code(modulate_rc(fd, RR_ERRNO), __func__);
      }
   }
   else {
      pDispHandle = create_adl_display_handle_from_display_ref(dref);
   }
   // needed?  for both or just I2C?
   // sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, NULL);
   call_tuned_sleep_i2c(SE_POST_OPEN);
   // report_display_handle(pDispHandle, __func__);
   return pDispHandle;
}


void ddc_close_display(Display_Handle * dh) {
   // printf("(%s) Starting.\n", __func__);
   // report_display_handle(dh, __func__);
   if (dh->ddc_io_mode == DDC_IO_DEVI2C) {
      bool failure_action = EXIT_IF_FAILURE;
      // bool  failure_action = RETURN_ERROR_IF_FAILURE;
      int rc = i2c_close_bus(dh->fh, dh->busno,  failure_action);
      if (rc != 0) {
         printf("(%s) close_i2c_bus returned %d\n", __func__, rc);
         log_status_code(modulate_rc(rc, RR_ERRNO), __func__);
      }
   }
}


Display_Ref* ddc_find_display_by_model_and_sn(const char * model, const char * sn) {
   // printf("(%s) Starting.  model=%s, sn=%s   \n", __func__, model, sn );
   Display_Ref * result = NULL;
   Bus_Info * businfo = i2c_find_bus_info_by_model_sn(model, sn);
   if (businfo) {
      result = create_bus_display_ref(businfo->busno);
   }
   else {
      ADL_Display_Rec * adlrec = adl_find_display_by_model_sn(model, sn);
      if (adlrec) {
         result = create_adl_display_ref(adlrec->iAdapterIndex, adlrec->iDisplayIndex);
      }
   }
   // printf("(%s) Returning: %p  \n", __func__, result );
   return result;
}


Display_Ref* ddc_find_display_by_edid(const Byte * pEdidBytes) {
   // printf("(%s) Starting.  model=%s, sn=%s   \n", __func__, model, sn );
   Display_Ref * result = NULL;
   Bus_Info * businfo = i2c_find_bus_info_by_edid((pEdidBytes));
   if (businfo) {
      result = create_bus_display_ref(businfo->busno);
   }
   else {
      ADL_Display_Rec * adlrec = adl_find_display_by_edid(pEdidBytes);
      if (adlrec) {
         result = create_adl_display_ref(adlrec->iAdapterIndex, adlrec->iDisplayIndex);
      }
   }
   // printf("(%s) Returning: %p  \n", __func__, result );
   return result;
}


Parsed_Edid* ddc_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   Parsed_Edid* pEdid = NULL;

   if (dref->ddc_io_mode == DDC_IO_DEVI2C)
      pEdid = i2c_get_parsed_edid_by_busno(dref->busno);
   else
      pEdid = adl_get_parsed_edid_by_adlno(dref->iAdapterIndex, dref->iDisplayIndex);

   // printf("(%s) Returning %p\n", __func__, pEdid);
   TRCMSG("Returning %p", __func__, pEdid);
   return pEdid;
}


/** Tests if a DisplayRef identifies an attached display.
 */
bool ddc_is_valid_display_ref(Display_Ref * dref) {
   assert( dref );
   // char buf[100];
   // printf("(%s) Starting.  %s   \n", __func__, displayRefShortName(pdisp, buf, 100) );
   bool result;
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      result = i2c_is_valid_bus(dref->busno, true /* emit_error_msg */);
   }
   else {
      result = adl_is_valid_adlno(dref->iAdapterIndex, dref->iDisplayIndex, true /* emit_error_msg */);
   }
   // printf("(%s) Returning %d\n", __func__, result);
   return result;
}


// Retry Management

// 11/2015: do these MAX_MAX_ constants serve any purpose?
// #define MAX_MAX_WRITE_ONLY_EXCHANGE_TRIES  15
// #define MAX_MAX_WRITE_READ_EXCHANGE_TRIES  15

// constants in parms.h:
static int max_write_only_exchange_tries =  MAX_WRITE_ONLY_EXCHANGE_TRIES;
static int max_write_read_exchange_tries =  MAX_WRITE_READ_EXCHANGE_TRIES;


void ddc_set_max_write_only_exchange_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_TRIES);
   max_write_only_exchange_tries = ct;
}


int ddc_get_max_write_only_exchange_tries() {
   return max_write_only_exchange_tries;
}


void ddc_set_max_write_read_exchange_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_TRIES);
   max_write_read_exchange_tries = ct;
}

int ddc_get_max_write_read_exchange_tries() {
   return max_write_read_exchange_tries;
}


// Retry Statistics

static void * write_read_stats_rec = NULL;
static void * write_only_stats_rec = NULL;


void ddc_reset_write_read_stats() {
   if (write_read_stats_rec)
      reset_try_data(write_read_stats_rec);
   else
      write_read_stats_rec = create_try_data("ddc write/read", max_write_read_exchange_tries);
}


void ddc_report_write_read_stats() {
   assert(write_read_stats_rec);
   report_try_data(write_read_stats_rec);
}


void ddc_reset_write_only_stats() {
   if (write_only_stats_rec)
      reset_try_data(write_only_stats_rec);
   else
      write_only_stats_rec = create_try_data("ddc write only", max_write_only_exchange_tries);
}


void ddc_report_write_only_stats() {
   assert(write_only_stats_rec);
   report_try_data(write_only_stats_rec);
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
// write and read operations that take DDC_Packets
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
   Trace_Group tg = TRACE_GROUP;
   if (debug)
      tg = 0xff;
   TRCMSGTG(tg, "Starting. dh=%s, readbuf=%p", display_handle_repr_r(dh, NULL, 0), readbuf);
   // printf("(%s) request_packet_ptr=%p\n", __func__, request_packet_ptr);
   // dump_packet(request_packet_ptr);
   ASSERT_VALID_DISPLAY_REF(dh, DDC_IO_DEVI2C);

   Global_Status_Code rc =
         invoke_i2c_writer(
                           dh->fh,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );
   TRCMSGTG(tg, "perform_i2c_write2() returned %d\n", rc);
   if (rc == 0) {
      call_tuned_sleep_i2c(SE_WRITE_TO_READ);
      rc = invoke_i2c_reader(dh->fh, max_read_bytes, readbuf);
      // note_io_event(IE_READ_AFTER_WRITE, __func__);
      if (rc == 0 && all_zero(readbuf, max_read_bytes)) {
         rc = DDCRC_READ_ALL_ZERO;
         // printf("(%s) All zero response.", __func__ );
         // printf("(%s) Request was: %s\n", __func__,
         //        hexstring(get_packet_start(request_packet_ptr)+1, get_packet_len(request_packet_ptr)-1));
         // COUNT_STATUS_CODE(rc);
         DDCMSG("All zero response detected in %s", __func__);
      }
   }
   if (rc < 0) {
      COUNT_STATUS_CODE(rc);
   }

   TRCMSGTG(tg, "Done. gsc=%d", rc);
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

// #ifdef NEW
Global_Status_Code ddc_adl_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            pbytes_received
     )
{
   bool debug = false;
   bool tf = IS_TRACING();
   if (debug)
      tf = true;
   TRCMSGTF(tf, "Starting. Using adl_ddc_write_only() and adl_ddc_read_only() dh=%s",
            display_handle_repr_r(dh, NULL, 0));
   ASSERT_VALID_DISPLAY_REF(dh, DDC_IO_ADL);

   Global_Status_Code gsc = 0;

   Base_Status_ADL adlrc = adl_ddc_write_only(
                              dh->iAdapterIndex,
                              dh->iDisplayIndex,
                              get_packet_start(request_packet_ptr),   // n. no adjustment, unlike i2c version
                              get_packet_len(request_packet_ptr)
                             );
   // note_io_event(IE_WRITE_BEFORE_READ, __func__);
   if (adlrc < 0) {
      TRCMSGTF(tf, "adl_ddc_write_only() returned adlrc=%d\n", adlrc);
      gsc = modulate_rc(adlrc, RR_ADL);
   }
   else {
      call_tuned_sleep_adl(SE_WRITE_TO_READ);
      adlrc = adl_ddc_read_only(
            dh->iAdapterIndex,
            dh->iDisplayIndex,
            readbuf,
            pbytes_received);
      // note_io_event(IE_READ_AFTER_WRITE, __func__);
      if (adlrc < 0) {
         TRCMSGTF(tf, "adl_ddc_read_only() returned adlrc=%d\n", adlrc);
         gsc = modulate_rc(adlrc, RR_ADL);
      }
      else {
         if ( all_zero(readbuf+1, max_read_bytes-1)) {
                 gsc = DDCRC_READ_ALL_ZERO;
                 printf("(%s) All zero response.\n", __func__ );
                 DDCMSG("All zero response.");
                 COUNT_STATUS_CODE(gsc);
         }
         else if (memcmp(get_packet_start(request_packet_ptr), readbuf, get_packet_len(request_packet_ptr)) == 0) {
            // printf("(%s) Bytes read same as bytes written.\n", __func__ );
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
   TRCMSGTF(tf, "Done. rc=%s\n", gsc_desc(gsc));
   return gsc;
}
// #endif

#ifdef OLD
Global_Status_Code ddc_adl_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            pbytes_received
     )
{
   bool debug = true;
   bool tf = IS_TRACING();
   if (debug)
      tf = true;
   TRCMSGTF(tf, "Starting. Using adl_ddc_write_read(), dh=%s", display_handle_repr_r(dh, NULL, 0));
   ASSERT_VALID_DISPLAY_REF(dh, DDC_IO_ADL);

   Global_Status_Code gsc = 0;



   int bytes_received = max_read_bytes;

   //   Base_Status_ADL adlrc = adl_ddc_write_read_onecall(  // just returns the bytes written
   Base_Status_ADL adlrc = adl_ddc_write_read(
                         dh->iAdapterIndex,
                         dh->iDisplayIndex,
                         get_packet_start(request_packet_ptr),
                         get_packet_len(request_packet_ptr),
                         readbuf,
                         &bytes_received);

   Global_Status_Code rc = modulate_rc(adlrc, RR_ADL);
   // what about positive ADL status codes?

   if (rc >= 0) {
      if ( all_zero(readbuf+1, max_read_bytes-1)) {
         rc = DDCRC_READ_ALL_ZERO;
         // printf("(%s) All zero response.\n", __func__ );
         DDCMSG("All zero response.");
         assert(rc < 0);
      }
      else if (memcmp(get_packet_start(request_packet_ptr), readbuf, get_packet_len(request_packet_ptr)) == 0) {
         // printf("(%s) Bytes read same as bytes written.\n", __func__ );
         // is this a DDC error or a programming bug?
         DDCMSG("Bytes read same as bytes written.", __func__ );
         rc = DDCRC_READ_EQUALS_WRITE;
         assert(rc < 0);
      }
      else 
         *pbytes_received = bytes_received;
   }



   TRCMSGTF(tf, "Done. rc=%s\n", gsc_desc(gsc));
   return gsc;
}
#endif


Global_Status_Code ddc_write_read_raw(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte *           readbuf,
      int *            pbytes_received
     )
{
   bool debug = false;  // override
   if (debug)
      printf("(%s) Starting.\n", __func__);
   Global_Status_Code rc;

   if (dh->ddc_io_mode == DDC_IO_DEVI2C) {
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

   if (debug)
      printf("(%s) Done, returning: %s\n", __func__, gsc_desc(rc));
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
   bool tf = IS_TRACING();
   if (debug) tf = 0xff;
   TRCMSGTF(tf, "Starting. io dh=%s", display_handle_repr_r(dh, NULL, 0) );

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
       TRCMSGTF(tf, "create_ddc_typed_response_packet() returned %s, *response_packet_ptr_loc=%p",
                ddcrc_desc(rc), *response_packet_ptr_loc );

       if (rc != 0 && *response_packet_ptr_loc) {  // paranoid,  should never occur
          free(*response_packet_ptr_loc);
       }
   }

   free(readbuf);    // or does response_packet_ptr_loc point into here?

   // already done
   // if (rc != 0) {
   //    COUNT_STATUS_CODE(rc);
   // }
   TRCMSGTF(tf, "Done. rc=%d: %s\n", rc, gsc_desc(rc) );
   if (rc == 0 && tf)
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
         DDC_Packet ** response_packet_ptr_loc
        )
{
   bool tf = IS_TRACING();
   // tf = true;
   TRCMSGTF(tf, "Starting. dh=%s", display_handle_repr_r(dh, NULL, 0)  );

   int  rc;
   int  tryctr;
   bool retryable;
   int  ddcrc_read_all_zero_ct = 0;

   for (tryctr=0, rc=-999, retryable=true;
        tryctr < max_write_read_exchange_tries && rc < 0 && retryable;
        tryctr++)
   {
      TRCMSGTF(tf,
           "Start of try loop, tryctr=%d, max_write_read_echange_tries=%d, rc=%d, retryable=%d",
           tryctr, max_write_read_exchange_tries, rc, retryable );

      rc = ddc_write_read(
                dh,
                request_packet_ptr,
                max_read_bytes,
                expected_response_type,
                expected_subtype,
                response_packet_ptr_loc);

      if (rc < 0) {     // n. ADL status codes have been modulated
         TRCMSGTF(tf, "perform_ddc_write_read() returned %d", rc );
         if (dh->ddc_io_mode == DDC_IO_DEVI2C) {
            if (rc == DDCRC_NULL_RESPONSE)
               retryable = false;
            // when is DDCRC_READ_ALL_ZERO actually an error vs the response of the monitor instead of NULL response?
            else if (rc == modulate_rc(-EIO, RR_ERRNO) || rc == DDCRC_READ_ALL_ZERO)
                retryable = true;
            else
               retryable = true;     // for now
         }   // DDC_IO_ADL
         else {
            // TODO more detailed tests
            if (rc == DDCRC_NULL_RESPONSE)
               retryable = false;
            else if (rc == DDCRC_READ_ALL_ZERO)
               retryable = true;
            else
               retryable = false;
         }
         if (rc == DDCRC_READ_ALL_ZERO)
            ddcrc_read_all_zero_ct++;
      }    // rc < 0
   }
   // n. rc is now the value from the last pass through the loop
   // set it to a DDC status code indicating max tries exceeded
   if ( rc < 0 && retryable ) {
      rc = DDCRC_RETRIES;
      if (ddcrc_read_all_zero_ct == max_write_read_exchange_tries) {
         rc = DDCRC_ALL_TRIES_ZERO;
         // printf("(%s) All tries zero ddcrc_read_all_zero_ct=%d, max_write_read_exchange_tries=%d, tryctr=%d\n",
         //        __func__, ddcrc_read_all_zero_ct, max_write_read_exchange_tries, tryctr);
      }

   }
   record_tries(write_read_stats_rec, rc, tryctr);
   TRCMSGTF(tf, "Done. rc=%s\n", gsc_desc(rc));
   return rc;
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
   bool tf = IS_TRACING();
   // tf = true;
   TRCMSGTF(tf, "Starting.");

   Global_Status_Code rc;
#ifdef OLD
   rc = perform_i2c_write2(fh,
#endif
   rc = invoke_i2c_writer(fh,
                           get_packet_len(request_packet_ptr)-1,
                           get_packet_start(request_packet_ptr)+1 );;
   if (rc < 0)
      log_status_code(rc, __func__);
   call_tuned_sleep_i2c(SE_POST_WRITE);
   TRCMSGTF(tf, "Done. rc=%d\n", rc);
   return rc;
}


/* Writes a DDC request packet to an ADL display
 *
 * Arguments:
 *   iAdapterIndex       ADL adapter number
 *   iDisplayIndex       ADL display number
 *   request_packet_ptr  DDC packet to write
 *
 * Returns:
 *   0 if success
 *   modulated ADL status code otherwise
 *
 * Do we need to deal with positive ADL status codes?
 */
Global_Status_Code ddc_adl_write_only(
          int           iAdapterIndex,
          int           iDisplayIndex,
          DDC_Packet *  request_packet_ptr
       )
{
   bool tf = IS_TRACING();
   TRCMSGTF(tf, "Starting.");
   Base_Status_ADL adlrc = adl_ddc_write_only(
                              iAdapterIndex,
                              iDisplayIndex,
                              get_packet_start(request_packet_ptr)+1,
                              get_packet_len(request_packet_ptr)-1
                             );
   Global_Status_Code rc = modulate_rc(adlrc, RR_ADL);
   TRCMSGTF(tf, "Done. rc=%d, adlrc=%d", rc, adlrc);
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
   bool tf = IS_TRACING();
   // tf = true;
   TRCMSGTF(tf, "Starting.");

   Global_Status_Code rc = 0;

   if (dh->ddc_io_mode == DDC_IO_DEVI2C) {
      rc = ddc_i2c_write_only(dh->fh, request_packet_ptr);
   }
   else {
      rc = ddc_adl_write_only(dh->iAdapterIndex, dh->iDisplayIndex, request_packet_ptr);
   }

   TRCMSGTF(tf, "Done. rc=%d\n", rc);
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
Global_Status_Code ddc_write_only_with_retry( Display_Handle * dh, DDC_Packet *   request_packet_ptr) {
   bool tf = IS_TRACING();
   tf = false;
   TRCMSGTF(tf, "Starting.");

   Global_Status_Code rc;
   int  tryctr;
   bool retryable;

   for (tryctr=0, rc=-999, retryable=true;
       tryctr < max_write_only_exchange_tries && rc < 0 && retryable;
       tryctr++)
   {
      TRCMSGTF(tf, "Start of try loop, tryctr=%d, max_write_only_exchange_tries=%d, rc=%d, retryable=%d",
               tryctr, max_write_only_exchange_tries, rc, retryable );

      rc = ddc_write_only(dh, request_packet_ptr);

      if (rc < 0) {
         if (dh->ddc_io_mode == DDC_IO_DEVI2C) {
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
   record_tries(write_only_stats_rec, rc, tryctr);

   TRCMSGTF(tf, "Done. rc=%d", rc);
   return rc;
}


