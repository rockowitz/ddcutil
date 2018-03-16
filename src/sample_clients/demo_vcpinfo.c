/* demo_vcpinfo.c
 *
 * Query VCP feature information
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

#define _GNU_SOURCE        // for asprintf()

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"


#define FUNCTION_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_rc_name(status_code),      \
          ddca_rc_desc(status_code))


/* A simple function that opens the first detected display.
 * For a more detailed example of display detection and management,
 * see demo_display_selection.c
 *
 * Arguments:    none
 * Returns:      display handle of first detected display,
 *               NULL if not found or can't be opened
 */
DDCA_Display_Handle * open_first_display() {
   printf("Check for monitors using ddca_get_displays()...\n");
   DDCA_Display_Handle dh = NULL;

   // Inquire about detected monitors.
   DDCA_Display_Info_List * dlist = ddca_get_display_info_list();
   printf("ddca_get_displays() returned %p\n", dlist);
   assert(dlist);   // this is sample code
   if (dlist->ct == 0) {
      printf("   No DDC capable displays found\n");
   }
   else {
      DDCA_Display_Info * dinf = &dlist->info[0];
      DDCA_Display_Ref * dref = dinf->dref;
      printf("Opening display %s\n", dinf->model_name);
      DDCA_Status rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_open_display", rc);
      }
   }
   ddca_free_display_info_list(dlist);
   return dh;
}


/* Creates a string representation of DDCA_Feature_Flags bitfield.
 *
 * Arguments:
 *    data       flags
 *
 * Returns:      string representation, caller must free
 */
char * interpret_feature_flags(DDCA_Version_Feature_Flags flags) {
   char * buffer = NULL;
   int rc = asprintf(&buffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s",
       flags & DDCA_RO             ? "Read-Only, "                   : "",
       flags & DDCA_WO             ? "Write-Only, "                  : "",
       flags & DDCA_RW             ? "Read-Write, "                  : "",
       flags & DDCA_STD_CONT       ? "Continuous (standard), "       : "",
       flags & DDCA_COMPLEX_CONT   ? "Continuous (complex), "        : "",
       flags & DDCA_SIMPLE_NC      ? "Non-Continuous (simple), "     : "",
       flags & DDCA_COMPLEX_NC     ? "Non-Continuous (complex), "    : "",
       flags & DDCA_NC_CONT        ? "Non-Continuous with continuous subrange, " :"",
       flags & DDCA_WO_NC          ? "Non-Continuous (write-only), " : "",
       flags & DDCA_NORMAL_TABLE   ? "Table (readable), "            : "",
       flags & DDCA_WO_TABLE       ? "Table (write-only), "          : "",
       flags & DDCA_DEPRECATED     ? "Deprecated, "                  : "",
       flags & DDCA_SYNTHETIC      ? "Synthesized, "                 : ""
       );
   assert(rc >= 0);   // real life code would check for malloc() failure in asprintf()
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   return buffer;
}


/* Displays the contents of DDCA_Version_Feature_Info instance.
 *
 * Arguments:
 *   info     pointer to DDCA_Version_Feature_Info
 */
void show_version_feature_info(DDCA_Version_Feature_Info * info) {
   printf("\nVersion Sensitive Feature Information for VCP Feature: 0x%02x - %s\n",
           info->feature_code, info->feature_name);
   printf("   VCP version:          %d.%d\n",   info->vspec.major, info->vspec.minor);
   printf("   VCP version id:       %d (%s) - %s \n",
                  info->version_id,
                  ddca_mccs_version_id_name(info->version_id),
                  ddca_mccs_version_id_desc(info->version_id)
             );
   printf("   Description:          %s\n",  info->desc);
   char * s = interpret_feature_flags(info->feature_flags);
   printf("   Feature flags: %s\n", s);
   free(s);
   if (info->sl_values) {
      printf("   SL values: \n");
      DDCA_Feature_Value_Entry * cur_entry = info->sl_values;
      while (cur_entry->value_name) {
         printf("      0x%02x - %s\n", cur_entry->value_code,  cur_entry->value_name);
         cur_entry++;
      }
   }
}


/** Retrieves and displays feature information for a specified MCCS version
 *  and feature code.
 *
 *  Arguments:
 *     version_id
 *     feature_code
 */
void test_get_single_feature_info(
        DDCA_MCCS_Version_Id  version_id,
        DDCA_Vcp_Feature_Code feature_code)
{
   printf("\n(%s) Getting metadata for feature 0x%02x, mccs version = %s\n", __func__,
          feature_code, ddca_mccs_version_id_desc(version_id));
   printf("Feature name: %s\n", ddca_get_feature_name(feature_code));
   // DDCA_Version_Feature_Flags feature_flags;
   DDCA_Version_Feature_Info * info = NULL;
   DDCA_Status rc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, &info);
   if (rc != 0)
      FUNCTION_ERRMSG("ddca_get_feature_info_by_vcp_version", rc);
   else {
      // TODO: Version_Specific_Feature_Info needs a report function
      //  report_ddca_version_feature_flags(feature_code, info->feature_flags);
      // report_version_feature_info(info, 1);
      show_version_feature_info(info);
      ddca_free_feature_info(info);
   }
   printf("%s) Done.\n", __func__);
}


/** Retrieves and displays feature information for a specified MCCS version
 *  and a representative sample of feature codes.
 *
 *  Arguments:
 *     version_id
 *     feature_code
 */
void test_get_feature_info(DDCA_MCCS_Version_Id version_id) {
   printf("\n(%s) ===> Starting.  version_id = %s\n", __func__, ddca_mccs_version_id_name(version_id));

   DDCA_Vcp_Feature_Code feature_codes[] = {0x00, 0x02, 0x03, 0x10, 0x43, 0x60, 0xe0};
   int feature_code_ct = sizeof(feature_codes)/sizeof(DDCA_Vcp_Feature_Code);
   for (int ndx = 0; ndx < feature_code_ct; ndx++)
      test_get_single_feature_info(version_id, feature_codes[ndx]);

   printf("%s) Done.\n", __func__);
}



void demo_feature_info() {
   test_get_feature_info(DDCA_MCCS_V10);
   test_get_feature_info(DDCA_MCCS_V20);
   test_get_feature_info(DDCA_MCCS_V21);
   test_get_feature_info(DDCA_MCCS_V30);
   test_get_feature_info(DDCA_MCCS_V22);
}


/* Retrieves and reports the capabilities string for the first detected monitor.
 */
void demo_get_capabilities() {
   DDCA_Display_Handle dh = open_first_display();
   if (!dh)
      goto bye;

   printf("\n(%s) ===> Starting.  dh = %s\n", __func__, ddca_dh_repr(dh));
   char * capabilities = NULL;
   DDCA_Status rc =  ddca_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      FUNCTION_ERRMSG("ddca_get_capabilities_string", rc);
   else
      printf("(%s) Capabilities: %s\n", __func__, capabilities);
   printf("(%s) Second call should be fast\n", __func__);
   rc =  ddca_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      FUNCTION_ERRMSG("ddca_get_capabilities_string", rc);
   else {
      printf("(%s) Capabilities: %s\n", __func__, capabilities);
      printf("(%s) Try parsing the string...\n", __func__);
        DDCA_Capabilities * pcaps = NULL;
      rc = ddca_parse_capabilities_string(
             capabilities,
             &pcaps);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_parse_capabilities_string", rc);
      else {
         printf("(%s) Parsing succeeded.  Report the result...\n", __func__);
         // DDCA_Output_Level saved_ol = ddca_get_output_level();
         // ddca_set_output_level(DDCA_OL_VERBOSE);  // show both unparsed and parsed capabilities
         ddca_report_parsed_capabilities(pcaps, 1);
         // ddca_set_output_level(saved_ol);
         ddca_free_parsed_capabilities(pcaps);
      }
   }

bye:
   printf("(%s) Done.\n", __func__);
}


int main(int argc, char** argv) {
   printf("\ndemo_vcpinfo Starting.\n");

   demo_feature_info();

   demo_get_capabilities();

   return 0;
}
