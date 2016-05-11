/* ddc_multi_part_io.c
 *
 * Handles multi-part reads and writes used for Table features and
 * Capabilities.
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/execution_stats.h"
#include "base/parms.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/try_stats.h"

#include "ddc/ddc_multi_part_io.h"


// Trace management

static Trace_Group TRACE_GROUP = TRC_DDC;


// Retry management and statistics

// Maximum value to which maximum number of capabilities exchange tries can be set
#define MAX_MAX_MULTI_EXCHANGE_TRIES  MAX_MAX_TRIES     /* from parms.h */

// Maximum number of capabilities exchange tries allowed. Can be adjusted.
static int max_multi_part_read_tries = MAX_MULTI_EXCHANGE_TRIES;

static void * multi_part_read_stats_rec = NULL;

void ddc_reset_multi_part_read_stats() {
   if (multi_part_read_stats_rec)
      try_data_reset(multi_part_read_stats_rec);
   else
      multi_part_read_stats_rec = try_data_create("multi-part exchange", max_multi_part_read_tries);
}

void ddc_report_multi_part_read_stats() {
   assert(multi_part_read_stats_rec);
   try_data_report(multi_part_read_stats_rec);
}


// Resets the maximum number of capabilities exchange tries allowed
void ddc_set_max_multi_part_read_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_MULTI_EXCHANGE_TRIES);
   max_multi_part_read_tries = ct;
   if (multi_part_read_stats_rec)
         try_data_set_max_tries(multi_part_read_stats_rec, ct);
}

// Gets the maximum number of capabilities exchange tries allowed
int ddc_get_max_multi_part_read_tries() {
   return max_multi_part_read_tries;
}


/* Makes one attempt to read the entire capabilities string or table feature value
*
* Arguments:
*   dh             display handle for open i2c or adl device
*   capabilities   address of buffer in which to return response
*
* Returns:         status code
*/
Global_Status_Code
try_multi_part_read(
      Display_Handle * dh,
      Byte             request_type,
      Byte             request_subtype,
      bool             all_zero_response_ok,
      Buffer *         accumulator)
{
   bool force_debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (force_debug)
      tg = 0xFF;  // force tracing
   TRCMSGTG(tg, "Starting. request_type=0x%02x, request_subtype=x%02x, accumulator=%p",
            request_type, request_subtype, accumulator);

   Global_Status_Code rc = 0;
   const int MAX_FRAGMENT_SIZE = 32;
   const int readbuf_size = 6 + MAX_FRAGMENT_SIZE + 1;

   DDC_Packet * request_packet_ptr  = NULL;
   DDC_Packet * response_packet_ptr = NULL;
   // request_packet_ptr = create_ddc_capabilities_request_packet(0, "try_multi_part_read");
   request_packet_ptr = create_ddc_multi_part_read_request_packet(
                           request_type, request_subtype, 0, "try_multi_part_read");
   buffer_set_length(accumulator,0);
   int  cur_offset = 0;
   bool complete   = false;
   while (!complete && rc == 0) {         // loop over fragments
      // if ( IS_TRACING() || force_debug )
      //    puts("");
      DBGMSF(force_debug, "Top of fragment loop");

      int fragment_size;
      update_ddc_multi_part_read_request_packet_offset(request_packet_ptr, cur_offset);
      response_packet_ptr = NULL;
      Byte expected_response_type = (request_type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST)
                                       ? DDC_PACKET_TYPE_CAPABILITIES_RESPONSE
                                       : DDC_PACKET_TYPE_TABLE_READ_RESPONSE;
      Byte expected_subtype = request_subtype;     // 0x00 for capabilities, VCP feature code for table read
      rc = ddc_write_read_with_retry(
           dh,
           request_packet_ptr,
           readbuf_size,
           expected_response_type,
           expected_subtype,
           all_zero_response_ok,
           &response_packet_ptr
        );
      DBGMSF(force_debug,
             "ddc_write_read_with_retry() request_type=0x%02x, request_subtype=0x%02x, returned %s",
             request_type, request_subtype, gsc_desc( rc));

      if (rc != 0) {
         if (response_packet_ptr)
            free_ddc_packet(response_packet_ptr);
         break;
      }

      if ( IS_TRACING() || force_debug ) {
         DBGMSF(force_debug, "After try_write_read():");
         report_interpreted_multi_read_fragment(response_packet_ptr->aux_data);
      }

      Interpreted_Multi_Part_Read_Fragment * aux_data_ptr =
         (Interpreted_Multi_Part_Read_Fragment *) response_packet_ptr->aux_data;    // ***
      int display_current_offset = aux_data_ptr->fragment_offset;
      if (display_current_offset != cur_offset) {
         DBGMSF(force_debug, "display_current_offset %d != cur_offset %d",
                display_current_offset, cur_offset);
         rc = DDCRC_MULTI_PART_READ_FRAGMENT;                       // ***
         COUNT_STATUS_CODE(rc);
         free_ddc_packet(response_packet_ptr);
         break;
      }
      // DBGMSG("display_current_offset = %d matches cur_offset", display_current_offset);

      fragment_size = aux_data_ptr->fragment_length;         // ***
      DBGMSF(force_debug, "fragment_size = %d", fragment_size);
      if (fragment_size == 0) {
         complete = true;   // redundant
         free_ddc_packet(response_packet_ptr);
         break;
      }
      // cur_offset = (readbuf[5] << 8) | readbuf[4];
      buffer_append(accumulator, aux_data_ptr->bytes, fragment_size);   // ***

      cur_offset = cur_offset + fragment_size;
      DBGMSF(force_debug, "Currently assembled fragment: |%.*s|",accumulator->len, accumulator->bytes);   // ***
      DBGMSF(force_debug, "cur_offset = %d", cur_offset);

      free_ddc_packet(response_packet_ptr);
      all_zero_response_ok = false;              // accept all zero response only on first fragment
   } // while loop assembling fragments

   free_ddc_packet(request_packet_ptr);

   if (rc > 0)
      rc = 0;
   if (rc == 0) {
        // NO, only works if accumulator contains string
        // unsigned char null_byte = 0x00;
        // buffer_append(accumulator, &null_byte, 1);                       // ***
        // TRCMSGTG(tg, "Returning capabilities: %s",accumulator->bytes);
   }

   TRCMSGTG(tg, "Returning %s", gsc_desc(rc));
   return rc;
}


/* Gets the DDC capabilities string for a monitor, performing retries if necessary.
*
* Arguments:
*   pdisp                display reference
*   ppCapabilitesBuffer  address at which to return address of newly created
*                        response buffer if successful,
*                        set to NULL if unsuccessful
*
* Returns:
*   0 if success
*   DDCRC_UNSUPPORTED   monitor does not support get capabilities request
*   DDCRC_TRIES         maximum retries exceeded
*/
Global_Status_Code
multi_part_read_with_retry(
      Display_Handle * dh,
      Byte             request_type,
      Byte             request_subtype,   // VCP feature code for table read, ignore for capabilities
      bool             all_zero_response_ok,
      Buffer**         ppbuffer)
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug)
      tg = 0xFF;
   // char buf[100];
   if (IS_TRACING())
      puts("");
   // TODO: fix:
   // TRCMSGTG(tg, "Starting. pdisp = %s", display_ref_short_name(pdisp, buf, 100) );

   Global_Status_Code rc = -1;   // dummy value for first call of while loop

   int try_ctr = 0;
   bool can_retry = true;
   Buffer * accumulator = buffer_new(2048, "multi part read buffer");

   while (try_ctr < max_multi_part_read_tries && rc < 0 && can_retry) {
      TRCMSGTG(tg, "Start of while loop. try_ctr=%d, max_multi_part_read_tries=%d",
               try_ctr, max_multi_part_read_tries);

      rc = try_multi_part_read(
              dh,
              request_type,
              request_subtype,
              all_zero_response_ok,
              accumulator);
      if (rc == DDCRC_NULL_RESPONSE) {
         // generally means this, but could conceivably indicate a protocol error.
         // try multiple times to ensure it's really unsupported?

         // just pass DDCRC_NULL_RESPONSE up the chain
         // rc = DDCRC_DETERMINED_UNSUPPORTED;
         // COUNT_STATUS_CODE(rc);   // double counting?

         can_retry = false;
      }
      else if (rc == DDCRC_READ_ALL_ZERO) {
         can_retry = false;

         // just pass DDCRC_READ_ALL_ZERO up the chain:
         // rc = DDCRC_DETERMINED_UNSUPPORTED;    // ??
         // COUNT_STATUS_CODE(rc);   // double counting?
      }
      else if (rc == DDCRC_ALL_TRIES_ZERO) {
         can_retry = false;

         // just pass it up
         // rc = DDCRC_DETERMINED_UNSUPPORTED;    // ??
         // COUNT_STATUS_CODE(rc);   // double counting?
      }
      try_ctr++;
   }
   if (rc < 0) {
      buffer_free(accumulator, "capabilities buffer, error");
      accumulator = NULL;
      if (try_ctr >= max_multi_part_read_tries) {
         rc = DDCRC_RETRIES;
      }
   }

   // if counts for DDCRC_ALL_TRIES_ZERO?
   try_data_record_tries(multi_part_read_stats_rec, rc, try_ctr);
   *ppbuffer = accumulator;
   return rc;
}



/* Makes one attempt to write an entire table value
*
* Arguments:
*   dh             display handle for open i2c or adl device
*   capabilities   address of buffer in which to return response
*
* Returns:         status code
*/
Global_Status_Code
try_multi_part_write(
      Display_Handle * dh,
      Byte             vcp_code,
      Buffer *         value_to_set)
{
   bool force_debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (force_debug)
      tg = 0xFF;  // force tracing
   Byte request_type = DDC_PACKET_TYPE_TABLE_WRITE_REQUEST;
   Byte request_subtype = vcp_code;
   TRCMSGTG(tg, "Starting. request_type=0x%02x, request_subtype=x%02x, accumulator=%p",
            request_type, request_subtype, value_to_set);



   Global_Status_Code rc = 0;
   int MAX_FRAGMENT_SIZE = 32;
   int max_fragment_size = MAX_FRAGMENT_SIZE - 4;    // hack
   // const int writebbuf_size = 6 + MAX_FRAGMENT_SIZE + 1;

   DDC_Packet * request_packet_ptr  = NULL;
   int bytes_remaining = value_to_set->len;
   int offset = 0;
   while (bytes_remaining >= 0 && rc == 0) {
      int bytect_to_write = (bytes_remaining <= max_fragment_size)
                                    ? bytes_remaining
                                    : max_fragment_size;
      request_packet_ptr =  create_ddc_multi_part_write_request_packet(
                   DDC_PACKET_TYPE_TABLE_WRITE_REQUEST,
                   vcp_code,       // request_subtype,
                   offset,
                   value_to_set->bytes+offset,
                   bytect_to_write,
                   __func__);
      rc = ddc_write_only_with_retry(dh, request_packet_ptr);
      free_ddc_packet(request_packet_ptr);

      if (rc == 0) {
         if (bytect_to_write == 0)   // if just wrote fine empty segment to indicate done
            break;
         offset += bytect_to_write;
         bytes_remaining -= bytect_to_write;
      }
   }

   TRCMSGTG(tg, "Returning %s", gsc_desc(rc));
   return rc;
}

Global_Status_Code
multi_part_write_with_retry(
     Display_Handle * dh,
     Byte             vcp_code,
     Buffer *         value_to_set)
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug)
      tg = 0xFF;
   // char buf[100];
   if (IS_TRACING())
      puts("");
   // TODO: fix:
   // TRCMSGTG(tg, "Starting. pdisp = %s", display_ref_short_name(pdisp, buf, 100) );

   Global_Status_Code rc = -1;   // dummy value for first call of while loop

   int try_ctr = 0;
   bool can_retry = true;

   while (try_ctr < max_multi_part_read_tries && rc < 0 && can_retry) {
      TRCMSGTG(tg, "Start of while loop. try_ctr=%d, max_multi_part_read_tries=%d",
               try_ctr, max_multi_part_read_tries);

      rc = try_multi_part_write(
              dh,
              vcp_code,
              value_to_set);

      // TODO: What rc values set can_retry = false?

      try_ctr++;
   }

   return rc;
}
