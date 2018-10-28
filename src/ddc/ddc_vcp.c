/** \file ddc_vcp.c
 *  Virtual Control Panel access
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/error_info.h"
#include "util/report_util.h"
#include "util/utilrpt.h"
/** \endcond */

#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_bus_core.h"

#include "adl/adl_shim.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#include "usb/usb_vcp.h"
#endif

#include "vcp/vcp_feature_codes.h"

#include <dynvcp/dyn_feature_codes.h>

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"

#include "ddc/ddc_vcp.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;



typedef struct {
   bool   verify_setvcp;
} Thread_Vcp_Settings;

static Thread_Vcp_Settings *  get_thread_vcp_settings() {
   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);

   Thread_Vcp_Settings *settings = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, settings=%p\n", __func__, this_thread, settings);

   if (!settings) {
      settings = g_new0(Thread_Vcp_Settings, 1);
      settings->verify_setvcp = false;     // set by g_new0(), but be explicit

      g_private_set(&per_thread_key, settings);
   }

   // printf("(%s) Returning: %p\n", __func__, settings);
   return settings;
}



//
//  Save Control Settings
//

/** Executes the DDC Save Control Settings command.
 *
 * \param  dh handle of open display device
 * \return NULL if success, pointer to #Ddc_Error if failure
 */
Error_Info *
ddc_save_current_settings(
      Display_Handle * dh)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Invoking DDC Save Current Settings command. dh=%s", dh_repr_t(dh));
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
      // command line parser should block this case
      PROGRAM_LOGIC_ERROR("MCCS over USB does not have Save Current Settings command", NULL);
      ddc_excp = errinfo_new(DDCRC_UNIMPLEMENTED, __func__);
   }
   else {
      DDC_Packet * request_packet_ptr =
         create_ddc_save_settings_request_packet("save_current_settings:request packet");
      // DBGMSG("create_ddc_save_settings_request_packet returned packet_ptr=%p", request_packet_ptr);
      // dump_packet(request_packet_ptr);

      ddc_excp = ddc_write_only_with_retry(dh, request_packet_ptr);
      psc = (ddc_excp) ? ddc_excp->status_code : 0;

      if (request_packet_ptr)
         free_ddc_packet(request_packet_ptr);
   }

   DBGTRC(debug, TRACE_GROUP, "Returning %s", psc_desc(psc));
   if ( (debug||IS_TRACING()) && ddc_excp)
      errinfo_report(ddc_excp, 0);
   return ddc_excp;
}


//
// Set VCP feature value
//

/** Sets a non-table VCP feature value.
 *
 *  \param  dh            display handle for open display
 *  \param  feature_code  VCP feature code
 *  \param  new_value     new value
 *  \return NULL if success,
 *          pointer to #Ddc_Error from #ddc_write_only_with_retry() if failure
 */
Error_Info *
ddc_set_nontable_vcp_value(
      Display_Handle * dh,
      Byte             feature_code,
      int              new_value)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Writing feature 0x%02x , new value = %d, dh=%s",
          feature_code, new_value, dh_repr_t(dh) );
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
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

      ddc_excp = ddc_write_only_with_retry(dh, request_packet_ptr);
      psc = (ddc_excp) ? ddc_excp->status_code : 0;

      if (request_packet_ptr)
         free_ddc_packet(request_packet_ptr);
   }

   DBGTRC(debug, TRACE_GROUP, "Returning %s", psc_desc(psc));
   if ( psc==DDCRC_RETRIES && (debug || IS_TRACING()) )
      DBGMSG("          Try errors: %s", errinfo_causes_string(ddc_excp));
   return ddc_excp;
}


/** Sets a table VCP feature value.
 *
 *  \param  dh            display handle for open display
 *  \param  feature_code  VCP feature code
 *  \param  bytes         pointer to table bytes
 *  \param  bytect        number of bytes
 *  \return NULL  if success
 *          DDCL_UNIMPLEMENTED if io mode is USB
 *          #Ddc_Error from #multi_part_write_with_retry() otherwise
 */
Error_Info *
set_table_vcp_value(
      Display_Handle *  dh,
      Byte              feature_code,
      Byte *            bytes,
      int               bytect)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Writing feature 0x%02x , bytect = %d",
                              feature_code, bytect);
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
      psc = DDCRC_UNIMPLEMENTED;
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      // TODO: clean up function signatures
      // pointless wrapping in a Buffer just to unwrap
      Buffer * new_value = buffer_new_with_value(bytes, bytect, __func__);

      ddc_excp = multi_part_write_with_retry(dh, feature_code, new_value);
      psc = (ddc_excp) ? ddc_excp->status_code : 0;

      buffer_free(new_value, __func__);
   }

   DBGTRC(debug, TRACE_GROUP, "Returning: %s", psc_desc(psc));
   if ( (debug || IS_TRACING()) && psc == DDCRC_RETRIES )
      DBGMSG("      Try errors: %s", errinfo_causes_string(ddc_excp));
   return ddc_excp;
}


/** Sets the setvcp verification setting for the current thread.
 *
 *  If enabled, setvcp will read the feature value from the monitor after
 *  writing it, to ensure the monitor has actually changed the feature value.
 *
 *  \param onoff  **true** for enabled, **false** for disabled.
 *  \return prior setting
 */
bool ddc_set_verify_setvcp(bool onoff) {
   bool debug = false;
   DBGMSF(debug, "Setting verify_setvcp = %s", bool_repr(onoff));
   Thread_Vcp_Settings * settings = get_thread_vcp_settings();
   bool old_value = settings->verify_setvcp;
   settings->verify_setvcp = onoff;
   return old_value;
}


/** Gets the current setvcp verification setting for the current thread.
 *
 *  \return **true** if setvcp verification enabled\n
 *          **false** if not
 */
bool ddc_get_verify_setvcp() {
   Thread_Vcp_Settings * settings = get_thread_vcp_settings();
   return settings->verify_setvcp;
}


static bool
is_rereadable_feature(
      Display_Handle * dh,
      DDCA_Vcp_Feature_Code opcode)
{
   bool debug = false;
   DBGMSF(debug, "Starting opcode = 0x%02x", opcode);

   // readable features that should not be read after write
   DDCA_Vcp_Feature_Code unrereadable_features[] = {
         0x02,        // new control value
         0x03,        // soft controls
         0x60,        // input source ???
   };

   bool result = true;
   for (int ndx = 0; ndx < ARRAY_SIZE(unrereadable_features); ndx++) {
      if ( unrereadable_features[ndx] == opcode ) {
          result = false;
          DBGMSF(debug, "Unreadable opcode 0x%02x", opcode);
          break;
      }
   }
   if (result) {
      Internal_Feature_Metadata * intmeta = dyn_get_feature_metadata_by_dh(
            opcode,
            dh,
            false    //                  with_default
            );
      // if not found, assume readable  ??
      if (intmeta) {
         result = intmeta->external_metadata->feature_flags & DDCA_READABLE;
      }
#ifdef OLD
      VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid(opcode);
      DBGMSF(debug, "vfte=%p", vfte);
      if (vfte) {
         assert(opcode < 0xe0);
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);  // ensure dh->vcp_version set
         DBGMSF(debug, "vspec = %d.%d", vspec.major, vspec.minor);
         // hack, make a guess
         if ( vcp_version_eq(vspec, DDCA_VSPEC_UNKNOWN)   ||
              vcp_version_eq(vspec, DDCA_VSPEC_UNQUERIED ))
            vspec = DDCA_VSPEC_V22;

         // if ( !vcp_version_eq(vspec, VCP_SPEC_UNKNOWN) &&
         //      !vcp_version_eq(vspec, VCP_SPEC_UNQUERIED ))
         // {
            result = is_feature_readable_by_vcp_version(vfte, vspec);
            DBGMSF(debug, "vspec=%d.%d, readable feature = %s", vspec.major, vspec.minor, bool_repr(result));
         // }
      }
#endif

   }

   DBGMSF(debug, "Returning: %s", bool_repr(result));
   return result;
}


static bool
single_vcp_value_equal(
      DDCA_Any_Vcp_Value * vrec1,
      DDCA_Any_Vcp_Value * vrec2)
{
   assert(vrec1 && vrec2);  // no implementation for degenerate cases
   bool debug = false;

   bool result = false;
   if (vrec1->opcode     == vrec2->opcode &&
       vrec1->value_type == vrec2->value_type)
   {
      switch(vrec1->value_type) {
      case(DDCA_NON_TABLE_VCP_VALUE):
            // only check SL byte which would be set for any VCP, monitor
      result = (vrec1->val.c_nc.sl == vrec2->val.c_nc.sl);
            break;
      case(DDCA_TABLE_VCP_VALUE):
            result = (vrec1->val.t.bytect == vrec2->val.t.bytect) &&
                     (memcmp(vrec1->val.t.bytes, vrec2->val.t.bytes, vrec1->val.t.bytect) == 0 );
      }
   }
   DBGMSF(debug, "Returning: %s", bool_repr(result));
   return result;
}



// TODO: Consider wrapping set_vcp_value() in set_vcp_value_with_retry(), which would
// retry in case verification fails

/** Sets a VCP feature value.
 *
 *  \param  dh            display handle for open display
 *  \param  vrec          pointer to value record
 *  \param  newval_loc    if non-null, address at which to return verified value
 *  \return NULL if success, pointer to #Ddc_Error if failure
 *
 *  If write verification is turned on, reads the feature value after writing it
 *  to ensure the display has actually changed the value.
 *
 * The caller is responsible for freeing the value returned at **newval_loc**.
 *  \remark
 *  If verbose messages are in effect, writes detailed messages to the current
 *  stdout device.
 */
Error_Info *
ddc_set_vcp_value(
      Display_Handle *    dh,
      DDCA_Any_Vcp_Value *  vrec,
      DDCA_Any_Vcp_Value ** newval_loc)
{
   bool debug = false;
   DBGMSF0(debug, "Starting. ");
   FILE * verbose_msg_dest = fout();
   if ( get_output_level() < DDCA_OL_VERBOSE && !debug )
      verbose_msg_dest = NULL;

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   if (newval_loc)
      *newval_loc = NULL;
   if (vrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
      ddc_excp = ddc_set_nontable_vcp_value(dh, vrec->opcode, VALREC_CUR_VAL(vrec));
      psc = (ddc_excp) ? ddc_excp->status_code : 0;
   }
   else {
      assert(vrec->value_type == DDCA_TABLE_VCP_VALUE);
      ddc_excp = set_table_vcp_value(dh, vrec->opcode, vrec->val.t.bytes, vrec->val.t.bytect);
      psc = (ddc_excp) ? ddc_excp->status_code : 0;
   }

   if (!ddc_excp && ddc_get_verify_setvcp()) {
      if (is_rereadable_feature(dh, vrec->opcode) ) {
         f0printf(verbose_msg_dest, "Verifying that value of feature 0x%02x successfully set...\n", vrec->opcode);
         DDCA_Any_Vcp_Value * newval = NULL;
         ddc_excp = ddc_get_vcp_value(
             dh,
             vrec->opcode,
             vrec->value_type,
             &newval);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         if (ddc_excp) {
            f0printf(verbose_msg_dest, "(%s) Read after write failed. get_vcp_value() returned: %s\n",
                           __func__, psc_desc(psc));
            if (psc == DDCRC_RETRIES)
               f0printf(verbose_msg_dest, "(%s)    Try errors: %s\n", __func__, errinfo_causes_string(ddc_excp));
            // psc = DDCRC_VERIFY;
         }
         else {
            assert(vrec && newval);    // silence clang complaint
            // dbgrpt_ddca_single_vcp_value(vrec, 2);
            // dbgrpt_ddca_single_vcp_value(newval, 3);

            if (! single_vcp_value_equal(vrec,newval)) {
               psc = DDCRC_VERIFY;
               ddc_excp = errinfo_new(DDCRC_VERIFY, __func__);
               f0printf(verbose_msg_dest, "Current value does not match value set.\n");
            }
            else {
               f0printf(verbose_msg_dest, "Verification succeeded\n");
            }
            if (newval_loc)
               *newval_loc = newval;
            else
               free_single_vcp_value(newval);
         }
      }
      else {
         f0printf(verbose_msg_dest, "Feature 0x%02x does not support verification\n", vrec->opcode);
      }
   }

   DBGMSF(debug, "Returning: %s", psc_desc(psc));
   return ddc_excp;
}


//
// Get VCP values
//

/** Gets the value for a non-table feature.
 *
 *  \param  dh                 handle for open display
 *  \param  feature_code       VCP feature code
 *  \param  ppInterpretedCode  where to return parsed respons
 *  \return NULL if success, pointer to #Ddc_Error if failure
 *
 * It is the responsibility of the caller to free the parsed response.
 *
 * The value pointed to by ppInterpretedCode is non-null iff the returned status code is 0.
 */
Error_Info *
ddc_get_nontable_vcp_value(
       Display_Handle *               dh,
       DDCA_Vcp_Feature_Code          feature_code,
       Parsed_Nontable_Vcp_Response** ppInterpretedCode)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Reading feature 0x%02x", feature_code);

   Public_Status_Code psc = 0;
   Error_Info * excp = NULL;
   Parsed_Nontable_Vcp_Response * parsed_response = NULL;

   DDC_Packet * request_packet_ptr  = NULL;
   DDC_Packet * response_packet_ptr = NULL;
   request_packet_ptr = create_ddc_getvcp_request_packet(
                           feature_code, "get_vcp_by_DisplayRef:request packet");
   // dump_packet(request_packet_ptr);

   Byte expected_response_type = DDC_PACKET_TYPE_QUERY_VCP_RESPONSE;
   Byte expected_subtype = feature_code;
   int max_read_bytes  = 20;    // actually 3 + 8 + 1, or is it 2 + 8 + 1?

   // retry:
   // psc = ddc_write_read_with_retry(
   excp = ddc_write_read_with_retry(
           dh,
           request_packet_ptr,
           max_read_bytes,
           expected_response_type,
           expected_subtype,
           false,                       // all_zero_response_ok
           &response_packet_ptr
        );
   assert( (!excp && response_packet_ptr) || (excp && !response_packet_ptr));
   if (debug || IS_TRACING() ) {
      if (psc != 0)
         DBGMSG("perform_ddc_write_read_with_retry() returned %s, reponse_packet_ptr=%p",
                psc_desc(psc), response_packet_ptr);
   }
   // psc = (excp) ? excp->psc : 0;

   if (!excp) {
      assert(response_packet_ptr);
      // dump_packet(response_packet_ptr);
      psc = get_interpreted_vcp_code(response_packet_ptr, true /* make_copy */, &parsed_response);   // ???
      if (psc == 0) {
#ifdef NO_LONGER_NEEDED
         if (parsed_response->vcp_code != feature_code) {
            DBGMSG("!!! WTF! requested feature_code = 0x%02x, but code in response is 0x%02x",
                   feature_code, parsed_response->vcp_code);
            call_tuned_sleep_i2c(SE_POST_READ);
            goto retry;
         }
#endif

         if (!parsed_response->valid_response)  {
            psc = DDCRC_DDC_DATA;             // was DDCRC_INVALID_DATA
            excp = errinfo_new(psc, __func__);
         }
         else if (!parsed_response->supported_opcode) {
            psc = DDCRC_REPORTED_UNSUPPORTED;
            excp = errinfo_new(psc, __func__);
         }

         if (psc != 0) {
            free(parsed_response);
            parsed_response = NULL;
         }
      }
      else {
         excp = errinfo_new(psc, __func__);
      }
   }

   if (request_packet_ptr)
      free_ddc_packet(request_packet_ptr);
   if (response_packet_ptr)
      free_ddc_packet(response_packet_ptr);

   assert( (!excp && parsed_response) || (excp && !parsed_response)); // needed to avoid clang warning
   if (debug || IS_TRACING() ) {
      if (excp) {
         DBGMSG("Error reading feature x%02x.  Returning exception: ", feature_code);
         errinfo_report(excp, 1);
         DBGMSG("Done");
      }
      else {
         DBGMSG("Success reading feature x%02x. *ppinterpreted_code=%p", feature_code, parsed_response);
         DBGMSG("  mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x, max value=%d, cur value=%d",
                parsed_response->mh, parsed_response->ml,
                parsed_response->sh, parsed_response->sl,
                (parsed_response->mh<<8) | parsed_response->ml,
                (parsed_response->sh<<8) | parsed_response->sl);
      }
   }
   *ppInterpretedCode = parsed_response;

   return excp;
}


/** Gets the value of a table feature in a newly allocated Buffer struct.
 *  It is the responsibility of the caller to free the Buffer.
 *
 *  \param  dh              display handle
 *  \param  feature_code    VCP feature code
 *  \param  pp_table_bytes  location at which to save address of newly allocated Buffer
 *  \return NULL if success, pointer to #Ddc_Error if failure
 */
Error_Info * ddc_get_table_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       Buffer**               pp_table_bytes)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x", feature_code);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   DDCA_Output_Level output_level = get_output_level();
   Buffer * paccumulator =  NULL;

   ddc_excp = multi_part_read_with_retry(
            dh,
            DDC_PACKET_TYPE_TABLE_READ_REQUEST,
            feature_code,
            true,                      // all_zero_response_ok
            &paccumulator);
   psc = (ddc_excp) ? ddc_excp->status_code : 0;
   if (debug || psc != 0) {
      DBGTRC(debug, TRACE_GROUP,
             "perform_ddc_write_read_with_retry() returned %s", psc_desc(psc));
   }

   if (psc == 0) {
      *pp_table_bytes = paccumulator;
      if (output_level >= DDCA_OL_VERBOSE) {
         DBGMSG0("Bytes returned on table read:");
         dbgrpt_buffer(paccumulator, 1);
      }
   }

   DBGTRC(debug, TRACE_GROUP,
          "Done. rc=%s, *pp_table_bytes=%p", psc_desc(psc), *pp_table_bytes);
   DBGTRC(debug, TRACE_GROUP, "Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}


/** Gets the value of a VCP feature.
 *
 * \param  dh              handle for open display
 * \param  feature_code    feature code id
 * \param  call_type       indicates whether table or non-table
 * \param  pvalrec         location where to return newly allocated #Single_Vcp_Value
 * \return NULL if success, pointer to #Ddc_Error if failure
 *
 * The value pointed to by pvalrec is non-null iff the returned status code is 0.
 *
 * The caller is responsible for freeing the value returned at **valrec_loc**.
 */


Error_Info *
ddc_get_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       DDCA_Vcp_Value_Type    call_type,
       DDCA_Any_Vcp_Value **  valrec_loc)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. Reading feature 0x%02x, dh=%s, dh->fh=%d",
            feature_code, dh_repr_t(dh), dh->fh);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   Buffer * buffer = NULL;
   Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;  // vs interpreted ..
   DDCA_Any_Vcp_Value * valrec = NULL;

   // why are we coming here for USB?
   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
      DBGMSF0(debug, "USB case");

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
                psc = DDCRC_UNIMPLEMENTED;
                ddc_excp = errinfo_new(DDCRC_UNIMPLEMENTED, __func__);
                break;
          }
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
   }
   else {
      switch (call_type) {

      case (DDCA_NON_TABLE_VCP_VALUE):
            ddc_excp = ddc_get_nontable_vcp_value(
                          dh,
                          feature_code,
                          &parsed_nontable_response);
            psc = (ddc_excp) ? ddc_excp->status_code : 0;
            if (!ddc_excp) {
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
            ddc_excp = ddc_get_table_vcp_value(
                    dh,
                    feature_code,
                    &buffer);
            psc = ERRINFO_STATUS(ddc_excp);
            if (!ddc_excp) {
               valrec = create_table_vcp_value_by_buffer(feature_code, buffer);
               buffer_free(buffer, __func__);
            }
            break;
      }

   } // non USB

   *valrec_loc = valrec;

   DBGTRC(debug, TRACE_GROUP, "Done. psc=%s", psc_desc(psc) );
   if (psc == 0 && debug)
      report_any_vcp_value(valrec,1);
   assert( (psc == 0 && *valrec_loc) || (psc != 0 && !*valrec_loc) );
   DBGTRC(debug, TRACE_GROUP, "Done. Returning: %s", errinfo_summary(ddc_excp));
   return ddc_excp;
}
