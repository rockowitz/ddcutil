/** @file demo_capabilities.c
 *
 *  Query capabilities string
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#define _GNU_SOURCE        // for asprintf()

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"


#define DDC_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_rc_name(status_code),      \
          ddca_rc_desc(status_code))


/* A simple function that opens the first detected display using
 * DDCA_create_display_ref() to locate the display.
 *
 * For more detailed examples of display detection and management,
 * see demo_display_selection.c
 *
 * Arguments:    none
 * Returns:      display handle of first detected display,
 *               NULL if not found or can't be opened
 */
DDCA_Display_Handle * open_first_display_by_dispno() {
   printf("Opening display 1...\n");
   DDCA_Display_Identifier did;
   DDCA_Display_Ref        dref;
   DDCA_Display_Handle     dh = NULL;

   ddca_create_dispno_display_identifier(1, &did);     // always succeeds
   DDCA_Status rc = ddca_create_display_ref(did, &dref);
   if (rc != 0) {
      DDC_ERRMSG("ddca_create_display_ref", rc);
   }
   else {
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         DDC_ERRMSG("ddca_open_display", rc);
      }
      else {
         printf("Opened display handle: %s\n", ddca_dh_repr(dh));
      }
   }
   return dh;
}


/* This is a simplified version of API function ddca_report_parsed_capabilities(),
 * illustrating use of DDCA_Capabilities.
 */
void
simple_report_parsed_capabilities(DDCA_Capabilities * pcaps, DDCA_Display_Handle dh)
{
   assert(pcaps && memcmp(pcaps->marker, DDCA_CAPABILITIES_MARKER, 4) == 0);
   printf("Unparsed capabilities string: %s\n", pcaps->unparsed_string);
   printf("VCP version:     %d.%d\n", pcaps->version_spec.major, pcaps->version_spec.minor);
   printf("Command codes:\n");
   for (int cmd_ndx = 0; cmd_ndx < pcaps->cmd_ct; cmd_ndx++) {
      uint8_t cur_code = pcaps->cmd_codes[cmd_ndx];
      printf("   0x%02x\n", cur_code);
   }
   printf("VCP Feature codes:\n");
   for (int code_ndx = 0; code_ndx < pcaps->vcp_code_ct; code_ndx++) {
      DDCA_Cap_Vcp * cur_vcp = &pcaps->vcp_codes[code_ndx];
      assert( memcmp(cur_vcp->marker, DDCA_CAP_VCP_MARKER, 4) == 0);

      char * feature_name = "";
      DDCA_Feature_Value_Entry * feature_value_table = NULL;
      DDCA_Feature_Metadata metadata = {{0}};
      DDCA_Status ddcrc =
      ddca_get_feature_metadata_by_dh(
            cur_vcp->feature_code,
            dh,
            false,     // create_default_if_not_found,
            &metadata);
      if (ddcrc == 0) {
         feature_value_table = metadata.sl_values;
         feature_name = metadata.feature_name;
      }
      printf("   Feature:  0x%02x (%s)\n", cur_vcp->feature_code, feature_name);
      if (cur_vcp->value_ct > 0) {
         printf("      Values:\n");
         for (int ndx = 0; ndx < cur_vcp->value_ct; ndx++) {
            char * value_desc = "No lookup table";
            if (feature_value_table) {
               value_desc = "Unrecognized feature value";

               for (DDCA_Feature_Value_Entry * entry = feature_value_table;
                     entry->value_name;
                     entry++)
               {
                  if (entry->value_code == cur_vcp->feature_code) {
                     value_desc = entry->value_name;
                     break;
                  }
               }

               // Alternatively, use convenience function to look up value description
               // ddca_get_simple_nc_feature_value_name_by_table(
               //     feature_value_table,
               //     cur_vcp->values[ndx],
               //     &value_desc);

            }

            printf("         0x%02x: %s\n", cur_vcp->values[ndx], value_desc);
         }
      }
      ddca_free_feature_metadata_contents(metadata);
   }
}


/* Retrieves and reports the capabilities string for the first detected monitor.
 */
void demo_get_capabilities() {
   DDCA_Display_Handle dh = open_first_display_by_dispno();
   if (!dh)
      goto bye;

   DDCA_Status rc = 0;

   ddca_dfr_check_by_dh(dh);
   if (rc != 0) {
      DDCA_Error_Detail * erec = ddca_get_error_detail();
      ddca_report_error_detail(erec, 1);
      ddca_free_error_detail(erec);
   }


   char * capabilities = NULL;
   printf("Calling ddca_get_capabilities_string...\n");
   rc =  ddca_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      DDC_ERRMSG("ddca_get_capabilities_string", rc);
   else
      printf("Capabilities: %s\n", capabilities);
   printf("Second call to ddca_get_capabilities() should be fast since value cached...\n");
   rc =  ddca_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      DDC_ERRMSG("ddca_get_capabilities_string", rc);
   else {
      printf("Capabilities: %s\n", capabilities);
      printf("Parse the string...\n");
        DDCA_Capabilities * pcaps = NULL;
      rc = ddca_parse_capabilities_string(
             capabilities,
             &pcaps);
      if (rc != 0)
         DDC_ERRMSG("ddca_parse_capabilities_string", rc);
      else {
         printf("Parsing succeeded.\n");
         printf("\nReport the result using local function simple_report_parsed_capabilities()...\n");
         simple_report_parsed_capabilities(pcaps, dh);

         printf("\nReport the result using API function ddca_report_parsed_capabilities()...\n");
         DDCA_Output_Level saved_ol = ddca_set_output_level(DDCA_OL_VERBOSE);
         ddca_report_parsed_capabilities_by_dh(
               pcaps,
               dh,
               0);
         ddca_set_output_level(saved_ol);
         ddca_free_parsed_capabilities(pcaps);
      }
   }

bye:
   return;
}


int main(int argc, char** argv) {
   demo_get_capabilities();
   return 0;
}
