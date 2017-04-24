/* demo_display_selection.c
 *
 * This file contains detailed examples of display selection.
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
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


/*
   Overview goes here.


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



DDCA_Display_Ref display_selection_using_ddca_get_displays() {
   printf("\nCheck for monitors using ddca_get_displays()...\n");
   // Inquire about detected monitors.
   DDCA_Display_Info_List * dlist = ddca_get_display_info_list();
   printf("   ddca_get_displays() returned %p\n", dlist);

   // A convenience function to report the result of ddca_get_displays()
   printf("   Report the result using ddca_report_display_info_list()...\n");
   ddca_report_display_info_list(dlist, 2);

   // A similar function that hooks directly into the "ddcutil detect" command.
   printf("\n   Calling ddca_report_active_displays()...\n");
   int displayct = ddca_report_active_displays(2);
   printf("ddca_report_active_displays() found %d displays\n", displayct);

   // This example selects the monitor by its ddcutil assigned display number,
   // since any working ddcutil installation will have at least 1 display.
   // In practice, selection could be performed using any of the monitor
   // description fields in DDCA_Display_Info.

   DDCA_Display_Ref dref = NULL;
   int desired_display_number = 1;
   for (int ndx = 0; ndx < dlist->ct; ndx++) {
      if (dlist->info[ndx].dispno == desired_display_number) {
         dref = dlist->info[ndx].dref;
         break;
      }
   }

   if (dref) {
      printf("Found display: %s\n", ddca_dref_repr(dref) );

      // printf("Detailed debug report:\n");
      // ddca_report_display_ref(
      //       dref,
      //       1);      // logical indentation depth
   }
   else
      printf("Display number %d not found.\n", desired_display_number);

   // dref is an (opaque) pointer to an internal ddcutil data structure.
   // It does not need to be freed.

   ddca_free_display_info_list(dlist);

   return dref;
}


DDCA_Display_Ref display_selection_using_display_identifier() {

   DDCA_Display_Identifier did;
   DDCA_Display_Ref        dref;
   DDCA_Status rc;

   printf("\nExamples of display identifier creation:\n");

   printf("\nCreate a Display Identifier using I2C bus number\n");
   ddca_create_busno_display_identifier(7, &did); // always succeeds
   printf("Created display identifier: %s\n", ddca_did_repr(did) );
   ddca_free_display_identifier(did);

   printf("\nCreate a Display Identifier using mfg code and model\n");
   rc = ddca_create_mfg_model_sn_display_identifier("ACI", "VE247", NULL, &did);
   assert(rc == 0);
   printf("Created display identifier: %s\n", ddca_did_repr(did) );
   ddca_free_display_identifier(did);

   printf("\nCalling ddca_create_mfg_model_sn_display_identifier() with an invalid argument fails\n");
   rc = ddca_create_mfg_model_sn_display_identifier(NULL, "Model name longer than 13 chars", NULL, &did);
   if (rc != 0) {
      printf("   ddca_create_mfg_model_sn_display_identifier() returned %d (%s): %s\n",
              rc, ddca_rc_name(rc), ddca_rc_desc(rc));
   }

   printf("\nCreate a Display Identifier for display 1...\n");
   ddca_create_dispno_display_identifier(1, &did);     // always succeeds
   printf("Created display identifier: %s\n", ddca_did_repr(did) );

   printf("\nFind a display reference for the display identifier...\n");
   rc = ddca_get_display_ref(did, &dref);
   if (rc != 0) {
      printf("     ddca_get_display_ref() returned %d (%s): %s\n",
               rc, ddca_rc_name(rc), ddca_rc_desc(rc));
   }
   else {
       printf("Found display reference: %s\n", ddca_dref_repr(dref) );
   }

   return dref;
}


DDCA_Display_Ref demo_get_display_ref() {

   DDCA_Display_Ref dref1 = display_selection_using_ddca_get_displays();
   DDCA_Display_Ref dref2 = display_selection_using_display_identifier();
   assert(dref1 == dref2);

   // printf("Debug report on display reference:\n");
   // ddca_report_display_ref(dref1, 2);

   return dref1;
}


void demo_use_display_ref(DDCA_Display_Ref dref) {
   DDCA_Status rc;
   DDCA_Display_Handle dh;

   printf("\nOpen the display reference, creating a display handle...\n");
   rc = ddca_open_display(dref, &dh);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_open_display", rc);
   }
   else {
      printf("   display handle: %s\n",  ddca_dh_repr(dh));

      DDCA_MCCS_Version_Spec vspec;
      rc = ddca_get_mccs_version(dh, &vspec);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_get_mccs_version_spec", rc);
      }
      else {
         printf("VCP version: %d.%d\n", vspec.major, vspec.minor);
      }

      DDCA_MCCS_Version_Id version_id;
      rc = ddca_get_mccs_version_id(dh, &version_id);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_get_mccs_version_id", rc);
      }
      else {
         printf("VCP version id: %s\n", ddca_mccs_version_id_desc(version_id));
      }

      rc = ddca_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_close_display", rc);
  }
}



int main(int argc, char** argv) {
   printf("\nStarting display selection example....\n");

   DDCA_Display_Ref dref = demo_get_display_ref();
   if (dref) {
      demo_use_display_ref(dref);
   }

}
