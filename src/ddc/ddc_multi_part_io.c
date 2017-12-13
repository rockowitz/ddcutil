/* ddc_multi_part_io.c
 *
 * Handles multi-part reads and writes used for Table features and
 * Capabilities.
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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
 * Multi-part DDC reads and writes used for Table features and
 * Capabilities retrieval.
 */

/** \cond */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/string_util.h"
/** \endcond */

#include "base/core.h"
#include "base/ddc_error.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/execution_stats.h"
#include "base/parms.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_try_stats.h"

#include "ddc/ddc_multi_part_io.h"


// Trace management

static Trace_Group TRACE_GROUP = TRC_DDC;


// Retry management and statistics

/** \def MAX_MAX_MULTI_EXCHANGE_TRIES
 * Maximum value to which maximum number of capabilities exchange tries can be set
 */
#define MAX_MAX_MULTI_EXCHANGE_TRIES  MAX_MAX_TRIES     /* from parms.h */

/** \def Maximum number of capabilities exchange tries allowed. Can be adjusted. */
static int max_multi_part_read_tries = MAX_MULTI_EXCHANGE_TRIES;

static int max_multi_part_write_tries = MAX_MULTI_EXCHANGE_TRIES;

static void * multi_part_read_stats_rec = NULL;
static void * multi_part_write_stats_rec = NULL;

/** Resets the statistics for multi-part reads */
void ddc_reset_multi_part_read_stats() {
   if (multi_part_read_stats_rec)
      try_data_reset(multi_part_read_stats_rec);
   else
      multi_part_read_stats_rec = try_data_create("multi-part read exchange", max_multi_part_read_tries);
}

/** Resets the statistics for multi-part writes */
void ddc_reset_multi_part_write_stats() {
   if (multi_part_write_stats_rec)
      try_data_reset(multi_part_write_stats_rec);
   else
      multi_part_write_stats_rec = try_data_create("multi-part write exchange", max_multi_part_write_tries);
}

/** Reports the statistics for multi-part reads */
void ddc_report_multi_part_read_stats(int depth) {
   assert(multi_part_read_stats_rec);
   try_data_report(multi_part_read_stats_rec, depth);
}

/** Reports the statistics for multi-part writes */
void ddc_report_multi_part_write_stats(int depth) {
   assert(multi_part_write_stats_rec);
   try_data_report(multi_part_write_stats_rec, depth);
}


/** Resets the maximum number of multi-part read exchange tries allowed.
 *  @param ct new maximum number, must be <= #MAX_MAX_MULTI_EXCHANGE_TRIES
 */
void ddc_set_max_multi_part_read_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_MULTI_EXCHANGE_TRIES);
   max_multi_part_read_tries = ct;
   if (multi_part_read_stats_rec)
         try_data_set_max_tries(multi_part_read_stats_rec, ct);
}

/** Resets the maximum number of multi-part write exchange tries allowed.
 *  @param ct new maximum number, must be <= #MAX_MAX_MULTI_EXCHANGE_TRIES
 */
void ddc_set_max_multi_part_write_tries(int ct) {
   assert(ct > 0 && ct <= MAX_MAX_MULTI_EXCHANGE_TRIES);
   max_multi_part_write_tries = ct;
   if (multi_part_write_stats_rec)
         try_data_set_max_tries(multi_part_write_stats_rec, ct);
}


/** Gets the current maximum number of multi-part read exchange tries allowed
  * @return maximum number of tries
  */
int ddc_get_max_multi_part_read_tries() {
   return max_multi_part_read_tries;
}

/** Gets the current maximum number of multi-part write exchange tries allowed
  * @return maximum number of tries
  */
int ddc_get_max_multi_part_write_tries() {
   return max_multi_part_write_tries;
}


/** Makes one attempt to read the entire capabilities string or table feature value
*
* @param  dh             display handle for open i2c or adl device
* @param  request_type   DDC_PACKET_TYPE_CAPABILITIES_REQUEST or DDC_PACKET_TYPE_TABLE_REQD_REQUEST
* @param  request_subtype  VCP feature code for table read, ignore for capabilities
* @param  all_zero_response_ok  if true, an all zero response is not regarded
*         as an error
* @param  accumulator    buffer in which to return result
*
* @return status code
*/
Error_Info *
try_multi_part_read(
      Display_Handle * dh,
      Byte             request_type,
      Byte             request_subtype,
      bool             all_zero_response_ok,
      Buffer *         accumulator)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting. request_type=0x%02x, request_subtype=x%02x, all_zero_response_ok=%s, accumulator=%p",
          request_type, request_subtype, all_zero_response_ok, accumulator);

   const int MAX_FRAGMENT_SIZE = 32;
   const int readbuf_size = 6 + MAX_FRAGMENT_SIZE + 1;

   Public_Status_Code psc = 0;
   Error_Info * excp = NULL;

   DDC_Packet * request_packet_ptr  = NULL;
   DDC_Packet * response_packet_ptr = NULL;
   request_packet_ptr = create_ddc_multi_part_read_request_packet(
                           request_type, request_subtype, 0, "try_multi_part_read");
   buffer_set_length(accumulator,0);
   int  cur_offset = 0;
   bool complete   = false;
   while (!complete && !excp) {         // loop over fragments
      DBGTRC(debug, TRC_NONE, "Top of fragment loop");

      int fragment_size;
      update_ddc_multi_part_read_request_packet_offset(request_packet_ptr, cur_offset);
      response_packet_ptr = NULL;
      Byte expected_response_type = (request_type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST)
                                       ? DDC_PACKET_TYPE_CAPABILITIES_RESPONSE
                                       : DDC_PACKET_TYPE_TABLE_READ_RESPONSE;
      Byte expected_subtype = request_subtype;     // 0x00 for capabilities, VCP feature code for table read
      excp = ddc_write_read_with_retry(
           dh,
           request_packet_ptr,
           readbuf_size,
           expected_response_type,
           expected_subtype,
           all_zero_response_ok,
           &response_packet_ptr
          );
      psc = (excp) ? excp->psc : 0;
      DBGTRC(debug, TRC_NONE,
             "ddc_write_read_with_retry() request_type=0x%02x, request_subtype=0x%02x, returned %s",
             request_type, request_subtype, errinfo_summary(excp));

      if (excp) {
      // if (psc != 0) {
         if (response_packet_ptr)
            free_ddc_packet(response_packet_ptr);
         continue;
      }
      assert(response_packet_ptr);
      assert(!excp && psc == 0);

      if ( IS_TRACING_BY_FUNC_OR_FILE() || debug ) {
         DBGMSG("After try_write_read():");
         report_interpreted_multi_read_fragment(response_packet_ptr->parsed.multi_part_read_fragment, 0);
      }

      Interpreted_Multi_Part_Read_Fragment * aux_data_ptr =
         response_packet_ptr->parsed.multi_part_read_fragment;

      int display_current_offset = aux_data_ptr->fragment_offset;
      if (display_current_offset != cur_offset) {
         DBGTRC(debug, TRC_NONE,
                "display_current_offset %d != cur_offset %d", display_current_offset, cur_offset);
         psc = DDCRC_MULTI_PART_READ_FRAGMENT;
         excp = errinfo_new(psc, __func__);
         COUNT_STATUS_CODE(psc);
      }
      else {
         DBGTRC(debug, TRC_NONE, "display_current_offset = %d matches cur_offset", display_current_offset);

         fragment_size = aux_data_ptr->fragment_length;         // ***
         DBGTRC(debug, TRC_NONE, "fragment_size = %d", fragment_size);
         if (fragment_size == 0) {
            complete = true;   // redundant
         }
         else {
            buffer_append(accumulator, aux_data_ptr->bytes, fragment_size);
            cur_offset = cur_offset + fragment_size;
            if ( IS_TRACING_BY_FUNC_OR_FILE() || debug ) {
               DBGMSG("Currently assembled fragment: |%.*s|", accumulator->len, accumulator->bytes);
               DBGMSG("cur_offset = %d", cur_offset);
            }
            all_zero_response_ok = false;           // accept all zero response only on first fragment
         }
      }
      free_ddc_packet(response_packet_ptr);

   } // while loop assembling fragments

   free_ddc_packet(request_packet_ptr);

   DBGTRC(debug, TRACE_GROUP, "Returning %s", errinfo_summary(excp));
   return excp;
}


/** Gets the DDC capabilities string for a monitor, performing retries if necessary.
*
*  @param  dh handle of open display
*  @param  request_type
*  @param  request_subtype  VCP function code for table read, ignore for capabilities
*  @param  all_zero_response_ok   if true, zero response is not an error
*  @param  pp_buffer  address at which to return newly allocated #Buffer in which
*                   result is returned
*
*  @retval  NULL    success
*  @retval  #Ddc_Error containing status DDCRC_UNSUPPORTED does not support Capabilities Request
*  @retval  #Ddc_Error containing status DDCRC_TRIES  maximum retries exceeded:
*/
Error_Info *
multi_part_read_with_retry(
      Display_Handle * dh,
      Byte             request_type,
      Byte             request_subtype,   // VCP feature code for table read, ignore for capabilities
      bool             all_zero_response_ok,
      Buffer**         pp_buffer)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting.  request_type=0x%02x, request_subtype=0x%02x, all_zero_response_ok=%s"
          ", max_multi_part_read_tries=%d",
          request_type, request_subtype, bool_repr(all_zero_response_ok), max_multi_part_read_tries);

   Public_Status_Code rc = -1;   // dummy value for first call of while loop
   Error_Info * ddc_excp = NULL;
   // Public_Status_Code try_status_codes[MAX_MAX_TRIES];
   Error_Info *        try_errors[MAX_MAX_TRIES];

   int tryctr = 0;
   bool can_retry = true;
   Buffer * accumulator = buffer_new(2048, "multi part read buffer");

   while (tryctr < max_multi_part_read_tries && rc < 0 && can_retry) {
      DBGTRC(debug, TRC_NEVER,
             "Start of while loop. try_ctr=%d, max_multi_part_read_tries=%d",
             tryctr, max_multi_part_read_tries);

      ddc_excp = try_multi_part_read(
              dh,
              request_type,
              request_subtype,
              all_zero_response_ok,
              accumulator);
      try_errors[tryctr] = ddc_excp;
      rc = (ddc_excp) ? ddc_excp->psc : 0;

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
      tryctr++;
   }
   assert( (rc<0 && ddc_excp) || (rc==0 && !ddc_excp) );

   if (rc < 0) {
      buffer_free(accumulator, "capabilities buffer, error");
      accumulator = NULL;
      if (tryctr >= max_multi_part_read_tries)
         rc = DDCRC_RETRIES;
      ddc_excp = errinfo_new_with_causes(rc, try_errors, tryctr, __func__);

      if (rc != try_errors[tryctr-1]->psc)
         COUNT_STATUS_CODE(rc);     // new status code, count it
   }
   else {
      for (int ndx = 0; ndx < tryctr; ndx++)
         errinfo_free(try_errors[ndx]);
   }

   // if counts for DDCRC_ALL_TRIES_ZERO?
   try_data_record_tries(multi_part_read_stats_rec, rc, tryctr);

   *pp_buffer = accumulator;
   return ddc_excp;
}


/** Makes one attempt to write an entire VCP Table value
*
*   @param dh             display handle for open i2c or adl device
*   @param vcp_code       VCP feature code
*   @param value_to_set   Table feature value
*
*   @return status code
*/
Error_Info *
try_multi_part_write(
      Display_Handle * dh,
      Byte             vcp_code,
      Buffer *         value_to_set)
{
   bool force_debug = false;
   Byte request_type = DDC_PACKET_TYPE_TABLE_WRITE_REQUEST;
   Byte request_subtype = vcp_code;
   DBGTRC(force_debug, TRACE_GROUP,
          "Starting. request_type=0x%02x, request_subtype=x%02x, accumulator=%p",
          request_type, request_subtype, value_to_set);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   int MAX_FRAGMENT_SIZE = 32;
   int max_fragment_size = MAX_FRAGMENT_SIZE - 4;    // hack
   // const int writebbuf_size = 6 + MAX_FRAGMENT_SIZE + 1;

   DDC_Packet * request_packet_ptr  = NULL;
   int bytes_remaining = value_to_set->len;
   int offset = 0;
   while (bytes_remaining >= 0 && psc == 0) {
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
      ddc_excp = ddc_write_only_with_retry(dh, request_packet_ptr);
      psc = (ddc_excp) ? ddc_excp->psc : 0;
      free_ddc_packet(request_packet_ptr);
      assert( (!ddc_excp && psc == 0) || (ddc_excp && psc!=0) );

      if (!ddc_excp) {
         if (bytect_to_write == 0)   // if just wrote final empty segment to indicate done
            break;
         offset += bytect_to_write;
         bytes_remaining -= bytect_to_write;
      }
   }

   DBGTRC(force_debug, TRACE_GROUP, "Returning %s", psc_desc(psc));
   if ( psc == DDCRC_RETRIES && (force_debug || IS_TRACING()) )
      DBGMSG("     Try errors: %s", errinfo_causes_string(ddc_excp));
   assert( (ddc_excp && psc<0) || (!ddc_excp && psc==0) );
   return ddc_excp;
}


/** Writes a VCP table feature, with retry.
 *
 * @param  dh display handle
 * @param vcp_code  VCP feature code to write
 * @param value_to_set bytes of Table feature
 * @return  NULL if success, pointer to #Ddc_Error if failure
 */
Error_Info *
multi_part_write_with_retry(
     Display_Handle * dh,
     Byte             vcp_code,
     Buffer *         value_to_set)
{
   bool debug = false;
   if (IS_TRACING())
      puts("");
   DBGTRC(debug, TRACE_GROUP, "Starting. dh=%s, vcp_code=0x%02x",
                              dh_repr_t(dh), vcp_code);

   Public_Status_Code rc = -1;   // dummy value for first call of while loop
   Error_Info * ddc_excp = NULL;

   Error_Info *         try_errors[MAX_MAX_TRIES];

   int tryctr = 0;
   bool can_retry = true;

   while (tryctr < max_multi_part_write_tries && rc < 0 && can_retry) {
      DBGTRC(debug, TRACE_GROUP,
             "Start of while loop. try_ctr=%d, max_multi_part_write_tries=%d",
             tryctr, max_multi_part_write_tries);

      ddc_excp = try_multi_part_write(
              dh,
              vcp_code,
              value_to_set);
      try_errors[tryctr] = ddc_excp;
      rc = (ddc_excp) ? ddc_excp->psc : 0;
      assert( (ddc_excp && rc<0) || (!ddc_excp && rc==0) );

      // TODO: What rc values set can_retry = false?

      tryctr++;
   }
   assert( (ddc_excp && rc < 0) || (!ddc_excp && rc==0) );

   if (rc < 0) {
      if (can_retry)
         rc = DDCRC_RETRIES;
      ddc_excp= errinfo_new_with_causes(rc, try_errors, tryctr, __func__);

      if (rc != try_errors[tryctr-1]->psc)
         COUNT_STATUS_CODE(rc);     // new status code, count it
   }
   else {
      for (int ndx=0; ndx<tryctr; ndx++)
         errinfo_free(try_errors[ndx]);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}
