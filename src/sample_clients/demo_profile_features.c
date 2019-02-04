/* demo_get_set_vcp.c
 *
 * Demonstrates save and restore of profile related features.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"


#define DDC_ERRMSG(function_name,status_code)       \
   do {                                             \
      printf("(%s) %s() returned %d (%s): %s\n",    \
             __func__, function_name, status_code,  \
             ddca_rc_name(status_code),             \
             ddca_rc_desc(status_code));            \
   } while(0)


#define SBOOL(val) ( (val) ? "true" : "false" )


DDCA_Display_Ref  get_dref_by_dispno(int dispno) {
   printf("Getting display reference for display %d...\n", dispno);
   DDCA_Display_Identifier did;
   DDCA_Display_Ref        dref = NULL;

   ddca_create_dispno_display_identifier(1, &did);     // always succeeds
   DDCA_Status rc = ddca_get_display_ref(did, &dref);
   if (rc != 0) {
      DDC_ERRMSG("ddca_create_display_ref", rc);
   }
   return dref;
}


// Parameter restore_using_dh indicates whether an existing open display
// handle should be used when restoring feature values.
//
// Display identification (manufacturre, model, serial number) is included
// in the saved profile value string.  This can be used to open a display
// when restoring values.  Normally this is sufficient.
// However, it is conceivable that that multiple monitors have the same identifiers,
// perhaps because the EDID has been cloned.  Therefore, restoration
// allows for restoring feature values to a display handle that has
// already been opened.


DDCA_Status
demo_get_set_profile_related_values(
      DDCA_Display_Ref dref,
      bool             restore_using_dh)
{
   printf("\nGetting and setting profile related values.  dref = %s, restore_using_dh=%s\n",
          ddca_dref_repr(dref),
          SBOOL(restore_using_dh));

   bool saved_verify_setvcp = ddca_enable_verify(true);

   ddca_reset_stats();
   DDCA_Status ddcrc = 0;
   DDCA_Display_Handle dh = NULL;

   // printf("Open display reference %s, creating a display handle...\n", ddca_dref_repr(dref));
    ddcrc = ddca_open_display(dref, &dh);
    if (ddcrc != 0) {
       DDC_ERRMSG("ddca_open_display", ddcrc);
       goto bye;
    }
    printf("Opened display handle: %s\n", ddca_dh_repr(dh));

   printf("Saving profile related feature values in a string...\n");
   char* profile_values_string;
   ddcrc = ddca_get_profile_related_values(dh, &profile_values_string);
   if (ddcrc != 0) {
      DDC_ERRMSG("ddca_get_profile_related_values", ddcrc);
      goto bye;
   }
   printf("profile values string = %s\n", profile_values_string);

   // must call ddca_close_display() because ddca_set_profile_related_values()
   // will determine the display to load from the stored values
   // and open the display itself
   if (!restore_using_dh) {
      printf("Closing display handle...\n");
      ddca_close_display(dh);
      dh = NULL;
   }

   if (restore_using_dh)
      printf("\nRestoring profile related values using existing display handle...\n");
   else
      printf("\nSelecting display for restore based on identifiers in the value string...\n");
   ddcrc = ddca_set_profile_related_values(dh, profile_values_string);
   if (ddcrc != 0) {
      DDC_ERRMSG("ddca_set_profile_related_values", ddcrc);
   }
   else {
      printf("Profile values successfully restored\n");
   }
   if (restore_using_dh) {
      printf("Closing display handle...\n");
      ddca_close_display(dh);
   }

bye:
   // uncomment if you want to see stats:
   // printf("\nStatistics for one execution of %s()", __func__);
   // ddca_show_stats(DDCA_STATS_ALL, 0);

   ddca_enable_verify(saved_verify_setvcp);
   return ddcrc;
}


int
main(int argc, char** argv) {
   ddca_reset_stats();

   DDCA_Display_Ref dref = get_dref_by_dispno(1);

   demo_get_set_profile_related_values(dref, true);
   demo_get_set_profile_related_values(dref, false);

   return 0;
}
