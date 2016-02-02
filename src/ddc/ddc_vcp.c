/* ddc_vcp.c
 *
 * Virtual Control Panel access
 *
 * Created on: Jun 10, 2014
 *      Author: rock
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/debug_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/msg_control.h"
#include "base/util.h"
#include "base/vcp_feature_values.h"

#include "i2c/i2c_bus_core.h"

#include "adl/adl_shim.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"

#include "ddc/ddc_vcp.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;


//
// Set VCP feature value
//

/* The workhorse for setting a new VCP feature value.
 *
 * Arguments:
 *    dh            display handle for open display
 *    feature_code  VCP feature code
 *    new_value     new value
 *
 *  Returns:
 *     status code from perform_ddc_write_only()
 */
Global_Status_Code set_nontable_vcp_value(
                      Display_Handle * dh,
                      Byte             feature_code,
                      int              new_value) {
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   TRCMSGTG(tg, "Writing feature 0x%02x , new value = %d\n", feature_code, new_value);

   DDC_Packet * request_packet_ptr =
      create_ddc_setvcp_request_packet(feature_code, new_value, "set_vcp:request packet");
   // DBGMSG("create_ddc_getvcp_request_packet returned packet_ptr=%p", request_packet_ptr);
   // dump_packet(request_packet_ptr);

   Global_Status_Code gsc = ddc_write_only_with_retry(dh, request_packet_ptr);

   if (request_packet_ptr)
      free_ddc_packet(request_packet_ptr);

   // DBGMSG("Returning %p", interpretation_ptr);
   return gsc;
}


//
// Get and show VCP values
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
Global_Status_Code get_nontable_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       Parsed_Nontable_Vcp_Response** ppInterpretedCode)
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   TRCMSGTG(tg, "Reading feature 0x%02x", feature_code);

   Global_Status_Code rc = 0;
   // Output_Level output_level = get_output_level();
   Parsed_Nontable_Vcp_Response * interpretation_ptr = NULL;

   DDC_Packet * request_packet_ptr  = NULL;
   DDC_Packet * response_packet_ptr = NULL;
   request_packet_ptr = create_ddc_getvcp_request_packet(feature_code, "get_vcp_by_DisplayRef:request packet");
   // dump_packet(request_packet_ptr);

   Byte expected_response_type = DDC_PACKET_TYPE_QUERY_VCP_RESPONSE;
   Byte expected_subtype = feature_code;
   int max_read_bytes  = 20;    // actually 3 + 8 + 1, or is it 2 + 8 + 1?

   rc = ddc_write_read_with_retry(
           dh,
           request_packet_ptr,
           max_read_bytes,
           expected_response_type,
           expected_subtype,
           false,                       // all_zero_respons_ok
           &response_packet_ptr
        );
   TRCMSGTG(tg, "perform_ddc_write_read_with_retry() returned %s", gsc_desc(rc));

   if (rc == 0) {
      interpretation_ptr = (Parsed_Nontable_Vcp_Response *) call_calloc(1, sizeof(Parsed_Nontable_Vcp_Response), "get_vcp_by_DisplayRef");

      rc = get_interpreted_vcp_code(response_packet_ptr, true /* make_copy */, &interpretation_ptr);
      //if (msgLevel >= VERBOSE)
      // if (output_level >= OL_VERBOSE)
      //    report_interpreted_nontable_vcp_response(interpretation_ptr);
   }

   if (rc == 0) {
      if (!interpretation_ptr->valid_response)  {
         rc = DDCRC_INVALID_DATA;
      }
      else if (!interpretation_ptr->supported_opcode) {
         rc = DDCRC_REPORTED_UNSUPPORTED;
      }
      if (rc != 0) {
         free(interpretation_ptr);
         interpretation_ptr = NULL;
      }
   }

   if (request_packet_ptr)
      free_ddc_packet(request_packet_ptr);
   if (response_packet_ptr)
      free_ddc_packet(response_packet_ptr);

   TRCMSGTG(tg, "Returning %s, *ppinterpreted_code=%p", gsc_name(rc), interpretation_ptr);
   *ppInterpretedCode = interpretation_ptr;
   return rc;
}


/* Gets the value of a table feature in a newly allocated Buffer struct.
 * It is the responsibility of the caller to free the Buffer.
 *
 * Arguments:
 *    dh              display handle
 *    feature_code    VCP feature code
 *    pp_table_bytes  location at which to save address of newly allocated Buffer
 *
 * Returns:
 *    status code
 */
Global_Status_Code get_table_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       Buffer**               pp_table_bytes)
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   TRCMSGTG(tg, "Starting. Reading feature 0x%02x", feature_code);

   Global_Status_Code gsc = 0;
   Output_Level output_level = get_output_level();
   Buffer * paccumulator =  NULL;

   gsc = multi_part_read_with_retry(
            dh,
            DDC_PACKET_TYPE_TABLE_READ_REQUEST,
            feature_code,
            true,                      // all_zero_response_ok
            &paccumulator);
   DBGMSF(debug, "perform_ddc_write_read_with_retry() returned %s", gsc_desc(gsc));

   if (gsc == 0) {
      *pp_table_bytes = paccumulator;
      if (output_level >= OL_VERBOSE) {
         printf("Bytes returned on table read:");
         buffer_dump(paccumulator);
      }
   }

   TRCMSGTG(tg, "Done. Returning rc=%s, *pp_table_bytes=%p", gsc_desc(gsc), *pp_table_bytes);
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
Global_Status_Code get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       VCP_Call_Type             call_type,
       Single_Vcp_Value **       pvalrec)
#ifdef OLD
       Parsed_Vcp_Response**     pp_parsed_response)
#endif
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;  if (debug) tg = 0xFF;
   TRCMSGTG(tg, "Starting. Reading feature 0x%02x", feature_code);

   Global_Status_Code gsc = 0;

   Buffer * buffer = NULL;
#ifdef OLD
   *pp_parsed_response = NULL;
   Parsed_Vcp_Response *  presp = calloc(1, sizeof(Parsed_Vcp_Response));
#endif
   Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;
   Single_Vcp_Value * valrec = NULL;
   switch (call_type) {

   case (NON_TABLE_VCP_CALL):
#ifdef OLD
         presp->response_type = NON_TABLE_VCP_CALL;
#endif
         gsc = get_nontable_vcp_value(
                  dh,
                  feature_code,
                  &parsed_nontable_response);
#ifdef OLD
         presp->non_table_response = parsed_nontable_response;
#endif
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

   case (TABLE_VCP_CALL):
#ifdef OLD
         presp->response_type = TABLE_VCP_CALL;
#endif
         gsc = get_table_vcp_value(
                 dh,
                 feature_code,
                 &buffer);
#ifdef OLD
         presp->table_response = buffer;
#endif
         if (gsc == 0) {
            valrec = create_table_vcp_value_by_buffer(feature_code, buffer);
            buffer_free(buffer, __func__);
         }
         break;
   }

#ifdef OLD
   TRCMSGTG(tg, "Done. Returning gsc=%s, presp=%p",
                gsc_desc(gsc), presp);
   if (gsc == 0) {
      assert(presp);
      assert( presp->response_type == call_type);
      if (call_type == NON_TABLE_VCP_CALL)
         assert(presp->non_table_response && !presp->table_response);
      else
         assert(!presp->non_table_response && presp->table_response);
   }
   else {
      if (presp)
         TRCMSGTG(tg, "WARNING: gsc == %s but presp=%p",
                      gsc_desc(gsc), presp);
   }
#endif

#ifdef OLD
   if (gsc == 0)
      *pp_parsed_response = presp;
#endif

   if (gsc == 0) {
#ifdef OLD
      valrec = create_single_vcp_value_by_parsed_vcp_response(
            feature_code,
            presp);
#endif
      if (debug) {
#ifdef OLD
         report_parsed_vcp_response(presp, 1);
#endif
         report_single_vcp_value(valrec,1);
      }
      *pvalrec = valrec;
   }

   TRCMSGTG(tg, "Done.  Returning: %s", gsc_desc(gsc) );
   return gsc;
}

