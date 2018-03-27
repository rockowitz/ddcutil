/* demo_vcpinfo.c
 *
 * Query VCP feature information and capabilities string
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

#define DDC_ERROR_ABORT(function_name,status_code) \
    do { \
        printf("(%s) %s() returned %d (%s): %s\n",   \
              __func__, function_name, status_code,  \
              ddca_rc_name(status_code),             \
              ddca_rc_desc(status_code));            \
        exit(1);                                     \
    } while(0)


/* A simple function that opens the first detected display.
 * For more detailed exampled of display detection and management,
 * see demo_display_selection.c
 *
 * Arguments:    none
 * Returns:      display handle of first detected display,
 *               NULL if not found or can't be opened
 */
DDCA_Display_Handle * open_first_display_by_dispno() {
   printf("Check for monitors using ddca_get_displays()...\n");
   DDCA_Display_Handle dh = NULL;

   // Inquire about detected monitors.
   DDCA_Display_Info_List* dlist = NULL;
   ddca_get_display_info_list2(
         false,    // don't include invalid displays
         &dlist);
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
 *    flags      feature characteristics
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


/* Displays the contents of DDCA_Feature_Metadata instance.
 *
 * Arguments:
 *   info     pointer to DDCA_Feature_Metadata instance
 */
void show_feature_metadata(DDCA_Feature_Metadata * info) {
   printf("\nVersion Sensitive Feature Metadata for VCP Feature: 0x%02x - %s\n",
           info->feature_code, info->feature_name);
   printf("   VCP version:          %d.%d\n",   info->vspec.major, info->vspec.minor);
   printf("   Description:          %s\n",  info->feature_desc);
   char * s = interpret_feature_flags(info->feature_flags);
   printf("   Feature flags:        %s\n", s);
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


/*  Retrieves and displays feature information for a specified MCCS version
 *  and feature code.
 *
 *  Arguments:
 *     vspec
 *     feature_code
 */
void test_get_single_feature_info(
        DDCA_MCCS_Version_Spec  vspec,
        DDCA_Vcp_Feature_Code   feature_code)
{
   printf("\n(%s) Getting metadata for feature 0x%02x, mccs version = %d.%d\n", __func__,
          feature_code, vspec.major, vspec.minor);
   printf("Feature name: %s\n", ddca_feature_name_by_vspec(feature_code, vspec));
   DDCA_Feature_Metadata metadata;
   DDCA_Status rc = ddca_get_feature_metadata_by_vspec(vspec, feature_code, &metadata);
   if (rc != 0)
      FUNCTION_ERRMSG("ddca_get_feature_info_by_vcp_version", rc);
   else {
      show_feature_metadata(&metadata);
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
void test_get_feature_info(DDCA_MCCS_Version_Spec vspec) {
   printf("\n(%s) ===> Starting.  version = %d.%d\n", __func__, vspec.major, vspec.minor);

   // DDCA_Vcp_Feature_Code feature_codes[] = {0x00, 0x02, 0x03, 0x10, 0x43, 0x60, 0xe0};
   DDCA_Vcp_Feature_Code feature_codes[] = {0x02};
   int feature_code_ct = sizeof(feature_codes)/sizeof(DDCA_Vcp_Feature_Code);
   for (int ndx = 0; ndx < feature_code_ct; ndx++)
      test_get_single_feature_info(vspec, feature_codes[ndx]);

   printf("%s) Done.\n", __func__);
}


void demo_feature_info() {
   test_get_feature_info(DDCA_VSPEC_V10);
   test_get_feature_info(DDCA_VSPEC_V20);
   test_get_feature_info(DDCA_VSPEC_V21);
   test_get_feature_info(DDCA_VSPEC_V30);
   test_get_feature_info(DDCA_VSPEC_V22);
   test_get_feature_info(DDCA_VSPEC_UNKNOWN);
}


int main(int argc, char** argv) {
   printf("\ndemo_vcpinfo Starting.\n");

   demo_feature_info();

   return 0;
}
