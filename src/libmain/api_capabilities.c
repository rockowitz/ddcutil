/** api_capabilities.c
 *
 *  Capabilities related functions of the API
 */

// Copyright (C) 2015-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <libmain/api_base_internal.h>
#include <libmain/api_capabilities_internal.h>
#include <libmain/api_displays_internal.h>
#include <libmain/api_metadata_internal.h>
#include <stdbool.h>
#include <stddef.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_c_api.h"

#include "util/error_info.h"

#include "base/displays.h"
#include "base/vcp_version.h"

#include "vcp/parsed_capabilities_feature.h"
#include "vcp/parse_capabilities.h"
#include "vcp/vcp_feature_codes.h"
#include "vcp/ddc_command_codes.h"

#include "dynvcp/dyn_parsed_capabilities.h"

#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_vcp_version.h"
 

//
// Monitor Capabilities
//

DDCA_Status
ddca_get_capabilities_string(
      DDCA_Display_Handle  ddca_dh,
      char**               pcaps)
{
   bool debug = false;
   Error_Info * ddc_excp = NULL;
   WITH_DH(ddca_dh,
      {
         char * p_cap_string = NULL;
         ddc_excp = get_capabilities_string(dh, &p_cap_string);
         psc = (ddc_excp) ? ddc_excp->status_code : 0;
         errinfo_free(ddc_excp);
         if (psc == 0) {
            // make copy to ensure caller does not muck around in ddcutil's
            // internal data structures
            *pcaps = strdup(p_cap_string);
            DBGMSF(debug, "*pcaps=%p", *pcaps);
         }
      }
   );
}


DDCA_Status
ddca_parse_capabilities_string(
      char *                   capabilities_string,
      DDCA_Capabilities **     p_parsed_capabilities)
{
   bool debug = false;
   DBGMSF(debug, "Starting. capabilities_string: |%s|", capabilities_string);
   DDCA_Status psc = DDCRC_OTHER;       // DDCL_BAD_DATA?
   DBGMSF(debug, "psc initialized to %s", psc_desc(psc));
   DDCA_Capabilities * result = NULL;

   // need to control messages?
   Parsed_Capabilities * pcaps = parse_capabilities_string(capabilities_string);
   if (pcaps) {
      if (debug) {
         DBGMSG("Parsing succeeded. ");
         dyn_report_parsed_capabilities(pcaps, NULL, 0);
         DBGMSG("Convert to DDCA_Capabilities...");
      }
      result = calloc(1, sizeof(DDCA_Capabilities));
      memcpy(result->marker, DDCA_CAPABILITIES_MARKER, 4);
      result->unparsed_string= strdup(capabilities_string);     // needed?
      result->version_spec = pcaps->parsed_mccs_version;
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
         // DBGMSF(debug, "allocate %d bytes at %p", result->vcp_code_ct * sizeof(DDCA_Cap_Vcp), result->vcp_codes);
         for (int ndx = 0; ndx < result->vcp_code_ct; ndx++) {
            DDCA_Cap_Vcp * cur_cap_vcp = &result->vcp_codes[ndx];
            // DBGMSF(debug, "cur_cap_vcp = %p", &result->vcp_codes[ndx]);
            memcpy(cur_cap_vcp->marker, DDCA_CAP_VCP_MARKER, 4);
            Capabilities_Feature_Record * cur_cfr = g_ptr_array_index(pcaps->vcp_features, ndx);
            // DBGMSF(debug, "Capabilities_Feature_Record * cur_cfr = %p", cur_cfr);
            assert(memcmp(cur_cfr->marker, CAPABILITIES_FEATURE_MARKER, 4) == 0);
            // if (debug)
            //    show_capabilities_feature(cur_cfr, result->version_spec);
            cur_cap_vcp->feature_code = cur_cfr->feature_id;
            // DBGMSF(debug, "cur_cfr = %p, feature_code - 0x%02x", cur_cfr, cur_cfr->feature_id);

            // cur_cap_vcp->raw_values = strdup(cur_cfr->value_string);
            // TODO: get values from Byte_Bit_Flags cur_cfr->bbflags
#ifdef OLD_BVA
            Byte_Value_Array bva = cur_cfr->values;
            if (bva) {
               cur_cap_vcp->value_ct = bva_length(bva);
               cur_cap_vcp->values = bva_bytes(bva);     // makes copy of bytes
            }
#endif
            if (cur_cfr->bbflags) {
               cur_cap_vcp->value_ct = bbf_count_set(cur_cfr->bbflags);
               cur_cap_vcp->values   = calloc(1, cur_cap_vcp->value_ct);
               bbf_to_bytes(cur_cfr->bbflags, cur_cap_vcp->values, cur_cap_vcp->value_ct);
            }
         }
      }
      psc = 0;
      free_parsed_capabilities(pcaps);
   }

   *p_parsed_capabilities = result;
   DBGMSF(debug, "Done. Returning: %d", psc);
   return psc;
}


void
ddca_free_parsed_capabilities(
      DDCA_Capabilities * pcaps)
{
   bool debug = false;
   if (pcaps) {
      assert(memcmp(pcaps->marker, DDCA_CAPABILITIES_MARKER, 4) == 0);
      free(pcaps->unparsed_string);

      DBGMSF(debug, "vcp_code_ct = %d", pcaps->vcp_code_ct);
      for (int ndx = 0; ndx < pcaps->vcp_code_ct; ndx++) {
         DDCA_Cap_Vcp * cur_vcp = &pcaps->vcp_codes[ndx];
         assert(memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);
         free(cur_vcp->values);
         cur_vcp->marker[3] = 'x';
      }

      pcaps->marker[3] = 'x';
      free(pcaps);
   }
}


DDCA_Status
ddca_report_parsed_capabilities_by_dref(
      DDCA_Capabilities *      p_caps,
      DDCA_Display_Ref         ddca_dref,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(p_caps && memcmp(p_caps->marker, DDCA_CAPABILITIES_MARKER, 4) == 0);
   // int d0 = depth;
   // quick hack since d0 no longer used
   int d1 = depth;
   int d2 = depth+1;
   int d3 = depth+2;
   int d4 = depth+3;

   DDCA_Output_Level ol = get_output_level();

   // rpt_structure_loc("DDCA_Capabilities", pcaps, depth);
   // rpt_label(  d0, "Capabilities:");
   if (ol >= DDCA_OL_VERBOSE)
      rpt_vstring(d1, "Unparsed string: %s", p_caps->unparsed_string);
   rpt_vstring(d1, "VCP version:     %d.%d", p_caps->version_spec.major, p_caps->version_spec.minor);
   if (ol >= DDCA_OL_VERBOSE) {
      rpt_label  (d1, "Command codes: ");
      for (int cmd_ndx = 0; cmd_ndx < p_caps->cmd_ct; cmd_ndx++) {
         uint8_t cur_code = p_caps->cmd_codes[cmd_ndx];
         char * cmd_name = ddc_cmd_code_name(cur_code);
         rpt_vstring(d2, "0x%02x (%s)", cur_code, cmd_name);
      }
   }

   rpt_vstring(d1, "VCP Feature codes:");
   for (int code_ndx = 0; code_ndx < p_caps->vcp_code_ct; code_ndx++) {
      DDCA_Cap_Vcp * cur_vcp = &p_caps->vcp_codes[code_ndx];
      assert( memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);

      DDCA_Feature_Metadata info;
      DDCA_Status ddcrc =
           ddca_get_feature_metadata_by_dref(
            cur_vcp->feature_code,
            ddca_dref,         // ok if NULL
            false,             // create_default_if_not_found,
            &info);
      bool found_metadata = true;
      if (ddcrc != 0) {
         DBGMSF(debug, "ddca_get_feature_metadata_by_dref() returned %s", ddca_rc_desc(ddcrc));
         found_metadata = false;
      }

      char * feature_name = NULL;
      if (found_metadata)
         feature_name = info.feature_name;
      else
         feature_name = get_feature_name_by_id_and_vcp_version(cur_vcp->feature_code, p_caps->version_spec);

      // char * feature_name
      //         = get_feature_name_by_id_and_vcp_version(cur_vcp->feature_code, p_caps->version_spec);
      // FUTURE:
      //        = dyn_get_feature_name(cur_vcp->feature_code, dref));  // n. handles dref == NULL

      rpt_vstring(d2, "Feature:  0x%02x (%s)", cur_vcp->feature_code, feature_name);

      DDCA_Feature_Value_Entry * feature_value_table;



      if (cur_vcp->value_ct > 0) {
         if (found_metadata)
            feature_value_table = info.sl_values;
         else {
            ddcrc = ddca_get_simple_sl_value_table_by_vspec(
                  cur_vcp->feature_code,
                  p_caps->version_spec,
                  NULL,
                  &feature_value_table);
            DBGMSG("ddca_get_simple_sl_value_table_by_vspec() returned %s", ddca_rc_desc(ddcrc));
         }



         if (ol > DDCA_OL_VERBOSE)
            rpt_vstring(d3, "Unparsed values:     %s", hexstring_t(cur_vcp->values, cur_vcp->value_ct) );
         rpt_label(d3, "Values:");
         for (int ndx = 0; ndx < cur_vcp->value_ct; ndx++) {
            char * value_desc = "No lookup table";
            if (feature_value_table) {
               value_desc = "Unrecognized feature value";
               ddca_get_simple_nc_feature_value_name_by_table(
                          feature_value_table,
                          cur_vcp->values[ndx],
                          &value_desc);
            }
            rpt_vstring(d4, "0x%02x: %s", cur_vcp->values[ndx], value_desc);
         }
      }
   }
   return 0;           // *** TEMP ***
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
   if (!library_initialized) {
      ddcrc = DDCRC_UNINITIALIZED;
      goto bye;
   }

   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 ) {
      ddcrc = DDCRC_ARG;
      goto bye;
   }

   // Ensure dh->dref->vcp_version is not unqueried,
   // ddca_report_parsed_capabilities_by_dref() will fail trying to lock the already open device
   get_vcp_version_by_display_handle(dh);
   DBGMSF(debug, "After get_vcp_version_by_display_handle(), dh->dref->vcp_version=%s",
                 format_vspec(dh->dref->vcp_version) );

   ddca_report_parsed_capabilities_by_dref(p_caps, dh->dref, depth);

bye:
   DBGMSF(debug, "Done. Returning %s", ddca_rc_desc(ddcrc));
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
      dyn_report_parsed_capabilities(pcaps, dref, 0);
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


