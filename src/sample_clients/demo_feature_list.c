/* clmain.c
 *
 * Framework for test code
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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
// #include <unistd.h>

// #include "util/string_util.h"

#include "public/ddcutil_c_api.h"


#define FUNCTION_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_rc_name(status_code),      \
          ddca_rc_desc(status_code))



void collect_features_for_display(DDCA_Display_Info * dinfo) {
   DDCA_MCCS_Version_Spec vspec = dinfo->vcp_version;

   DDCA_Status rc = 0;

   // Note that the defined features vary by MCCS version, so
   // can vary by monitor.  The fact that whether a feature is
   // of type table can vary by MCCS version.  So we can't just
   // ask for a superset of features in any very.  Therefore
   // feature lists need to be maintained on a per-monitor basis.

   // get the feature list for feature set PROFILE
   DDCA_Feature_List vcplist1;
   rc = ddca_get_feature_list(
         DDCA_SUBSET_PROFILE,
         vspec,
         false,               // exclude table features
         &vcplist1);
   assert(rc == 0);           // this is sample code


   printf("\nFeatures in feature set PROFILE:  ");
   for (int ndx = 0; ndx < 256; ndx++) {
      if (ddca_feature_list_contains(&vcplist1, ndx))
         printf(" 0x%02x", ndx);
   }
   puts("");

   // Assume we have read the values for the VCP features in PROFILE.
   // The user then changes the feature set to COLOR

   DDCA_Feature_List vcplist2;
   rc = ddca_get_feature_list(
         DDCA_SUBSET_COLOR,
         vspec,
         false,               // exclude table features
         &vcplist2);
   assert(rc == 0);

   // Instead of looping through the codes as above, use convenience
   // function ddca_feature_list_to_codes()
   int codect;
   uint8_t feature_codes[256];
   ddca_feature_list_to_codes(&vcplist2, &codect, feature_codes);

   printf("\nFeatures in feature set COLOR:  ");
   for (int ndx = 0; ndx < codect; ndx++) {
         printf(" 0x%02x", feature_codes[ndx]);
   }
   puts("");

   // We only need to get read the features that have not yet been read
   DDCA_Feature_List vcplist3 = ddca_feature_list_subtract(&vcplist2, &vcplist1);

   printf("\nFeatures in feature set COLOR but not in PROFILE:  ");
   for (int ndx = 0; ndx < 256; ndx++) {
      if (ddca_feature_list_contains(&vcplist3, ndx))
         printf(" 0x%02x", ndx);
   }
   puts("");

}


int main(int argc, char** argv) {
   // printf("\n(%s) Starting.\n", __func__);

    // Just grab the first monitor
    DDCA_Display_Info_List * dlist = ddca_get_display_info_list();
    // printf("ddca_get_displays() returned %p\n", dlist);
    assert(dlist->ct > 0);
    DDCA_Display_Info * dinfo = &dlist->info[0];
    ddca_dbgrpt_display_info(dinfo, /* depth=*/ 1);

    collect_features_for_display(dinfo);

   return 0;
}
