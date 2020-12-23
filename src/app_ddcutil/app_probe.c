/** \file app_probe.c
  * Implement PROBE command
  */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "public/ddcutil_types.h"

#include "util/report_util.h"

#include "base/core.h"
#include "base/displays.h"

#include "vcp/parse_capabilities.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "app_ddcutil/app_getvcp.h"
#include "app_ddcutil/app_capabilities.h"

#include "app_ddcutil/app_probe.h"

void app_probe_display_by_dh(Display_Handle * dh)
{
   FILE * fout = stdout;
   bool debug = false;
   DBGMSF(debug, "Starting. dh=%s", dh_repr(dh));
   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;

   Parsed_Edid * pedid = dh->dref->pedid;
   f0printf(fout, "\nEDID version: %d.%d", pedid->edid_version_major, pedid->edid_version_minor);
   f0printf(fout, "\nMfg id: %s, model: %s, sn: %s\n",
                  pedid->mfg_id, pedid->model_name, pedid->serial_ascii);
   f0printf(fout,   "Product code: %u, binary serial number %"PRIu32" (0x%08x)\n",
                  pedid->product_code, pedid->serial_binary, pedid->serial_binary);

   Dref_Flags flags = dh->dref->flags;
   char interpreted[200];
#define FLAG_NAME(_flag) (flags & _flag) ? #_flag : ""
   g_snprintf(interpreted, 200, "%s%s%s%s",
         FLAG_NAME(DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED),
         FLAG_NAME(DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED),
         FLAG_NAME(DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED),
         FLAG_NAME(DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED) );
         f0printf(fout, "\nHow display reports unsupported feature: %s\n", interpreted);
#undef FLAG_NAME

   f0printf(fout, "\nCapabilities for display on %s\n", dref_short_name_t(dh->dref));

   DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
   // not needed, message causes confusing messages if get_vcp_version fails but get_capabilities succeeds
   // if (vspec.major < 2) {
   //    printf("VCP (aka MCCS) version for display is less than 2.0. Output may not be accurate.\n");
   // }

   // reports capabilities, and if successful returns Parsed_Capabilities
   DDCA_Output_Level saved_ol = get_output_level();
   set_output_level(DDCA_OL_VERBOSE);

#ifdef OLD
   Parsed_Capabilities * pcaps =  app_get_capabilities_by_dh(dh);
   if (pcaps) {
      app_show_parsed_capabilities(pcaps->raw_value,dh,  pcaps);
   }
#endif
   char * capabilities_string;
   DDCA_Status ddcrc;
   Parsed_Capabilities * pcaps = NULL;


   ddcrc = app_get_capabilities_string(dh, &capabilities_string);
   if (ddcrc == 0) {
         // pcaps is always set, but may be damaged if there was a parsing error
         pcaps = parse_capabilities_string(capabilities_string);
         app_show_parsed_capabilities(dh, pcaps);

         // how to pass this information down into app_show_vcp_subset_values_by_dh()?
         bool table_reads_possible = parsed_capabilities_supports_table_commands(pcaps);
         f0printf(fout, "\nMay support table reads:   %s\n", sbool(table_reads_possible));
   }

   set_output_level(saved_ol);


   // *** VCP Feature Scan ***
   // printf("\n\nScanning all VCP feature codes for display %d\n", dispno);
   f0printf(fout, "\nScanning all VCP feature codes for display %s\n", dh_repr(dh) );
   Byte_Bit_Flags features_seen = bbf_create();
   app_show_vcp_subset_values_by_dh(
         dh, VCP_SUBSET_SCAN, FSF_SHOW_UNSUPPORTED, features_seen);

   if (pcaps) {
      f0printf(fout, "\n\nComparing declared capabilities to observed features...\n");
      Byte_Bit_Flags features_declared =
            get_parsed_capabilities_feature_ids(pcaps, /*readable_only=*/true);
      char * s0 = bbf_to_string(features_declared, NULL, 0);
      f0printf(fout, "\nReadable features declared in capabilities string: %s\n", s0);
      free(s0);

      Byte_Bit_Flags caps_not_seen = bbf_subtract(features_declared, features_seen);
      Byte_Bit_Flags seen_not_caps = bbf_subtract(features_seen, features_declared);

      f0printf(fout, "\nMCCS (VCP) version reported by capabilities: %s\n",
               format_vspec(pcaps->parsed_mccs_version));
      f0printf(fout, "MCCS (VCP) version reported by feature 0xDf: %s\n",
               format_vspec(vspec));
      if (!vcp_version_eq(pcaps->parsed_mccs_version, vspec))
         f0printf(fout, "Versions do not match!!!\n");

      if (bbf_count_set(caps_not_seen) > 0) {
         f0printf(fout, "\nFeatures declared as readable capabilities but not found by scanning:\n");
         for (int code = 0; code < 256; code++) {
            if (bbf_is_set(caps_not_seen, code)) {
               VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);
               Display_Feature_Metadata * dfm =
                     dyn_get_feature_metadata_by_dh(
                        code,
                         dh,
                         true);   //  with_default
               char * feature_name = get_version_sensitive_feature_name(vfte, pcaps->parsed_mccs_version);
               if (!streq(feature_name, dfm->feature_name)) {
                  rpt_vstring(1, "VCP_Feature_Table_Entry feature name: %s", feature_name);
                  rpt_vstring(1, "Display_Feature_Metadata feature name: %s",
                                 dfm->feature_name);
                  f0printf(fout, "   Feature x%02x - %s, (alt.) %s\n", code, feature_name, dfm->feature_name);
               }
               else {
                  // assert( streq(feature_name, ifm->external_metadata->feature_name));
                  f0printf(fout, "   Feature x%02x - %s\n", code, feature_name);
               }
               if (vfte->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
                  free_synthetic_vcp_entry(vfte);
               }
               // need to free ifm?
            }
         }
      }
      else
         f0printf(fout, "\nAll readable features declared in capabilities were found by scanning.\n");

      if (bbf_count_set(seen_not_caps) > 0) {
         f0printf(fout, "\nFeatures found by scanning but not declared as capabilities:\n");
         for (int code = 0; code < 256; code++) {
            if (bbf_is_set(seen_not_caps, code)) {
               VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(code);

               Display_Feature_Metadata * dfm =
                     dyn_get_feature_metadata_by_dh(
                        code,
                         dh,
                         true);   //  with_default
               char * feature_name = get_version_sensitive_feature_name(vfte, vspec);
               f0printf(fout, "   Feature x%02x - %s\n", code, feature_name);
               if (!streq(feature_name, dfm->feature_name)) {
                  rpt_vstring(1, "VCP_Feature_Table_Entry feature name: %s", feature_name);
                  rpt_vstring(1, "Internal_Feature_Metadata feature name: %s",
                                 dfm->feature_name);
               }
               // assert( streq(feature_name, ifm->external_metadata->feature_name));
               if (vfte->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
                  free_synthetic_vcp_entry(vfte);
               }
               // free ifm
            }
         }
      }
      else
         f0printf(fout, "\nAll features found by scanning were declared in capabilities.\n");

      bbf_free(features_declared);
      bbf_free(caps_not_seen);
      bbf_free(seen_not_caps);
      free_parsed_capabilities(pcaps);
   }
   else {
      f0printf(fout, "\n\nUnable to read or parse capabilities.\n");
      f0printf(fout, "Skipping comparison of declared capabilities to observed features\n");
   }
   bbf_free(features_seen);


   puts("");
   // get VCP 0B
   DDCA_Any_Vcp_Value * valrec;
   int color_temp_increment = 0;
   int color_temp_units = 0;
   ddc_excp = ddc_get_vcp_value(
                 dh,
                 0x0b,              // color temperature increment,
                 DDCA_NON_TABLE_VCP_VALUE,
                 &valrec);
   psc = ERRINFO_STATUS(ddc_excp);
   if (psc == 0) {
      if (debug)
         f0printf(fout, "Value returned for feature x0b: %s\n", summarize_single_vcp_value(valrec) );
      color_temp_increment = valrec->val.c_nc.sl;
      free_single_vcp_value(valrec);

      ddc_excp = ddc_get_vcp_value(
                    dh,
                    0x0c,              // color temperature request
                    DDCA_NON_TABLE_VCP_VALUE,
                    &valrec);
      psc = ERRINFO_STATUS(ddc_excp);
      if (psc == 0) {
         if (debug)
            f0printf(fout, "Value returned for feature x0c: %s\n", summarize_single_vcp_value(valrec) );
         color_temp_units = valrec->val.c_nc.sl;
         int color_temp = 3000 + color_temp_units * color_temp_increment;
         f0printf(fout, "Color temperature increment (x0b) = %d degrees Kelvin\n", color_temp_increment);
         f0printf(fout, "Color temperature request   (x0c) = %d\n", color_temp_units);
         f0printf(fout, "Requested color temperature = (3000 deg Kelvin) + %d * (%d degrees Kelvin)"
               " = %d degrees Kelvin\n",
               color_temp_units,
               color_temp_increment,
               color_temp);
      }
   }
   if (psc != 0) {
      f0printf(fout, "Unable to calculate color temperature from VCP features x0B and x0C\n");
      // errinfo_free(ddc_excp);
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
   }
   // get VCP 14
   // report color preset

   DBGMSF(debug, "Done.");
}


void app_probe_display_by_dref(Display_Ref * dref) {
   FILE * fout = stdout;
   Display_Handle * dh = NULL;
   Public_Status_Code psc = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
   if (psc != 0) {
      f0printf(fout, "Unable to open display %s, status code %s",
                     dref_short_name_t(dref), psc_desc(psc) );
   }
   else {
      app_probe_display_by_dh(dh);
      ddc_close_display(dh);
   }
}
