/** api_capabilities.c
 *
 *  Capabilities related functions of the API
 */

// Copyright (C) 2015-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"

#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/displays.h"
#include "base/feature_metadata.h"
#include "base/rtti.h"
#include "base/vcp_version.h"

#include "vcp/ddc_command_codes.h"
#include "vcp/parse_capabilities.h"
#include "vcp/parsed_capabilities_feature.h"
#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_codes.h"
#include "dynvcp/dyn_parsed_capabilities.h"

#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_vcp_version.h"

#include "libmain/api_base_internal.h"
#include "libmain/api_displays_internal.h"
#include "libmain/api_metadata_internal.h"

#include "libmain/api_error_info_internal.h"
#include "libmain/api_capabilities_internal.h"
 

//
// Monitor Capabilities
//

DDCA_Status
ddca_get_capabilities_string(
      DDCA_Display_Handle  ddca_dh,
      char**               pcaps_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "ddca_dh=%s", dh_repr((Display_Handle *) ddca_dh ) );
   free_thread_error_detail();
   // assert(pcaps_loc);
   API_PRECOND(pcaps_loc);
   *pcaps_loc = NULL;
   Error_Info * ddc_excp = NULL;
   WITH_VALIDATED_DH2(ddca_dh,
      {
         char * p_cap_string = NULL;
         ddc_excp = ddc_get_capabilities_string(dh, &p_cap_string);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         save_thread_error_detail(error_info_to_ddca_detail(ddc_excp));
         errinfo_free(ddc_excp);
         if (psc == 0) {
            // make copy to prevent caller from mucking around in ddcutil's
            // internal data structures
            *pcaps_loc = strdup(p_cap_string);
            // DBGMSF(debug, "*pcaps_loc=%p", *pcaps_loc);
         }
        //  DBGMSF(debug, "psc=%s", ddca_rc_desc(psc));
         assert( (psc==0 && *pcaps_loc) || (psc!=0 && !*pcaps_loc));
#ifdef OLD
         if (debug)
            printf("(%s)1 psc=%d, *pcaps_loc=%p -> |%s|\n", __func__, psc, *pcaps_loc, *pcaps_loc);
         DBGTRC(debug, DDCA_TRC_API,
               "Done. Returrning %s(%d), ddca_dh=%s, *pcaps_loc=%p -> |%s|",
               psc_desc(psc), psc,
               dh_repr_t((Display_Handle *) ddca_dh),
               *pcaps_loc, *pcaps_loc);
         if (debug)
            printf("\n(%s) Using DBGTRC_RETURNING...\n", __func__);
#endif
         DBGTRC_DONE(debug, DDCA_TRC_API,
               "Returning %s. ddca_dh=%s, *pcaps_loc=%p -> |%s|",
               psc_name_code(psc),
               dh_repr_t((Display_Handle *) ddca_dh),
               *pcaps_loc, *pcaps_loc);
      }
   );
}


void
dbgrpt_ddca_cap_vcp(DDCA_Cap_Vcp * cap, int depth) {
   rpt_structure_loc("DDCA_Cap_Vcp", cap, depth);
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(d1, "feature code:    0x%02x", cap->feature_code);
   rpt_vstring(d1, "value_ct:        %d", cap->value_ct);
   if (cap->value_ct > 0) {
      rpt_label(d1, "Values: ");
      for (int ndx = 0; ndx < cap->value_ct; ndx++) {
         rpt_vstring(d2, "Value:   0x%02x", cap->values[ndx]);
      }
   }
}


void
dbgrpt_ddca_capabilities(DDCA_Capabilities * p_caps, int depth) {
   rpt_structure_loc("DDCA_Capabilities", p_caps, depth);
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(d1, "Unparsed string: %s", p_caps->unparsed_string);
   rpt_vstring(d1, "Version spec:    %d.%d", p_caps->version_spec.major, p_caps->version_spec.minor);
   rpt_label(d1, "Command codes:");
   for (int ndx = 0; ndx < p_caps->cmd_ct; ndx++) {
      rpt_vstring(d2, "0x%02x", p_caps->cmd_codes[ndx]);
   }
   rpt_vstring(d1, "Feature code count: %d", p_caps->vcp_code_ct);
   for (int ndx = 0; ndx < p_caps->vcp_code_ct; ndx++) {
      DDCA_Cap_Vcp * cur = &p_caps->vcp_codes[ndx];
      dbgrpt_ddca_cap_vcp(cur, d2);
   }
   rpt_vstring(d1, "msg_ct:       %d", p_caps->msg_ct);
   if (p_caps->msg_ct > 0) {
      rpt_label(d1, "messages: ");
      for (int ndx = 0; ndx < p_caps->msg_ct; ndx++) {
         rpt_vstring(d2, "Message:   %s", p_caps->messages[ndx]);
      }
   }
}


DDCA_Status
ddca_parse_capabilities_string(
      char *                   capabilities_string,
      DDCA_Capabilities **     parsed_capabilities_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "parsed_capabilities_loc=%p, capabilities_string: |%s|",
                                       parsed_capabilities_loc, capabilities_string);
   // assert(parsed_capabilities_loc);
   free_thread_error_detail();
   API_PRECOND(parsed_capabilities_loc);
   DDCA_Status ddcrc = DDCRC_BAD_DATA;
   DBGMSF(debug, "ddcrc initialized to %s", psc_desc(ddcrc));
   DDCA_Capabilities * result = NULL;

   // need to control messages?
   Parsed_Capabilities * pcaps = parse_capabilities_string(capabilities_string);
   if (pcaps) {
      if (debug) {
         DBGMSG("Parsing succeeded: ");
         dyn_report_parsed_capabilities(pcaps, NULL, NULL, 2);
         DBGMSG("Convert to DDCA_Capabilities...");
      }
      result = calloc(1, sizeof(DDCA_Capabilities));
      memcpy(result->marker, DDCA_CAPABILITIES_MARKER, 4);
      result->unparsed_string = strdup(capabilities_string);     // needed?
      result->version_spec = pcaps->parsed_mccs_version;
      DBGMSF(debug, "version: %d.%d", result->version_spec.major,  result->version_spec.minor);
      Byte_Value_Array bva = pcaps->commands;
      if (bva) {
         result->cmd_ct = bva_length(bva);
         result->cmd_codes = malloc(result->cmd_ct);
         memcpy(result->cmd_codes, bva_bytes(bva), result->cmd_ct);
      }
      // n. needen't set vcp_code_ct if !pcaps, calloc() has done it
      if (pcaps->vcp_features) {
         result->vcp_code_ct = pcaps->vcp_features->len;
         result->vcp_codes = calloc(result->vcp_code_ct, sizeof(DDCA_Cap_Vcp));
         DBGMSF(debug, "allocate %d bytes at %p", result->vcp_code_ct * sizeof(DDCA_Cap_Vcp), result->vcp_codes);
         for (int ndx = 0; ndx < result->vcp_code_ct; ndx++) {
            DDCA_Cap_Vcp * cur_cap_vcp = &result->vcp_codes[ndx];
            DBGMSF(debug, "cur_cap_vcp = %p", &result->vcp_codes[ndx]);
            memcpy(cur_cap_vcp->marker, DDCA_CAP_VCP_MARKER, 4);
            Capabilities_Feature_Record * cur_cfr = g_ptr_array_index(pcaps->vcp_features, ndx);
            DBGMSF(debug, "Capabilities_Feature_Record * cur_cfr = %p", cur_cfr);
            assert(memcmp(cur_cfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);
            if (debug)
               dbgrpt_capabilities_feature_record(cur_cfr, 2);
            //    show_capabilities_feature(cur_cfr, result->version_spec);
            cur_cap_vcp->feature_code = cur_cfr->feature_id;
            DBGMSF(debug, "cur_cfr = %p, feature_code - 0x%02x", cur_cfr, cur_cfr->feature_id);

            // cur_cap_vcp->raw_values = strdup(cur_cfr->value_string);
            // TODO: get values from Byte_Bit_Flags cur_cfr->bbflags
#ifdef CFR_BVA
            Byte_Value_Array bva = cur_cfr->values;
            if (bva) {
               cur_cap_vcp->value_ct = bva_length(bva);
               cur_cap_vcp->values = calloc( cur_cap_vcp->value_ct, sizeof(Byte));
               memcpy(cur_cap_vcp->values, bva_bytes(bva), cur_cap_vcp->value_ct);
            }
#endif
#ifdef CFR_BBF
            if (cur_cfr->bbflags) {
               cur_cap_vcp->value_ct = bbf_count_set(cur_cfr->bbflags);
               cur_cap_vcp->values   = calloc(1, cur_cap_vcp->value_ct);
               bbf_to_bytes(cur_cfr->bbflags, cur_cap_vcp->values, cur_cap_vcp->value_ct);
            }
#endif
         }
      }

      // DBGMSG("pcaps->messages = %p", pcaps->messages);
      // if (pcaps->messages) {
      //    DBGMSG("pcaps->messages->len = %d", pcaps->messages->len);
      // }

      if (pcaps->messages && pcaps->messages->len > 0) {
         result->msg_ct = pcaps->messages->len;
         result->messages = g_ptr_array_to_ntsa(pcaps->messages, /*duplicate=*/ true);
      }

      ddcrc = 0;
      free_parsed_capabilities(pcaps);
   }

   *parsed_capabilities_loc = result;

   DBGTRC_RETURNING(debug, DDCA_TRC_API, ddcrc, "*parsed_capabilities_loc=%p", *parsed_capabilities_loc);
   if ( (debug || IS_TRACING_GROUP(DDCA_TRC_API) ) && *parsed_capabilities_loc)
      dbgrpt_ddca_capabilities(*parsed_capabilities_loc, 2);

   ASSERT_IFF(ddcrc==0, *parsed_capabilities_loc);
   return ddcrc;
}


void
ddca_free_parsed_capabilities(
      DDCA_Capabilities * pcaps)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "pcaps=%p", pcaps);
   if (pcaps) {
      assert(memcmp(pcaps->marker, DDCA_CAPABILITIES_MARKER, 4) == 0);
      free(pcaps->unparsed_string);

      DBGMSF(debug, "vcp_code_ct = %d", pcaps->vcp_code_ct);
      for (int ndx = 0; ndx < pcaps->vcp_code_ct; ndx++) {
         DDCA_Cap_Vcp * cur_vcp = &pcaps->vcp_codes[ndx];
         assert(memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);
         cur_vcp->marker[3] = 'x';
         free(cur_vcp->values);
      }

      ntsa_free(pcaps->messages, true);
      pcaps->marker[3] = 'x';
      free(pcaps);
   }
   DBGTRC_DONE(debug, DDCA_TRC_API, "");
}


DDCA_Status
ddca_report_parsed_capabilities_by_dref(
      DDCA_Capabilities *      p_caps,
      DDCA_Display_Ref         ddca_dref,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. p_caps=%p, ddca_dref=%s", p_caps, dref_repr_t((Display_Ref*) ddca_dref));

   free_thread_error_detail();
   DDCA_Status ddcrc = 0;

   API_PRECOND(p_caps);   // no need to check marker, DDCA_CAPABILITIES not opaque

   Display_Ref * dref = NULL;
   // dref may be NULL, but if not it must be valid
   if (ddca_dref) {
      dref = validated_ddca_display_ref(ddca_dref);
      if (!dref) {
         ddcrc = DDCRC_ARG;
         goto bye;
      }
   }

   int d0 = depth;
   int d1 = depth+1;
   int d2 = depth+2;
   int d3 = depth+3;

   DDCA_Output_Level ol = get_output_level();

   if (ol >= DDCA_OL_VERBOSE)
      rpt_vstring(d0, "Unparsed string: %s", p_caps->unparsed_string);

   char * s = NULL;
   if (vcp_version_eq(p_caps->version_spec, DDCA_VSPEC_UNQUERIED))
      s = "Not present";
   else if (vcp_version_eq(p_caps->version_spec, DDCA_VSPEC_UNKNOWN))
      s = "Invalid value";
   else
      s = format_vspec(p_caps->version_spec);
   rpt_vstring(d0, "VCP version: %s", s);
   if (ol >= DDCA_OL_VERBOSE) {
      rpt_label  (d0, "Command codes: ");
      for (int cmd_ndx = 0; cmd_ndx < p_caps->cmd_ct; cmd_ndx++) {
         uint8_t cur_code = p_caps->cmd_codes[cmd_ndx];
         char * cmd_name = ddc_cmd_code_name(cur_code);
         rpt_vstring(d1, "0x%02x (%s)", cur_code, cmd_name);
      }
   }

   rpt_vstring(d0, "VCP Feature codes:");
   for (int code_ndx = 0; code_ndx < p_caps->vcp_code_ct; code_ndx++) {
      DDCA_Cap_Vcp * cur_vcp = &p_caps->vcp_codes[code_ndx];
      assert( memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);

      Display_Feature_Metadata * dfm =
         dyn_get_feature_metadata_by_dref(
               cur_vcp->feature_code,
               ddca_dref,
               true);    // create_default_if_not_found);
      assert(dfm);
      // dbgrpt_display_feature_metadata(dfm, 3);

      rpt_vstring(d1, "Feature:  0x%02x (%s)", cur_vcp->feature_code, dfm->feature_name);

      if (cur_vcp->value_ct > 0) {
         if (ol > DDCA_OL_VERBOSE)
            rpt_vstring(d2, "Unparsed values:     %s", hexstring_t(cur_vcp->values, cur_vcp->value_ct) );

         DDCA_Feature_Value_Entry * feature_value_table = dfm->sl_values;
         rpt_label(d2, "Values:");
         for (int ndx = 0; ndx < cur_vcp->value_ct; ndx++) {
            char * value_desc = "No lookup table";
            if (feature_value_table) {
               value_desc =
                   sl_value_table_lookup(feature_value_table, cur_vcp->values[ndx]);
               if (!value_desc)
                  value_desc = "Unrecognized feature value";
            }
            rpt_vstring(d3, "0x%02x: %s", cur_vcp->values[ndx], value_desc);
         }
      }
      dfm_free(dfm);
   } // one feature code

   if (p_caps->messages && *p_caps->messages) {
      rpt_nl();
      rpt_label(d0, "Parsing errors:");
      char ** m = p_caps->messages;
      while (*m) {
         rpt_label(d1, *m);
         m++;
      }
   }
   else {
      DBGMSF(debug, "No error messages");
   }

bye:
   return ddcrc;
}


void
ddca_report_parsed_capabilities(
      DDCA_Capabilities *      p_caps,
      int                      depth)
{
   ddca_report_parsed_capabilities_by_dref(p_caps, NULL, depth);
}


DDCA_Status
ddca_report_parsed_capabilities_by_dh(
      DDCA_Capabilities *      p_caps,
      DDCA_Display_Handle      ddca_dh,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. p_caps=%p, ddca_dh=%s, depth=%d", p_caps, ddca_dh_repr(ddca_dh), depth);
   DDCA_Status ddcrc = 0;
   free_thread_error_detail();
   assert(library_initialized);

   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 ) {
      ddcrc = DDCRC_ARG;
      goto bye;
   }

   // Ensure dh->dref->vcp_version is not unqueried,
   // ddca_report_parsed_capabilities_by_dref() will fail trying to lock the already open device
   get_vcp_version_by_dh(dh);
   DBGMSF(debug, "After get_vcp_version_by_dh(), dh->dref->vcp_version_df=%s",
                 format_vspec_verbose(dh->dref->vcp_version_xdf));

   ddca_report_parsed_capabilities_by_dref(p_caps, dh->dref, depth);

bye:
   DBGMSF(debug, "Done.     Returning %s", ddca_rc_desc(ddcrc));
   return ddcrc;
}


// UNPUBLISHED
void
ddca_parse_and_report_capabilities(
      char *                    capabilities_string,
      DDCA_Display_Ref          dref,
      int                       depth)
{
      Parsed_Capabilities* pcaps = parse_capabilities_string(capabilities_string);
      dyn_report_parsed_capabilities(pcaps, NULL, dref, 0);
      free_parsed_capabilities(pcaps);
}


DDCA_Feature_List
ddca_feature_list_from_capabilities(
      DDCA_Capabilities * parsed_caps)
{
   DDCA_Feature_List result = {{0}};
   for (int ndx = 0; ndx < parsed_caps->vcp_code_ct; ndx++) {
      DDCA_Cap_Vcp curVcp = parsed_caps->vcp_codes[ndx];
      ddca_feature_list_add(&result, curVcp.feature_code);
   }
   return result;
}

#ifdef NOT_NEEDED
void init_api_capabilities() {
   printf("(%s) Executing\n", __func__);
   RTTI_ADD_FUNC(ddca_get_capabilities_string);
}
#endif
