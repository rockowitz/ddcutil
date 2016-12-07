/* clmain.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "base/core.h"

#include "ddc/ddc_dumpload.h"     // loadvcp.h should be elsewhere, should not be including in main

#include "vcp/vcp_feature_codes.h"

#include "public/ddcutil_c_api.h"


#define FUNCTION_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_status_code_name(status_code),      \
          ddca_status_code_desc(status_code))



void test_get_single_feature_info(DDCA_MCCS_Version_Id version_id, Byte feature_code) {
   printf("\n(%s) Getting metadata for feature 0x%02x, mccs version = %s\n", __func__,
          feature_code, ddca_mccs_version_id_string(version_id));
   printf("Feature name: %s\n", ddca_get_feature_name(feature_code));
   // DDCA_Version_Feature_Flags feature_flags;
   Version_Feature_Info * info;
     DDCA_Status rc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, &info);
     if (rc != 0)
        FUNCTION_ERRMSG("ddct_get_feature_info", rc);
     else {
        // TODO: Version_Specific_Feature_Info needs a report function
       //  report_ddca_version_feature_flags(feature_code, info->feature_flags);
        report_version_feature_info(info, 1);
     }
}

void test_get_feature_info(DDCA_MCCS_Version_Id version_id) {
   printf("\n(%s) Starting.  version_id = %s\n", __func__, ddca_repr_mccs_version_id(version_id));
   Byte feature_codes[] = {0x02, 0x03, 0x10, 0x43, 0x60};
   int feature_code_ct = sizeof(feature_codes)/sizeof(Byte);
   int ndx = 0;
   for (; ndx < feature_code_ct; ndx++)
      test_get_single_feature_info(version_id, feature_codes[ndx]);
}


bool
test_cont_value(
      DDCA_Display_Handle  dh,
      Byte                 feature_code)
{
   DDCA_Status rc;
   bool ok = true;
   char * feature_name = ddca_get_feature_name(feature_code);

   // DDCA_Version_Feature_Flags feature_flags;
   Version_Feature_Info * info;
   rc = ddca_get_feature_info_by_display(
           dh,    // needed because in rare cases feature info is MCCS version dependent
           feature_code,
           &info);
   if (rc != 0)
      FUNCTION_ERRMSG("ddct_get_feature_info", rc);
   else {
     //  report_ddca_version_feature_flags(feature_code, info->feature_flags);
      report_version_feature_info(info, 1);
   }

   DDCA_Non_Table_Value_Response non_table_response;
   rc = ddca_get_nontable_vcp_value(dh, feature_code, &non_table_response);

   if (rc != 0)
      FUNCTION_ERRMSG("ddct_set_continuous_vcp_value", rc);
   else {
      printf("(%s) Feature 0x%02x (%s) current value = %d, max value = %d\n",
             __func__, feature_code, feature_name,
             non_table_response.cur_value,
             non_table_response.max_value);
      int cur_value = non_table_response.cur_value;
      int new_value = cur_value/2;
      rc = ddca_set_continuous_vcp_value(dh, feature_code, new_value);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddct_set_continuous_vcp_value", rc);
      }
      else {
         printf("(%s) Setting new value succeeded.\n", __func__);
         rc = ddca_get_nontable_vcp_value(dh, feature_code, &non_table_response);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddct_get_continuous_vcp_value", rc);
         }
         else {
            if ( non_table_response.cur_value != new_value) {
               printf("(%s) Current value %d does not match new value set %d", __func__, non_table_response.cur_value, new_value);
               ok = false;
            }
         }
         // reset original value
         rc = ddca_set_continuous_vcp_value(dh, feature_code, cur_value);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddct_set_continuous_vcp_value", rc);
         }
         else {
            printf("(%s) Resetting original value succeeded.\n", __func__);
            rc = ddca_get_nontable_vcp_value(dh, feature_code, &non_table_response);
            if (rc != 0) {
               FUNCTION_ERRMSG("ddct_get_continuous_vcp_value", rc);
            }
            else {
               rc = ddca_get_nontable_vcp_value(dh, feature_code, &non_table_response);
               if (rc != 0) {
                  FUNCTION_ERRMSG("ddct_get_continuous_vcp_value", rc);
               }
               else {
                  if ( non_table_response.cur_value != cur_value) {
                     printf("(%s) Current value %d does not match original current value %d", __func__,
                            non_table_response.cur_value, cur_value);
                     ok = false;
                  }
               }
            }

        }

      }
   }
   return ( (rc == 0) && ok);
}

bool test_get_capabilities(DDCA_Display_Handle dh) {
   printf("\n(%s) Starting.  dh = %s\n", __func__, ddca_repr_display_handle(dh));
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
   return false;
}



DDCA_Status test_get_set_profile_related_values(DDCA_Display_Handle dh) {
   printf("\n(%s) Calling ddca_get_profile_related_values()...\n", __func__);
   DDCA_Status psc = 0;
   char* profile_values_string;
   // DBGMSG("&profile_values_string=%p", &profile_values_string);
   psc = ddca_get_profile_related_values(dh, &profile_values_string);
   if (psc != 0) {
      FUNCTION_ERRMSG("ddct_get_profile_related_values", psc);
   } else {
      printf("(%s) profile values string = %s\n", __func__, profile_values_string);
   }
   printf("(%s) Calling ddca_set_profile_related_values()...\n", __func__);
   psc = ddca_set_profile_related_values(profile_values_string);
   if (psc != 0) {
      FUNCTION_ERRMSG("ddca_set_profile_related_values", psc);
   }
   return psc;
}




void my_abort_func(Public_Status_Code psc) {
   fprintf(stderr, "(%s) Aborting. Internal status code = %d\n", __func__, psc);
   exit(EXIT_FAILURE);
}


int main(int argc, char** argv) {
   printf("\n(%s) Starting.\n", __func__);

   DDCA_Status rc;
   DDCA_Display_Identifier did;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning

   // Initialize libddcutil.   Must be called first
   ddca_init();

   // Register an abort function.
   // If libddcutil encounters an unexpected, unrecoverable error, it will
   // normally exit, causing the calling program to fail.  If the caller registers an
   // abort function, that function will be called instead.
   ddca_register_abort_func(my_abort_func);

   printf("Probe static build information...\n");
   // Get the ddcutil version as a string in the form "major.minor.micro".
   printf("ddcutil version: %s\n", ddca_ddcutil_version_string() );

   // Query library build settings.
   printf("(%s) Built with ADL support: %s\n", __func__, (ddca_built_with_adl()) ? "yes" : "no");
   printf("(%s) Built with USB support: %s\n", __func__, (ddca_built_with_usb()) ? "yes" : "no");

   unsigned long build_options = ddca_get_build_options();
   printf("(%s) Built with ADL support: %s\n", __func__, (build_options & DDCA_BUILT_WITH_ADL) ? "yes" : "no");
   printf("(%s) Built with USB support: %s\n", __func__, (build_options & DDCA_BUILT_WITH_USB) ? "yes" : "no");


   //
   // Retry management
   //

   printf("\nExercise retry management functions...\n");

   // The maximum retry number that can be specified on ddca_set_max_tries().
   // Any larger number will case the call to fail.
   int max_max_tries = ddca_get_max_max_tries();
   printf("Maximum retry count that can be set: %d\n", max_max_tries);

   rc = ddca_set_max_tries(DDCA_WRITE_READ_TRIES, 15);
   printf("(%s) ddca_set_max_tries(DDCA_WRITE_READ_TRIES,15) returned: %d (%s)\n",
          __func__, rc, ddca_status_code_name(rc) );

   printf("Calling ddca_set_max_tries() with a retry count that's too large...\n");
   int badct = max_max_tries + 1;
   rc = ddca_set_max_tries(DDCA_WRITE_READ_TRIES, badct);
   printf("(%s) ddca_set_max_tries(DDCA_WRITE_READ_TRIES, %d) returned: %d (%s)\n",
          __func__, badct, rc, ddca_status_code_name(rc) );

   printf("Setting the count to exactly max_max_tries works...\n");
   rc = ddca_set_max_tries(DDCA_WRITE_READ_TRIES, max_max_tries);
   printf("(%s) ddca_set_max_tries(DDCA_WRITE_READ_TRIES, %d) returned: %d (%s)\n",
          __func__, max_max_tries, rc, ddca_status_code_name(rc) );

   rc = ddca_set_max_tries(DDCA_MULTI_PART_TRIES, 15);
   if (rc != 0)
      FUNCTION_ERRMSG("DDCT_MULTI_PART_TRIES:ddct_set_max_tries", rc);


   printf("(%s) max write only tries: %d\n", __func__, ddca_get_max_tries(DDCA_WRITE_ONLY_TRIES));
   printf("(%s) max write read tries: %d\n", __func__, ddca_get_max_tries(DDCA_WRITE_READ_TRIES));
   printf("(%s) max multi part tries: %d\n", __func__, ddca_get_max_tries(DDCA_MULTI_PART_TRIES));

   // TODO: Add functions to access ddcutil's runtime statistics.


   printf("\nCheck for monitors using ddca_get_displays()...\n");
   // Inquire about detected monitors.
   DDCA_Display_Info_List * dlist = ddca_get_displays();

   // A convenience function to report the result of ddca_get_displays()
   printf("Report the result using ddca_report_display_info_list()...\n");
   ddca_report_display_info_list(dlist, 0);

   // A similar function that hooks directly into the "ddcutil detect" command.
   printf("\nCalling ddca_report_active_displays()...\n");
   ddca_report_active_displays(0);


   printf("\nCreate a Display Identifier for display 2...\n");
   rc = ddca_create_dispno_display_identifier(2, &did);
   assert(rc == 0);
   char * did_repr = ddca_repr_display_identifier(did);
   assert(did_repr);
   printf("(%s) did=%s\n", __func__, did_repr);

   printf("\nCreate a display reference from the display identifier...\n");
   rc = ddca_create_display_ref(did, &dref);

   if (rc != 0) {
      printf("(%s) ddct_get_display_ref() returned %d (%s): %s\n",
             __func__, rc, ddca_status_code_name(rc), ddca_status_code_desc(rc));
   }
   else {
      char * dref_repr = ddca_repr_display_ref(dref);
      assert(dref_repr);
      printf("(%s) dref=%s\n", __func__, dref_repr);

      printf("\nOpen the display reference, creating a display handle...\n");
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddct_open_display", rc);
      }
      else {
         char * dh_repr = ddca_repr_display_handle(dh);
         printf("(%s) display handle: %s\n", __func__, dh_repr);

         DDCA_MCCS_Version_Spec vspec;
         rc = ddca_get_mccs_version(dh, &vspec);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddca_get_mccs_version_spec", rc);
         }
         else {
            printf("(%s) VCP version: %d.%d\n", __func__, vspec.major, vspec.minor);
         }

         DDCA_MCCS_Version_Id version_id;
         rc = ddca_get_mccs_version_id(dh, &version_id);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddca_get_mccs_version_id", rc);
         }
         else {
            printf("(%s) VCP version id: %s\n", __func__, ddca_mccs_version_id_string(version_id));
         }

         test_get_feature_info(version_id);
         test_cont_value(dh, 0x10);
         test_get_capabilities(dh);
         test_get_set_profile_related_values(dh);

      }
   }


   printf("\n(%s) Cleanup...\n", __func__);
   if (dh) {
      rc = ddca_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_close_display", rc);
   }
   if (dref) {
      rc = ddca_free_display_ref(dref);
      printf("(%s) ddca_free_display_ref() returned %d\n", __func__, rc);
   }
   if (did) {
      rc = ddca_free_display_identifier(did);
      printf("(%s) ddca_free_display_identifier() returned %d\n", __func__, rc);
   }
   return 0;
}
