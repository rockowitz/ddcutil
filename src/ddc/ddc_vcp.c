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
// #include <i2c-dev.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "util/debug_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/msg_control.h"
#include "base/util.h"

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
Global_Status_Code set_nontable_vcp_value_by_display_handle(
                      Display_Handle * dh,
                      Byte             feature_code,
                      int              new_value) {
   // bool debug = false;
   // if (debug) {
   //    DBGMSG("Writing feature 0x%02x , new value = %d", feature_code, new_value);
   // }
   TRCMSG("Writing feature 0x%02x , new value = %d\n", feature_code, new_value);

   Global_Status_Code rc = 0;
   // int fd;

   DDC_Packet * request_packet_ptr = NULL;

   request_packet_ptr = create_ddc_setvcp_request_packet(feature_code, new_value, "set_vcp:request packet");
   // DBGMSG("create_ddc_getvcp_request_packet returned packet_ptr=%p", request_packet_ptr);
   // dump_packet(request_packet_ptr);

   rc = ddc_write_only_with_retry(dh, request_packet_ptr);

   if (request_packet_ptr)
      free_ddc_packet(request_packet_ptr);

   // DBGMSG("Returning %p", interpretation_ptr);
   return rc;
}


/* Sets a new VCP feature value.
 *
 * Arguments:
 *    pDisp         display reference
 *    feature_code  VCP feature code
 *    new_value     new value
 *
 *  Returns:
 *     status code from perform_ddc_write_only()
 */
Global_Status_Code set_nontable_vcp_value_by_display_ref(
                      Display_Ref * dref,
                      Byte          feature_code,
                      int           new_value) {
   // bool debug = false;
   // if (debug) {
   //    char buf[100];

   //    DBGMSG("Writing feature 0x%02x for %s, new value = %d", feature_code,
   //           displayRefShortName(pdisp, buf, 100 ), new_value);
   // }
   char buf[100];
   TRCMSG("Writing feature 0x%02x for %s, new value = %d\n", feature_code,
             display_ref_short_name_r(dref, buf, 100 ), new_value);
   Display_Handle * pDispHandle = ddc_open_display(dref, EXIT_IF_FAILURE);
   Global_Status_Code rc = set_nontable_vcp_value_by_display_handle( pDispHandle, feature_code, new_value);
   ddc_close_display(pDispHandle);
   return rc;
}


#ifdef OLD
/* Similar to set_vcp_by_display_ref(), but specifies the feature using
 * a VCP feature table entry.
 */
// corresponds to show_vcp
Global_Status_Code put_vcp_by_display_ref(Display_Ref * pdisp, VCP_Feature_Table_Entry * vcp_entry, int new_value) {
   Byte vcp_code = vcp_entry->code;
   // char * feature_name = vcp_entry->name;
   // printf("\nSetting new value for VCP code 0x%02x - %s:\n", vcp_code, feature_name);

   Global_Status_Code rc = set_nontable_vcp_value_by_display_ref(pdisp, vcp_code, new_value);

   if (rc != 0) {
      printf("Setting value failed. rc=%d: %s\n", rc , gsc_desc(rc));
   }

   return rc;
   // DBGMSG("Done");
}
#endif


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
Global_Status_Code get_nontable_vcp_value_by_display_handle(
       Display_Handle *       dh,
       Byte                   feature_code,
       Preparsed_Nontable_Vcp_Response** ppInterpretedCode)
{
   // char buf0[100];
   // bool debug = false;

   // if (debug)
   //    printf("(%s) Reading feature 0x%02x for %s\n",
   //       __func__,
   //       feature_code,
   //       displayRefShortName(pDisp, buf0, 100)
   //       );

   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug) tg = 0xFF;
   // DBGMSG("Reading feature 0x%02x", feature_code);
   TRCMSGTG(tg, "Reading feature 0x%02x", feature_code);

   Global_Status_Code rc = 0;
   // Output_Level output_level = get_output_level();
   Preparsed_Nontable_Vcp_Response * interpretation_ptr = NULL;

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
           &response_packet_ptr
        );
   // if (debug)
   //    DBGMSG("perform_ddc_write_read_with_retry() returned %d", rc);
   TRCMSGTG(tg, "perform_ddc_write_read_with_retry() returned %s\n", gsc_desc(rc));

   if (rc == 0) {
      interpretation_ptr = (Preparsed_Nontable_Vcp_Response *) call_calloc(1, sizeof(Preparsed_Nontable_Vcp_Response), "get_vcp_by_DisplayRef");

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

   // if (debug)
   //    DBGMSG("Returning %p", interpretation_ptr);
   TRCMSGTG(tg, "Returning %p\n", __func__, interpretation_ptr);
   *ppInterpretedCode = interpretation_ptr;
   return rc;
}


/* Gets the value of a table feature in a newly allocated Buffer struct.
 * It is the responsibility of the caller to free the Buffer.
 *
 * Arguments:
 *    dh         display handle
 *    feature_code   VCP feature code
 *    pp_table_bytes save address of newly allocated Buffer struct here
 *
 * Returns:
 *    status code
 */
Global_Status_Code get_table_vcp_value_by_display_handle(
       Display_Handle *       dh,
       Byte                   feature_code,
       Buffer**               pp_table_bytes)
{
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug) tg = 0xFF;
   TRCMSGTG(tg, "Starting. Reading feature 0x%02x", feature_code);

   Global_Status_Code gsc = 0;
   Output_Level output_level = get_output_level();
   Buffer * paccumulator =  NULL;

   gsc = multi_part_read_with_retry(
            dh,
            DDC_PACKET_TYPE_TABLE_READ_REQUEST,
            feature_code,
            &paccumulator);
   TRCMSGTG(tg, "perform_ddc_write_read_with_retry() returned %s", gsc_desc(gsc));

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


Global_Status_Code get_nontable_vcp_value_by_display_ref(
                      Display_Ref *          pDisp,
                      Byte                   feature_code,
                      Preparsed_Nontable_Vcp_Response** ppInterpretedCode) {
   char buf0[100];

   // bool debug = false;

   TRCMSG("Reading feature 0x%02x for %s\n",
         feature_code,
         display_ref_short_name_r(pDisp, buf0, 100)
         );

   Display_Handle * pDispHandle = ddc_open_display(pDisp, EXIT_IF_FAILURE);
   Global_Status_Code rc = get_nontable_vcp_value_by_display_handle(pDispHandle, feature_code, ppInterpretedCode);
   ddc_close_display(pDispHandle);

   TRCMSG("Returning %d\n", __func__, rc);
   return rc;
}

