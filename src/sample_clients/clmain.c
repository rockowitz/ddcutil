/* clmain.c
 *
 * Framework for test code
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


#define DDC_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_rc_name(status_code),      \
          ddca_rc_desc(status_code))


int main(int argc, char** argv) {
   printf("\n(%s) Starting.\n", __func__);

   ddca_reset_stats();

   DDCA_Status rc;

   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning

   DDCA_Display_Info_List* dlist = NULL;
   ddca_get_display_info_list2(
         false,    // don't include invalid displays
         &dlist);

   for (int ndx = 0; ndx <  dlist->ct; ndx++) {
      DDCA_Display_Info * dinfo = &dlist->info[ndx];
      ddca_dbgrpt_display_info(dinfo, /* depth=*/ 1);
      printf("\n(%s) ===> Test loop for display %d\n", __func__, dinfo->dispno);

#ifdef ALT
      DDCA_Display_Identifier did = NULL;
      printf("Create a Display Identifier for display %d...\n", dispno);
      rc = ddca_create_dispno_display_identifier(dispno, &did);

      printf("Create a display reference from the display identifier...\n");
      rc = ddca_get_display_ref(did, &dref);
      assert(rc == 0);

      rc = ddca_free_display_identifier(did);
      printf("ddca_free_display_identifier() returned %d\n", rc);
#endif

      dref = dinfo->dref;

      printf("Open the display reference, creating a display handle...\n");
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         DDC_ERRMSG("ddca_open_display", rc);
         continue;
      }
      printf("(%s) Opened display handle: %s\n", __func__, ddca_dh_repr(dh));

      //
      //  Insert test code here
      //

      rc = ddca_close_display(dh);
      if (rc != 0)
         DDC_ERRMSG("ddca_close_display", rc);
   }

   ddca_show_stats(DDCA_STATS_ALL, 0);
   return 0;
}
