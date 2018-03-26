/* demo_get_set_vcp.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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


#define DDC_ERROR_ABORT(function_name,status_code) \
    do { \
        printf("(%s) %s() returned %d (%s): %s\n",   \
              __func__, function_name, status_code,  \
              ddca_rc_name(status_code),             \
              ddca_rc_desc(status_code));            \
        exit(1);                                     \
    } while(0)






static bool saved_report_ddc_errors = false;
static bool saved_verify_setvcp = false;

void set_standard_settings() {
   printf("Seting standard settings in function %s()\n", __func__);
   saved_report_ddc_errors = ddca_is_report_ddc_errors_enabled();
   printf("   Calling ddca_enable_report_ddc_errors(true)...\n");
   ddca_enable_report_ddc_errors(true);
   printf("   Calling ddca_enable_verify(true)...\n");
   saved_verify_setvcp = ddca_enable_verify(true);
   // ddca_report_error_info(true);
}

void restore_standard_settings() {
   ddca_enable_verify(saved_verify_setvcp);
   ddca_enable_report_ddc_errors(saved_report_ddc_errors);
}


bool verify_continuous_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code,
      int                     expected_value)
{
   DDCA_Status rc;
   bool ok = false;
   DDCA_Any_Vcp_Value * valrec;
   rc = ddca_get_any_vcp_value_using_explicit_type(
           dh,
           feature_code,
           DDCA_NON_TABLE_VCP_VALUE_PARM,
           &valrec);
   if (rc != 0)
       DDC_ERROR_ABORT("ddca_get_any_vcp_value", rc);

   if ( VALREC_CUR_VAL(valrec) != expected_value) {
       printf("   Current value %d does not match expected value %d\n",
                  VALREC_CUR_VAL(valrec), expected_value);
   }
   else {
       printf("   Current value matches expected value\n");
       ok = true;
   }

   return ok;
}




bool
test_cont_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   char * feature_name = ddca_get_feature_name(feature_code);
   printf("\nTesting query and setting continuous value. dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, feature_name);
   DDCA_Status rc;
   bool ok = true;

   printf("Resetting statistics...\n");
   ddca_reset_stats();
   set_standard_settings();
   printf("Disabling automatic verification by calling ddca_enable_verify(false)\n");
   bool saved_enable_verify = ddca_enable_verify(false);   // we'll do the check ourselves


   DDCA_Feature_Metadata info;
   rc = ddca_get_feature_metadata_by_display(
           dh,    // needed because in rare cases feature info is MCCS version dependent
           feature_code,
           &info);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_get_simplified_feature_info", rc);
      ok = false;
      goto bye;
   }
   if ( !(info.feature_flags & DDCA_CONT) ) {
      printf("Feature 0x%02x is not Continuous\n", feature_code);
      ok = false;
      goto bye;
   }


   DDCA_Any_Vcp_Value * valrec;
   rc =
   ddca_get_any_vcp_value_using_explicit_type(
         dh,
         feature_code,
         DDCA_NON_TABLE_VCP_VALUE_PARM,
         &valrec);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_get_any_vcp_value", rc);
      ok = false;
      goto bye;
   }

   printf("Feature 0x%02x (%s) current value = %d, max value = %d\n",
          feature_code, feature_name,
          VALREC_CUR_VAL(valrec),
          VALREC_MAX_VAL(valrec) );

   uint16_t old_value = VALREC_CUR_VAL(valrec) ;

   ddca_enable_verify(saved_enable_verify);

   uint16_t new_value = old_value/2;
   // uint16_t verified_value = 0;
   printf("Setting new value %d,,,\n", new_value);
   // rc = ddca_set_continuous_vcp_value(dh, feature_code, new_value, &verified_value);
   rc = ddca_set_continuous_vcp_value(dh, feature_code, new_value);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
      ok = false;
      goto bye;
   }
   // printf("Verified value: %d\n", verified_value);

   printf("Setting new value succeeded.  Verifying the new current value...\n");
   ok = verify_continuous_value(dh, feature_code, new_value);

   printf("Resetting original value %d...\n", old_value);
   // rc = ddca_set_continuous_vcp_value(dh, feature_code, old_value, &verified_value);
   rc = ddca_set_continuous_vcp_value(dh, feature_code, old_value);
   if (rc != 0) {
      DDC_ERROR_ABORT("ddca_set_continuous_vcp_value", rc);
   }

   printf("Resetting original value succeeded. Verifying the new current value...\n");
   ok = verify_continuous_value(dh, feature_code, old_value) && ok;

bye:
   restore_standard_settings();
   // Uncomment to see statistics:
   // printf("\nStatistics for one execution of %s()", __func__);
   // ddca_show_stats(DDCA_STATS_ALL, 0);

   // printf("(%s) Done. Returning: %d\n", __func__, ok);
   return ok;
}



bool verify_simple_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code,
      uint8_t                 expected_value)
{
    DDCA_Status rc;
    bool ok = false;

    DDCA_Any_Vcp_Value * valrec;
    rc = ddca_get_any_vcp_value_using_explicit_type(
              dh,
              feature_code,
              DDCA_NON_TABLE_VCP_VALUE_PARM,
              &valrec);
    if (rc != 0)
        DDC_ERROR_ABORT("ddca_get_any_vcp_value", rc);

    if ( valrec->val.c_nc.sl != expected_value) {
        printf("   Current value 0x%02x does not match expected value 0x%02x\n",
                   valrec->val.c_nc.sl, expected_value);
    }
    else {
        printf("   Current value matches expected value\n");
        ok = true;
    }

   return ok;
}

bool show_simple_nc_feature_value(
        DDCA_MCCS_Version_Spec vspec,
        DDCA_Vcp_Feature_Code  feature_code,
        uint8_t                feature_value)
{
    char * feature_value_name = NULL;
    bool ok = false;

    DDCA_Status rc =
    ddca_get_simple_nc_feature_value_name_by_vspec(
          vspec,    // needed because value lookup mccs version dependent
          feature_code,
          feature_value,
          &feature_value_name);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_nc_feature_value_name", rc);
       printf("Unable to get interpretation of value 0x%02x\n",  feature_value);
       ok = false;
    }
    else {
       printf("Current value: 0x%02x - %s\n", feature_value, feature_value_name);
       ok = true;
    }

    return ok;
}

bool show_simple_nc_feature_value2(
        DDCA_Feature_Value_Table feature_table,
        uint8_t                  feature_value)
{
    char * feature_value_name = NULL;
    bool ok = false;

    DDCA_Status rc =
    ddca_get_simple_nc_feature_value_name_by_table(
          feature_table,
          feature_value,
          &feature_value_name);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_nc_feature_value_name", rc);
       printf("Unable to get interpretation of value 0x%02x\n",  feature_value);
       ok = false;
    }
    else {
       printf("Current value: 0x%02x - %s\n", feature_value, feature_value_name);
       ok = true;
    }

    return ok;
}



bool test_simple_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code,
      uint8_t                 new_value)
{
   printf("\nTesting query and set of simple NC value: dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, ddca_get_feature_name(feature_code));

    printf("Resetting statistics...\n");
    ddca_reset_stats();
    DDCA_Status rc;
    bool ok = false;
    set_standard_settings();
    // printf("Enabling automatic verification by calling ddca_enable_verify(true)\n");
    // ddca_enable_verify(true);

    DDCA_Feature_Metadata info;
    rc = ddca_get_feature_metadata_by_display(
            dh,    // needed because in rare cases feature info is MCCS version dependent
            feature_code,
            &info);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_simplified_feature_info", rc);
       ok = false;
       goto bye;
    }
    if ( !(info.feature_flags & DDCA_SIMPLE_NC) ) {
       printf("Feature 0x%02x is not simple NC\n", feature_code);
       ok = false;
       goto bye;
    }

#ifdef OLD
    DDCA_MCCS_Version_Spec vspec;
    rc = ddca_get_mccs_version(dh, &vspec);
    assert(rc == 0);
    // DDCA_VSPEC_UNKNOWN is possible for a pre-v2.0 monitor
    // ddca_get_feature_flags_by_vspec() and ddca_get_simple_vcp_feature_name_by_vspec() can
    // handle this

    // vspec = DDCA_VSPEC_UNKNOWN;   // *** TEST ***
    // printf("(%s) vspec=%d.%d\n", __func__, vspec.major, vspec.minor);

    DDCA_Feature_Flags feature_flags;
    rc = ddca_get_feature_flags_by_vspec(
          feature_code,
          vspec,
          &feature_flags);
    assert(rc == 0);
    assert(feature_flags & DDCA_SIMPLE_NC);
#endif

    DDCA_Non_Table_Vcp_Value valrec;
    rc =
    ddca_get_non_table_vcp_value(
          dh,
          feature_code,
          &valrec);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_non_table_vcp_value", rc);
       goto bye;
    }

    printf("Feature 0x%02x current value = 0x%02x\n",
              feature_code,
              valrec.sl);
    uint8_t old_value = valrec.sl;

    /* ok = */ show_simple_nc_feature_value(info.vspec, feature_code, old_value);

    printf("Using     ddca_get_simple_nc_feature_value_name_by_table\n");
    ok = show_simple_nc_feature_value2(info.sl_values, old_value);

    printf("Setting new value 0x%02x...\n", new_value);
    rc = ddca_set_non_table_vcp_value(dh, feature_code, 0, new_value);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_set_non_table_vcp_value", rc);
       goto bye;
    }

    printf("Resetting original value 0x%02x...\n", old_value);
    rc = ddca_set_non_table_vcp_value(dh, feature_code, 0, old_value);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_set_non_table_vcp_value", rc);
       goto bye;
    }

    printf("Resetting original value succeeded.\n");
    ok = true;


bye:
    restore_standard_settings();

    // uncomment to show statistics:
    // printf("\nStatistics for one execution of %s()", __func__);
    // ddca_show_stats(DDCA_STATS_ALL, 0);

    printf("(%s) Done. Returning: %d\n", __func__, ok);
    return ok;
}


// there's no commonly implemented complex NC feature that's writable.  Just read.

bool test_complex_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   printf("\nTesting query of complex NC value: dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, ddca_get_feature_name(feature_code));

    printf("Resetting statistics...\n");
    ddca_reset_stats();

    DDCA_Status rc;
    bool ok = false;
    set_standard_settings();

    DDCA_Feature_Metadata info;
    rc = ddca_get_feature_metadata_by_display(
            dh,    // needed because in rare cases feature info is MCCS version dependent
            feature_code,
            &info);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_feature_info", rc);
       goto bye;
    }
    assert(info.feature_flags & (DDCA_COMPLEX_NC|DDCA_NC_CONT));

    DDCA_Non_Table_Vcp_Value valrec;
    rc =
    ddca_get_non_table_vcp_value(
          dh,
          feature_code,
          &valrec);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_non_table_vcp_value", rc);
       goto bye;
    }

    printf("Feature 0x%02x current value: mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x\n",
              feature_code,
              valrec.mh,
              valrec.ml,
              valrec.sh,
              valrec.sl);


    char * formatted_value;
    rc = ddca_format_non_table_vcp_value(
                feature_code,
                info.vspec,
                &valrec,
                &formatted_value);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_format_non_table_vcp_value", rc);
       goto bye;
    }

    printf("Formatted value: %s\n", formatted_value);
    free(formatted_value);
    ok = true;


bye:
    restore_standard_settings();

    // uncomment to show statistics:
    // printf("\nStatistics for one execution of %s()", __func__);
    // ddca_show_stats(DDCA_STATS_ALL, 0);

    printf("(%s) Done. Returning: %d\n", __func__, ok);
    return ok;
}


DDCA_Status test_get_set_profile_related_values(DDCA_Display_Handle dh) {
   printf("\nTesting retrieval and setting of profile related values.  dh = %s\n", ddca_dh_repr(dh));

   printf("Resetting statistics...\n");
   ddca_reset_stats();

   set_standard_settings();

   printf("Calling ddca_get_profile_related_values()...\n");
   DDCA_Status psc = 0;
   char* profile_values_string;
   psc = ddca_get_profile_related_values(dh, &profile_values_string);
   if (psc != 0) {
      FUNCTION_ERRMSG("ddca_get_profile_related_values", psc);
      goto bye;
   }
   printf("profile values string = %s\n", profile_values_string);

   printf("Calling ddca_set_profile_related_values()...\n");
   psc = ddca_set_profile_related_values(profile_values_string);
   if (psc != 0) {
      FUNCTION_ERRMSG("ddca_set_profile_related_values", psc);
   }

bye:
   restore_standard_settings();

   printf("\nStatistics for one execution of %s()", __func__);
   ddca_show_stats(DDCA_STATS_ALL, 0);

   return psc;
}



int main(int argc, char** argv) {
   // printf("\n(%s) Starting. argc = %d\n", __func__, argc);

   int which_test = 0;
   if (argc > 1) {
      which_test = atoi(argv[1]);   // live dangerously, it's test code
   }

   ddca_reset_stats();

   DDCA_Status rc;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning
   int MAX_DISPLAYS = 4;           // limit the number of displays

   DDCA_Display_Info_List* dlist = NULL;
   ddca_get_display_info_list2(
         false,    // don't include invalid displays
         &dlist);
   for (int ndx = 0; ndx <  dlist->ct && ndx < MAX_DISPLAYS; ndx++) {
      DDCA_Display_Info * dinfo = &dlist->info[ndx];
      printf("\n(%s) ===> Test loop for display %d\n", __func__, dinfo->dispno);
      // For all the gory details:
      // ddca_report_display_info(dinfo, /* depth=*/ 1);
      dref = dinfo->dref;

      // printf("Open display reference %s, creating a display handle...\n", ddca_dref_repr(dref));
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_open_display", rc);
         continue;
      }
      printf("Opened display handle: %s\n", ddca_dh_repr(dh));

      if (which_test == 0 || which_test == 1)
         test_cont_value(dh, 0x10);

      if (which_test == 0 || which_test == 2) {
         // feature 0xcc = OSD language, value 0x03 = French
         test_simple_nc_value(dh, 0xcc, 0x03);
      }

      if (which_test == 0 || which_test == 3)
         test_complex_nc_value(dh, 0xDF);    // VCP version


      if (which_test == 0 || which_test == 4)
         test_get_set_profile_related_values(dh);

      rc = ddca_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_close_display", rc);
      dh = NULL;
   }

   ddca_free_display_info_list(dlist);


// bye:
   // uncomment if you want to see stats:
   // ddca_show_stats(DDCA_STATS_ALL, 0);
   return 0;
}
