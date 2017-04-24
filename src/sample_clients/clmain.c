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
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"


#define FUNCTION_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_rc_name(status_code),      \
          ddca_rc_desc(status_code))

int
test_cont_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   printf("\n(%s) ===> Starting. dh=%s, feature_code=0x%02x\n",
          __func__, ddca_dh_repr(dh), feature_code);
   DDCA_Status rc;
   bool ok = true;
   char * feature_name = ddca_get_feature_name(feature_code);
   printf("(%s) Feature name: %s\n", __func__, feature_name);

   bool saved_report_ddc_errors = ddca_is_report_ddc_errors_enabled();
   printf("(%s) Calling ddca_enable_report_ddc_errors(true)...\n", __func__);
   ddca_enable_report_ddc_errors(true);
   bool saved_verify_setvcp = ddca_get_verify_setvcp();
   printf("(%s) Calling ddca_set_verify_setvcp(true)...\n", __func__);
   ddca_set_verify_setvcp(true);

   DDCA_Version_Feature_Info * info;
   rc = ddca_get_feature_info_by_display(
           dh,    // needed because in rare cases feature info is MCCS version dependent
           feature_code,
           &info);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddct_get_feature_info", rc);
      ok = false;
   }
   else {
     //  report_ddca_version_feature_flags(feature_code, info->feature_flags);
      // report_version_feature_info(info, 1);

      // TMI
      // exit(info);
   }

   DDCA_Non_Table_Value_Response non_table_response;
   // rc = ddca_get_nontable_vcp_value(dh, feature_code, &non_table_response);

   DDCA_Single_Vcp_Value * valrec;
   rc =
   ddca_get_vcp_value(
         dh,
         feature_code,
         DDCA_NON_TABLE_VCP_VALUE,   // why is this needed?   look it up from dh and feature_code
         &valrec);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
      ok = false;
   }
   else {
      printf("(%s) Feature 0x%02x (%s) current value = %d, max value = %d\n",
             __func__, feature_code, feature_name,
             valrec->val.c.cur_val,
             valrec->val.c.max_val);
      int old_value = valrec->val.c.cur_val;
      int new_value = old_value/2;
      printf("(%s) Setting new value %d,,,\n", __func__, new_value);
      rc = ddca_set_continuous_vcp_value(dh, feature_code, new_value);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
         ok = false;
      }
      else {
         printf("(%s) Setting new value succeeded.  Reading the current value...\n", __func__);
         rc = ddca_get_vcp_value(
               dh,
               feature_code,
               DDCA_NON_TABLE_VCP_VALUE,
               &valrec);
         if (rc != 0) {
            ok = false;
            FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
         }
         else {
            if ( valrec->val.c.cur_val != new_value) {
               printf("(%s) Current value %d does not match new value set %d",
                      __func__, non_table_response.c.cur_val, new_value);
               ok = false;
            }
            else {
               printf("(%s) New current value matches value set\n", __func__);
            }
         }
         printf("(%s) Resetting original value %d...\n", __func__, old_value);
         // reset original value
         rc = ddca_set_continuous_vcp_value(dh, feature_code, old_value);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
            ok = false;
         }
         else {
            printf("(%s) Resetting original value succeeded.\n", __func__);
            // doesn't help DDCRC_NULL_RESPONSE on HWP
            // printf("Sleeping first...\n");
            // usleep(100*1000);

            rc = ddca_get_vcp_value(dh, feature_code,DDCA_NON_TABLE_VCP_VALUE, &valrec);
            if (rc != 0) {
               ok = false;
               FUNCTION_ERRMSG("ddca_get_vcp_vvalue", rc);
            }
            else {
               if ( valrec->val.c.cur_val != old_value) {
                  printf("(%s) Current value %d does not match original current value %d", __func__,
                         valrec->val.c.cur_val, old_value);
                  ok = false;
               }
            }

        }

      }
   }

   ddca_set_verify_setvcp(saved_verify_setvcp);
   ddca_enable_report_ddc_errors(saved_report_ddc_errors);
   printf("(%s) Done. Returning: %d\n", __func__, ok);
   return ok;
}





DDCA_Status test_get_set_profile_related_values(DDCA_Display_Handle dh) {
   printf("\n(%s) ===> Starting.  dh = %s\n", __func__, ddca_dh_repr(dh));
   printf("(%s) Calling ddca_get_profile_related_values()...\n", __func__);
   DDCA_Status psc = 0;
   char* profile_values_string;
   // DBGMSG("&profile_values_string=%p", &profile_values_string);
   psc = ddca_get_profile_related_values(dh, &profile_values_string);
   if (psc != 0) {
      FUNCTION_ERRMSG("ddct_get_profile_related_values", psc);
      return psc;
   }

   printf("(%s) profile values string = %s\n", __func__, profile_values_string);

   bool saved_verify_setvcp = ddca_get_verify_setvcp();
   ddca_set_verify_setvcp(true);
   printf("(%s) Calling ddca_set_profile_related_values()...\n", __func__);
   psc = ddca_set_profile_related_values(profile_values_string);
   if (psc != 0) {
      FUNCTION_ERRMSG("ddca_set_profile_related_values", psc);
   }
   ddca_set_verify_setvcp(saved_verify_setvcp);
   return psc;
}


#ifdef OLD
int test_monitor_detection() {
   printf("\n(%s) ===> Starting.\n", __func__);
   printf("Check for monitors using ddca_get_displays()...\n");
   // Inquire about detected monitors.
   DDCA_Display_Info_List * dlist = ddca_get_display_info_list();
   printf("ddca_get_displays() returned %p\n", dlist);

   // A convenience function to report the result of ddca_get_displays()
   printf("Report the result using ddca_report_display_info_list()...\n");
   ddca_report_display_info_list(dlist, 0);

   // A similar function that hooks directly into the "ddcutil detect" command.
   printf("\nCalling ddca_report_active_displays()...\n");
   int displayct = ddca_report_active_displays(0);
   printf("ddca_report_active_displays() found %d displays\n", displayct);
   return displayct;
}


void test_display_id_ref_handle_use() {
   DDCA_Display_Identifier did;
   DDCA_Display_Ref        dref;
   DDCA_Display_Handle     dh;
   int rc;
   // printf("\nCreate a Display Identifier for display 2...\n");
   // rc = ddca_create_dispno_display_identifier(2, &did);

   printf("\nCreate a Display Identifier for bus 7...\n");
   rc = ddca_create_busno_display_identifier(7, &did);

   // printf("\nCreate a Display Identifier for mfg HWP...\n");
   // rc = ddca_create_mfg_model_sn_display_identifier("HWP", NULL, NULL, &did);

   assert(rc == 0);
   char * did_repr = ddca_repr_display_identifier(did);
   assert(did_repr);
   printf("(%s) did=%s\n", __func__, did_repr);

   printf("\nCreate a display reference from the display identifier...\n");
   rc = ddca_get_display_ref(did, &dref);

   if (rc != 0) {
      printf("(%s) ddct_get_display_ref() returned %d (%s): %s\n",
             __func__, rc, ddca_rc_name(rc), ddca_rc_desc(rc));
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
         char * dh_repr = ddca_dh_repr(dh);
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
            printf("(%s) VCP version id: %s\n", __func__, ddca_mccs_version_id_desc(version_id));
         }

         // test_get_feature_info(version_id);
         test_cont_value(dh, 0x10);
         // test_get_capabilities(dh);
         test_get_set_profile_related_values(dh);

         rc = ddca_close_display(dh);
         if (rc != 0)
            FUNCTION_ERRMSG("ddca_close_display", rc);
      }
   }

}
#endif



int main(int argc, char** argv) {
   printf("\n(%s) Starting.\n", __func__);

   ddca_reset_stats();

   DDCA_Status rc;
   DDCA_Display_Identifier did;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning

   DDCA_Display_Info_List * dlist = ddca_get_display_info_list();
   printf("ddca_get_displays() returned %p\n", dlist);

   for (int ndx = 0; ndx <  dlist->ct; ndx++) {
;

      DDCA_Display_Info * dinfo = &dlist->info[ndx];
      ddca_report_display_info(dinfo, /* depth=*/ 1);
      printf("\n(%s) ===> Test loop for display %d\n", __func__, dinfo->dispno);

      did = NULL;
#ifdef ALT
      printf("Create a Display Identifier for display %d...\n", dispno);
      rc = ddca_create_dispno_display_identifier(dispno, &did);

      printf("Create a display reference from the display identifier...\n");
      rc = ddca_get_display_ref(did, &dref);
      assert(rc == 0);
#endif

      dref = dinfo->dref;

      printf("Open the display reference, creating a display handle...\n");
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_open_display", rc);
         continue;
      }
      char * dh_repr = ddca_dh_repr(dh);
      printf("(%s) Opened display handle: %s\n", __func__, dh_repr);

      test_cont_value(dh, 0x10);

      rc = ddca_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_close_display", rc);
   }


   printf("\n(%s) Cleanup...\n", __func__);
   if (dh) {
      rc = ddca_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_close_display", rc);
   }

   if (did) {
      rc = ddca_free_display_identifier(did);
      printf("(%s) ddca_free_display_identifier() returned %d\n", __func__, rc);
   }

   // ddca_show_stats(0);

// bye:
   ddca_show_stats(DDCA_STATS_ALL, 0);
   return 0;
}
