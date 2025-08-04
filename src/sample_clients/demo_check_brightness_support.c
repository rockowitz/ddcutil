/** @file demo_test_brightness.c
 *
 *  Demonstrates the ddca_check_brightness_support() function
 */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"

#define DDC_ERRMSG(function_name,status_code)       \
   do {                                             \
      printf("(%s) %s() returned %d (%s): %s\n",    \
             __func__, function_name, status_code,  \
             ddca_rc_name(status_code),             \
             ddca_rc_desc(status_code));            \
   } while(0)

int main(int argc, char** argv) {
   printf("=== ddcutil Brightness Support Test Demo ===\n\n");
   
   // Get display info list
   DDCA_Display_Info_List* dlist = NULL;
   DDCA_Status ddcrc = ddca_get_display_info_list2(false, &dlist);
   if (ddcrc != 0) {
      DDC_ERRMSG("ddca_get_display_info_list2", ddcrc);
      return 1;
   }
   
   printf("Found %d display(s)\n\n", dlist->ct);
   
   for (int i = 0; i < dlist->ct; i++) {
      DDCA_Display_Info* dinfo = &dlist->info[i];
      printf("Display %d: %s\n", i+1, dinfo->model_name);
      printf("  Manufacturer: %s\n", dinfo->mfg_id);
      printf("  Model: %s\n", dinfo->model_name);
      printf("  Serial Number: %s\n", dinfo->sn);
      
      // Open display
      DDCA_Display_Handle dh;
      ddcrc = ddca_open_display2(dinfo->dref, false, &dh);
      if (ddcrc != 0) {
         DDC_ERRMSG("ddca_open_display2", ddcrc);
         printf("  → Cannot open display\n\n");
         continue;
      }
      
      printf("  → Display opened successfully\n");
      
      // Test brightness support
      bool is_supported;
      uint16_t current_value, max_value;
      
      printf("  Testing brightness support...\n");
      printf("  Note: This will briefly change the display brightness during testing.\n");
      
      ddcrc = ddca_check_brightness_support(dh, &is_supported, &current_value, &max_value);
      
      if (ddcrc != 0) {
         DDC_ERRMSG("ddca_check_brightness_support", ddcrc);
         printf("  → Test failed\n\n");
      } else {
         printf("  → Test completed successfully\n");
         printf("  → Brightness support: %s\n", is_supported ? "YES" : "NO");
         printf("  → Current brightness: %d\n", current_value);
         printf("  → Maximum brightness: %d\n", max_value);
         
         if (is_supported) {
            printf("  → This display supports brightness control\n");
            printf("  → You can safely use ddca_set_non_table_vcp_value() for brightness\n");
         } else {
            printf("  → This display does NOT support brightness control\n");
            printf("  → Brightness control functions will fail\n");
         }
      }
      
      // Close display
      ddca_close_display(dh);
      printf("\n");
   }
   
   ddca_free_display_info_list(dlist);
   
   printf("=== Test completed ===\n");
   return 0;
} 