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


// A simple function that opens the first detected display.
// For a more detailed example of display detection and management,
// see ...
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
   return dh;
}



#ifdef REF
// Exactly 1 of DDCA_RO, DDCA_WO, DDCA_RW is set
#define DDCA_RO           0x0400               /**< Read only feature */
#define DDCA_WO           0x0200               /**< Write only feature */
#define DDCA_RW           0x0100               /**< Feature is both readable and writable */
#define DDCA_READABLE     (DDCA_RO | DDCA_RW)  /**< Feature is either RW or RO */
#define DDCA_WRITABLE     (DDCA_WO | DDCA_RW)  /**< Feature is either RW or WO */

// Further refine the C/NC/TABLE categorization of the MCCS spec
// Exactly 1 of the following 7 bits is set
#define DDCA_STD_CONT       0x80       /**< Normal continuous feature */
#define DDCA_COMPLEX_CONT   0x40       /**< Continuous feature with special interpretation */
#define DDCA_SIMPLE_NC      0x20       /**< Non-continuous feature, having a defined list of values in byte SL */
#define DDCA_COMPLEX_NC     0x10       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define DDCA_WO_NC          0x08       // TODO: CHECK USAGE
#define DDCA_NORMAL_TABLE 0x04       /**< Normal table type feature */
#define DDCA_WO_TABLE       0x02       /**< Write only table feature */

#define DDCA_CONT           (DDCA_STD_CONT|DDCA_COMPLEX_CONT)            /**< Continuous feature, of any subtype */
#define DDCA_NC             (DDCA_SIMPLE_NC|DDCA_COMPLEX_NC|DDCA_WO_NC)  /**< Non-continuous feature of any subtype */
#define DDCA_NON_TABLE      (DDCA_CONT | DDCA_NC)                        /**< Non-table feature of any type */

#define DDCA_TABLE          (DDCA_NORMAL_TABLE | DDCA_WO_TABLE)        /**< Table type feature, of any subtype */
#define DDCA_KNOWN          (DDCA_CONT | DDCA_NC | DDCA_TABLE)           // TODO: Usage??? Check

// Additional bits:
#define DDCA_DEPRECATED     0x01     /**< Feature is deprecated in the specified VCP version */

#endif


/* Create a string representation of Version_Feature_Flags bitfield.
 * The representation is returned in a buffer provided.
 *
 * Arguments:
 *    data       flags
 *    buffer     where to save formatted response
 *    bufsz      buffer size
 *
 * Returns:      buffer
 */
char * interpret_version_feature_flags_r(
          DDCA_Version_Feature_Flags flags,
          char *                     buffer,
          int                        bufsz)
{
   assert(buffer);
   // assert(buffer && bufsz > 150);
   // printf("(%s) bufsz=%d\n", __func__, bufsz);

   snprintf(buffer, bufsz, "%s%s%s%s%s%s%s%s%s%s%s",
       flags & DDCA_RO             ? "Read-Only, "                   : "",
       flags & DDCA_WO             ? "Write-Only, "                  : "",
       flags & DDCA_RW             ? "Read-Write, "                  : "",
       flags & DDCA_STD_CONT       ? "Continuous (standard), "       : "",
       flags & DDCA_COMPLEX_CONT   ? "Continuous (complex), "        : "",
       flags & DDCA_SIMPLE_NC      ? "Non-Continuous (simple), "     : "",
       flags & DDCA_COMPLEX_NC     ? "Non-Continuous (complex), "    : "",
       flags & DDCA_WO_NC          ? "Non-Continuous (write-only), " : "",
       flags & DDCA_NORMAL_TABLE ? "Table (readable), "            : "",
       flags & DDCA_WO_TABLE       ? "Table (write-only), "          : "",
       flags & DDCA_DEPRECATED     ? "Deprecated, "                  : ""
       );
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   // printf("(%s) returning |%s|\n", __func__, buffer);
   return buffer;
}


char * interpret_global_feature_flags_r(
          uint8_t flags,
          char *  buffer,
          int     bufsz)
{
   assert(buffer);
   // assert(buffer && bufsz > 150);
   // printf("(%s) bufsz=%d\n", __func__, bufsz);

   snprintf(buffer, bufsz, "%s",
       flags & DDCA_SYNTHETIC             ? "Dummy Info, "                   : ""
       );
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   // printf("(%s) returning |%s|\n", __func__, buffer);
   return buffer;
}




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
   // printf("info->sl_values = %p\n", info->sl_values);
   const int workbuf_sz = 100;
#define WORKBUF_SZ 100
   char workbuf[workbuf_sz];
   printf("   Version insensitive flags: %s\n",
          interpret_global_feature_flags_r(info->global_flags, workbuf, WORKBUF_SZ));
   printf("   Version sensitive flags: %s\n",
          interpret_version_feature_flags_r(info->feature_flags, workbuf, WORKBUF_SZ));
#undef WORKBUF_SZ
   if (info->sl_values) {
      printf("   SL values: \n");
      DDCA_Feature_Value_Entry * cur_entry = info->sl_values;
      while (cur_entry->value_name) {
         printf("      0x%02x - %s\n", cur_entry->value_code,  cur_entry->value_name);
         cur_entry++;
         // printf("cur_entry=%p\n", cur_entry);
      }
   }
}



void test_get_single_feature_info(
        DDCA_MCCS_Version_Id  version_id,
        DDCA_Vcp_Feature_Code feature_code)
{
   printf("\n(%s) Getting metadata for feature 0x%02x, mccs version = %s\n", __func__,
          feature_code, ddca_mccs_version_id_desc(version_id));
   printf("Feature name: %s\n", ddca_get_feature_name(feature_code));
   // DDCA_Version_Feature_Flags feature_flags;
   DDCA_Version_Feature_Info * info;
  DDCA_Status rc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, &info);
  if (rc != 0)
     FUNCTION_ERRMSG("ddca_get_feature_info_by_vcp_version", rc);
  else {
     // TODO: Version_Specific_Feature_Info needs a report function
    //  report_ddca_version_feature_flags(feature_code, info->feature_flags);
     // report_version_feature_info(info, 1);
     show_version_feature_info(info);
  }
  printf("%s) Done.\n", __func__);
}


void test_get_feature_info(DDCA_MCCS_Version_Id version_id) {
   printf("\n(%s) ===> Starting.  version_id = %s\n", __func__, ddca_mccs_version_id_name(version_id));

   DDCA_Vcp_Feature_Code feature_codes[] = {0x00, 0x02, 0x03, 0x10, 0x43, 0x60, 0xe0};
   int feature_code_ct = sizeof(feature_codes)/sizeof(DDCA_Vcp_Feature_Code);
   for (int ndx = 0; ndx < feature_code_ct; ndx++)
      test_get_single_feature_info(version_id, feature_codes[ndx]);

   printf("%s) Done.\n", __func__);
}

void demo_feature_info() {
   test_get_feature_info(DDCA_V20);
}



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
      FUNCTION_ERRMSG("ddct_get_capabilities_string", rc);
   else
      printf("(%s) Capabilities: %s\n", __func__, capabilities);


   if (capabilities) {
      printf("(%s) Try parsing the string...\n", __func__);
      // ddca_set_output_level(OL_VERBOSE);
      DDCA_Capabilities * pcaps = NULL;
      rc = ddca_parse_capabilities_string(
             capabilities,
             &pcaps);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_parse_capabilities_string", rc);
      else {
         printf("(%s) Parsing succeeded.  Report the result...\n", __func__);
         ddca_report_parsed_capabilities(pcaps, 1);
         ddca_free_parsed_capabilities(pcaps);
      }
   }

bye:
   printf("(%s) Done.\n", __func__);
}








int main(int argc, char** argv) {
   printf("\n(%s) Starting.\n", __func__);

   demo_feature_info();

   demo_get_capabilities();

   return 0;
}
