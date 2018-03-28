/* demo_capabilities.c
 *
 * Query capabilities string
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
      FUNCTION_ERRMSG("ddca_create_display_ref", rc);
   }
   else {
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_open_display", rc);
      }
      else {
         printf("Opened display handle: %s\n", ddca_dh_repr(dh));
      }
   }
   return dh;
}


/* Retrieves and reports the capabilities string for the first detected monitor.
 */
void demo_get_capabilities() {
   DDCA_Display_Handle dh = open_first_display_by_dispno();
   if (!dh)
      goto bye;

   // printf("\n(%s) ===> Starting.  dh = %s\n", __func__, ddca_dh_repr(dh));
   char * capabilities = NULL;
   printf("(%s) Calling ddca_get_capabilities_string...\n", __func__);
   DDCA_Status rc =  ddca_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      FUNCTION_ERRMSG("ddca_get_capabilities_string", rc);
   else
      printf("(%s) Capabilities: %s\n", __func__, capabilities);
   printf("(%s) Second call should be fast since value cached...\n", __func__);
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
         DDCA_Output_Level saved_ol = ddca_set_output_level(DDCA_OL_NORMAL);
         ddca_report_parsed_capabilities(pcaps, 0);
         ddca_free_parsed_capabilities(pcaps);

         printf("(%s) Use \"ddcutil capabilities\" code to display capabilities...\n", __func__);
         ddca_set_output_level(DDCA_OL_VERBOSE);  // show both unparsed and parsed capabilities
         ddca_parse_and_report_capabilities(capabilities, 1);
         ddca_set_output_level(saved_ol);
      }
   }

bye:
   printf("(%s) Done.\n", __func__);
}


int main(int argc, char** argv) {
   // printf("\ndemo_vcpinfo Starting.\n");

   demo_get_capabilities();

   return 0;
}
