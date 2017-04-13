/* ddc_vcp.c
 *
 * Virtual Control Panel access
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
 *
 */

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/report_util.h"

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"

#include "i2c/i2c_bus_core.h"

#include "adl/adl_shim.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#include "usb/usb_vcp.h"
#endif

#include "vcp/vcp_feature_codes.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"

#include "ddc/ddc_vcp.h"


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_DDC;


//
// Set VCP feature value
//

/* Sets a non-table VCP feature value.
 *
 * Arguments:
 *    dh            display handle for open display
 *    feature_code  VCP feature code
 *    new_value     new value
 *
 *  Returns:
 *     status code from ddc_write_only_with_retry()
 */
Public_Status_Code
set_nontable_vcp_value(
      Display_Handle * dh,
      Byte             feature_code,
      int              new_value)
{
   bool debug = false;
   // Trace_Group tg = (debug) ? 0xFF : TRACE_GROUP;
   // TRCMSGTG(tg, "Writing feature 0x%02x , new value = %d\n", feature_code, new_value);
   DBGTRC(debug, TRACE_GROUP,
          "Writing feature 0x%02x , new value = %d, dh=%s\n",
          feature_code, new_value, display_handle_repr(dh));
   Public_Status_Code psc = 0;

   if (dh->dref->io_mode == DDCA_IO_USB) {
#ifdef USE_USB
      psc = usb_set_nontable_vcp_value(dh, feature_code, new_value);
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      DDC_Packet * request_packet_ptr =
         create_ddc_setvcp_request_packet(feature_code, new_value, "set_vcp:request packet");
      // DBGMSG("create_ddc_getvcp_request_packet returned packet_ptr=%p", request_packet_ptr);
      // dump_packet(request_packet_ptr);

      psc = ddc_write_only_with_retry(dh, request_packet_ptr);

      if (request_packet_ptr)
         free_ddc_packet(request_packet_ptr);
   }

   // TRCMSGTG(tg, "Returning %s", gsc_desc(gsc));
   DBGTRC(debug, TRACE_GROUP, "Returning %s", psc_desc(psc));
   return psc;
}


/* Sets a table VCP feature value.
 *
 * Arguments:
 *    dh            display handle for open display
 *    feature_code  VCP feature code
 *    bytes         pointer to table bytes
 *    bytect        number of bytes
 *
 *  Returns:
 *     status code (currently DDCL_UNIMPLEMENTED)
 */
Public_Status_Code
set_table_vcp_value(
      Display_Handle *  dh,
      Byte              feature_code,
      Byte *            bytes,
      int               bytect)
{
   bool debug = false;
   // Trace_Group tg = (debug) ? 0xFF : TRACE_GROUP;
   // TRCMSGTG(tg, "Writing feature 0x%02x , bytect = %d\n", feature_code, bytect);
   DBGTRC(debug, TRACE_GROUP, "Writing feature 0x%02x , bytect = %d\n", feature_code, bytect);
   Public_Status_Code psc = 0;


   if (dh->dref->io_mode == DDCA_IO_USB) {
#ifdef USE_USB
      psc = DDCL_UNIMPLEMENTED;
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      // TODO: clean up function signatures
      // pointless wrapping in a Buffer just to unwrap
      Buffer * new_value = buffer_new_with_value(bytes, bytect, __func__);

      psc = multi_part_write_with_retry(dh, feature_code, new_value);

      buffer_free(new_value, __func__);
   }
   // TRCMSGTG(tg, "Returning: %s", gsc_desc(gsc));
   DBGTRC(debug, TRACE_GROUP, "Returning: %s", psc_desc(psc));
   return psc;
}


static bool verify_setvcp = false;


void set_verify_setvcp(bool onoff) {
   bool debug = false;
   DBGMSF(debug, "Setting verify_setvcp = %s", bool_repr(onoff));
   verify_setvcp = onoff;
}

bool get_verify_setvcp() {
   return verify_setvcp;
}


bool
is_rereadable_feature(
      Display_Handle * dh,
      DDCA_Vcp_Feature_Code opcode)
{
   bool debug = false;
   DBGMSF(debug, "Starting opcode = 0x%02x", opcode);
   bool result = false;

   // readable features that should not be read after write
   DDCA_Vcp_Feature_Code unrereadable_features[] = {
         0x02,        // new control value
         0x03,        // soft controls
         0x60,        // input source ???
   };

   VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid(opcode);
   DBGMSF(debug, "vfte=%p", vfte);
   if (vfte) {
      assert(opcode < 0xe0);
      DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);  // ensure dh->vcp_version set
      DBGMSF(debug, "vspec = %d.%d", vspec.major, vspec.minor);
      // hack, make a guess
      if ( vcp_version_eq(vspec, VCP_SPEC_UNKNOWN)   ||
           vcp_version_eq(vspec, VCP_SPEC_UNQUERIED ))
         vspec = VCP_SPEC_V22;

      // if ( !vcp_version_eq(vspec, VCP_SPEC_UNKNOWN) &&
      //      !vcp_version_eq(vspec, VCP_SPEC_UNQUERIED ))
      // {
         result = is_feature_readable_by_vcp_version(vfte, vspec);
         DBGMSF(debug, "vspec=%d.%d, readable feature = %s", vspec.major, vspec.minor, bool_repr(result));
      // }
   }
   if (result) {
      for (int ndx = 0; ndx < ARRAY_SIZE(unrereadable_features); ndx++) {
         if ( unrereadable_features[ndx] == opcode ) {
            result = false;
            DBGMSF(debug, "Unreadable opcode");
            break;
         }
      }
   }

   DBGMSF(debug, "Returning: %s", bool_repr(result));
   return result;
}


bool single_vcp_value_equal(
      DDCA_Single_Vcp_Value * vrec1,
      DDCA_Single_Vcp_Value * vrec2)
{
   bool debug = false;

   bool result = false;
   if (vrec1->opcode     == vrec2->opcode &&
       vrec1->value_type == vrec2->value_type)
   {
      switch(vrec1->value_type) {
      case(DDCA_NON_TABLE_VCP_VALUE):
            // only check SL byte which would be set for any VCP, monitor
            result = (vrec1->val.nc.sl == vrec2->val.nc.sl);
            break;
      case(DDCA_TABLE_VCP_VALUE):
            result = (vrec1->val.t.bytect == vrec2->val.t.bytect) &&
                     (memcmp(vrec1->val.t.bytes, vrec2->val.t.bytes, vrec1->val.t.bytect) == 0 );
      }
   }
   DBGMSF(debug, "Returning: %s", bool_repr(result));
   return result;
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
Public_Status_Code
set_vcp_value(
      Display_Handle *        dh,
      DDCA_Single_Vcp_Value * vrec)
{
   bool debug = false;
   DBGMSF(debug, "Starting");

   Public_Status_Code psc = 0;
   if (vrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      psc = set_nontable_vcp_value(dh, vrec->opcode, vrec->val.c.cur_val);
   }
   else {
      assert(vrec->value_type == DDCA_TABLE_VCP_VALUE);
      psc = set_table_vcp_value(dh, vrec->opcode, vrec->val.t.bytes, vrec->val.t.bytect);
   }

   if (psc == 0 && verify_setvcp) {
      if (is_rereadable_feature(dh, vrec->opcode) ) {
         fprintf(FOUT, "Verifying that value of feature 0x%02x successfully set...\n", vrec->opcode);
         DDCA_Single_Vcp_Value * newval = NULL;
         psc = get_vcp_value(
             dh,
             vrec->opcode,
             vrec->value_type,
             &newval);
         if (psc != 0) {
            f0printf(FOUT, "Read after write failed. get_vcp_value() returned: %s\n", psc_desc(psc));
            psc = DDCRC_VERIFY;
         }
         else {
            if (! single_vcp_value_equal(vrec,newval)) {
               psc = DDCRC_VERIFY;
               f0printf(FOUT, "Current value does not match value set.\n");
            }
            else {
               f0printf(FOUT, "Verification succeeded\n");
            }
         }
      }
      else {
         fprintf(FOUT, "Feature 0x%02x does not support verification\n", vrec->opcode);
         // rpt_vstring(0, "Feature 0x%02x does not support verification", vrec->opcode);
      }


   }

   DBGMSF(debug, "Returning: %s", psc_desc(psc));
   return psc;
}


//
// Get VCP values
//

/* Gets the value for a non-table feature.
 *
 * Arguments:
 *   dh                 handle for open display
 *   feature_code
 *   ppInterpretedCode  where to return parsed response
 *
 * Returns:
 *   status code
 *
 * It is the responsibility of the caller to free the parsed response.
 */
Public_Status_Code get_nontable_vcp_value(
       Display_Handle *               dh,
       Byte                           feature_code,
  //   bool                           retry_null_response,
       Parsed_Nontable_Vcp_Response** ppInterpretedCode)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Reading feature 0x%02x",
                              feature_code);

   Public_Status_Code psc = 0;
   // Output_Level output_level = get_output_level();
   Parsed_Nontable_Vcp_Response * parsed_response = NULL;

   DDC_Packet * request_packet_ptr  = NULL;
   DDC_Packet * response_packet_ptr = NULL;
   request_packet_ptr = create_ddc_getvcp_request_packet(
                           feature_code, "get_vcp_by_DisplayRef:request packet");
   // dump_packet(request_packet_ptr);

   Byte expected_response_type = DDC_PACKET_TYPE_QUERY_VCP_RESPONSE;
   Byte expected_subtype = feature_code;
   int max_read_bytes  = 20;    // actually 3 + 8 + 1, or is it 2 + 8 + 1?

   psc = ddc_write_read_with_retry(
           dh,
           request_packet_ptr,
           max_read_bytes,
           expected_response_type,
           expected_subtype,
           false,                       // all_zero_response_ok
       //  retry_null_response,
           &response_packet_ptr
        );
   // TRCMSGTG(tg, "perform_ddc_write_read_with_retry() returned %s", psc_desc(psc));
   if (debug || IS_TRACING() ) {
      if (psc != 0)
         DBGMSG("perform_ddc_write_read_with_retry() returned %s", psc_desc(psc));
   }

   if (psc == 0) {
      // ??? why is this allocated?  it's discarded by get_interpreted_vcp_code()?
      parsed_response = (Parsed_Nontable_Vcp_Response *) calloc(1, sizeof(Parsed_Nontable_Vcp_Response));

      psc = get_interpreted_vcp_code(response_packet_ptr, true /* make_copy */, &parsed_response);   // ???
      //if (msgLevel >= VERBOSE)
      // if (output_level >= OL_VERBOSE)
      //    report_interpreted_nontable_vcp_response(interpretation_ptr);
   }

   if (psc == 0) {
      if (!parsed_response->valid_response)  {
         psc = DDCRC_INVALID_DATA;
      }
      else if (!parsed_response->supported_opcode) {
         psc = DDCRC_REPORTED_UNSUPPORTED;
      }
      if (psc != 0) {
         free(parsed_response);
         parsed_response = NULL;
      }
   }

   if (request_packet_ptr)
      free_ddc_packet(request_packet_ptr);
   if (response_packet_ptr)
      free_ddc_packet(response_packet_ptr);

   DBGTRC(debug, TRACE_GROUP,
          "Returning %s, *ppinterpreted_code=%p", psc_desc(psc), parsed_response);
   *ppInterpretedCode = parsed_response;
   return psc;
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
Public_Status_Code get_table_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       Buffer**               pp_table_bytes)
{
   bool debug = false;
   // Trace_Group tg = (debug) ? 0xff : TRACE_GROUP;
   // TRCMSGTG(tg, "Starting. Reading feature 0x%02x", feature_code);
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x", feature_code);

   Public_Status_Code psc = 0;
   DDCA_Output_Level output_level = get_output_level();
   Buffer * paccumulator =  NULL;

   psc = multi_part_read_with_retry(
            dh,
            DDC_PACKET_TYPE_TABLE_READ_REQUEST,
            feature_code,
            true,                      // all_zero_response_ok
            &paccumulator);
   if (debug || psc != 0) {
      DBGTRC(debug, TRACE_GROUP,
             "perform_ddc_write_read_with_retry() returned %s", psc_desc(psc));
   }

   if (psc == 0) {
      *pp_table_bytes = paccumulator;
      if (output_level >= DDCA_OL_VERBOSE) {
         printf("Bytes returned on table read:");
         buffer_dump(paccumulator);
      }
   }

   // TRCMSGTG(tg, "Done. Returning rc=%s, *pp_table_bytes=%p", gsc_desc(gsc), *pp_table_bytes);
   DBGTRC(debug, TRACE_GROUP,
          "Done. Returning rc=%s, *pp_table_bytes=%p", psc_desc(psc), *pp_table_bytes);
   return psc;
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
Public_Status_Code
get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       DDCA_Vcp_Value_Type       call_type,
       DDCA_Single_Vcp_Value **  pvalrec)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x, dh=%s, dh->fh=%d",
            feature_code, display_handle_repr(dh), dh->fh);

  // bool retry_null_response = true;   // *** TEMP ***

   Public_Status_Code psc = 0;

   Buffer * buffer = NULL;
   Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;  // vs interpreted ..
   DDCA_Single_Vcp_Value * valrec = NULL;

   // why are we coming here for USB?
   if (dh->dref->io_mode == DDCA_IO_USB) {
#ifdef USE_USB
      DBGMSF(debug, "USB case");

      switch (call_type) {

          case (DDCA_NON_TABLE_VCP_VALUE):
                psc = usb_get_nontable_vcp_value(
                      dh,
                      feature_code,
                      &parsed_nontable_response);    //
                if (psc == 0) {
                   valrec = create_nontable_vcp_value(
                               feature_code,
                               parsed_nontable_response->mh,
                               parsed_nontable_response->ml,
                               parsed_nontable_response->sh,
                               parsed_nontable_response->sl);
                   free(parsed_nontable_response);
                }
                break;

          case (DDCA_TABLE_VCP_VALUE):
                psc = DDCRC_REPORTED_UNSUPPORTED;
                break;
          }
#else
      PROGRAM_LOGIC_ERROR("ddcutil not build with USB support");
#endif
   }
   else {
      switch (call_type) {

      case (DDCA_NON_TABLE_VCP_VALUE):
            psc = get_nontable_vcp_value(
                     dh,
                     feature_code,
                 //     retry_null_response,
                     &parsed_nontable_response);
            if (psc == 0) {
               valrec = create_nontable_vcp_value(
                           feature_code,
                           parsed_nontable_response->mh,
                           parsed_nontable_response->ml,
                           parsed_nontable_response->sh,
                           parsed_nontable_response->sl);
               free(parsed_nontable_response);
            }
            break;

      case (DDCA_TABLE_VCP_VALUE):
            psc = get_table_vcp_value(
                    dh,
                    feature_code,
                    &buffer);
            if (psc == 0) {
               valrec = create_table_vcp_value_by_buffer(feature_code, buffer);
               buffer_free(buffer, __func__);
            }
            break;
      }

   } // non USB

   *pvalrec = valrec;

   // TRCMSGTG(tg, "Done.  Returning: %s", gsc_desc(gsc) );
   DBGTRC(debug, TRACE_GROUP, "Done.  Returning: %s", psc_desc(psc) );
   if (psc == 0 && debug)
      report_single_vcp_value(valrec,1);
   assert( (psc == 0 && *pvalrec) || (psc != 0 && !*pvalrec) );
   return psc;
}

