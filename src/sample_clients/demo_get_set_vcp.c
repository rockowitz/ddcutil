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



static bool saved_report_ddc_errors = false;
static bool saved_verify_setvcp = false;

void set_standard_settings() {
   printf("Seting standard settings in function %s()\n", __func__);
   saved_report_ddc_errors = ddca_is_report_ddc_errors_enabled();
   printf("   Calling ddca_enable_report_ddc_errors(true)...\n");
   ddca_enable_report_ddc_errors(true);
   saved_verify_setvcp = ddca_is_verify_enabled();
   printf("   Calling ddca_set_verify_setvcp(true)...\n");
   ddca_enable_verify(true);
}

void restore_standard_settings() {
   ddca_enable_verify(saved_verify_setvcp);
   ddca_enable_report_ddc_errors(saved_report_ddc_errors);
}





bool verify_cont_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code,
      int                     expected_value)
{


   DDCA_Status rc;
   bool ok = false;
#ifdef OLD
   Single_Vcp_Value * valrec;
   rc = ddca_get_vcp_value(
           dh,
           feature_code,
           DDCA_NON_TABLE_VCP_VALUE,
           &valrec);
   if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
   }
   else {
      if ( valrec->val.c.cur_val != expected_value) {
         printf("   Current value %d does not match expected value %d\n",
                   valrec->val.c.cur_val, expected_value);
      }
#endif
   DDCA_Any_Vcp_Value * valrec;
   rc = ddca_get_any_vcp_value_using_explicit_type(
           dh,
           feature_code,
           DDCA_NON_TABLE_VCP_VALUE_PARM,
           &valrec);
   if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_any_vcp_value", rc);
   }
   else {
      if ( VALREC_CUR_VAL(valrec) != expected_value) {
         printf("   Current value %d does not match expected value %d\n",
                   VALREC_CUR_VAL(valrec), expected_value);
      }
      else {
         printf("   Current value matches expected value\n");
         ok = true;
      }
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
   ddca_enable_verify(false);   // we'll do the check ourselves


   DDCA_Version_Feature_Info * info = NULL;
   rc = ddca_get_feature_info_by_display(
           dh,    // needed because in rare cases feature info is MCCS version dependent
           feature_code,
           &info);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_get_feature_info", rc);
      ok = false;
      goto bye;
   }

#ifdef OLD
   Single_Vcp_Value * valrec;
   rc =
   ddca_get_vcp_value(
         dh,
         feature_code,
         DDCA_NON_TABLE_VCP_VALUE,   // why is this needed?   look it up from dh and feature_code
         &valrec);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
      ok = false;
      goto bye;
   }

   printf("Feature 0x%02x (%s) current value = %d, max value = %d\n",
          feature_code, feature_name,
          valrec->val.c.cur_val,
          valrec->val.c.max_val);

   int old_value = valrec->val.c.cur_val;
#endif
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

   ddca_enable_verify(true);

   uint16_t new_value = old_value/2;
   uint16_t verified_value = 0;
   printf("Setting new value %d,,,\n", new_value);
   rc = ddca_set_continuous_vcp_value(dh, feature_code, new_value, &verified_value);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
      ok = false;
      goto bye;
   }
   printf("Verified value: %d\n", verified_value);

   // printf("Setting new value succeeded.  Verifying the new current value...\n");
   // ok = verify_cont_value(dh, feature_code, new_value);

   printf("Resetting original value %d...\n", old_value);
   rc = ddca_set_continuous_vcp_value(dh, feature_code, old_value, &verified_value);
   if (rc != 0) {
      FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
      ok = false;
      goto bye;
   }

   // printf("Resetting original value succeeded. Verifying the new current value...\n");
   // ok = verify_cont_value(dh, feature_code, old_value) && ok;

bye:
   if (info)
      ddca_free_feature_info(info);
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
#ifdef OLD
   Single_Vcp_Value * valrec;
   rc = ddca_get_vcp_value(
           dh,
           feature_code,
           DDCA_NON_TABLE_VCP_VALUE,
           &valrec);
   if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
   }
   else {
      if ( valrec->val.nc.sl != expected_value) {
         printf("   Current value 0x%02x does not match expected value 0x%02x\n",
                   valrec->val.nc.sl, expected_value);
      }
#endif
      DDCA_Any_Vcp_Value * valrec;
      rc = ddca_get_any_vcp_value_using_explicit_type(
              dh,
              feature_code,
              DDCA_NON_TABLE_VCP_VALUE_PARM,
              &valrec);
      if (rc != 0) {
          FUNCTION_ERRMSG("ddca_get_any_vcp_value", rc);
      }
      else {
         if ( valrec->val.c_nc.sl != expected_value) {
            printf("   Current value 0x%02x does not match expected value 0x%02x\n",
                      valrec->val.c_nc.sl, expected_value);
         }

      else {
         printf("   Current value matches expected value\n");
         ok = true;
      }
   }

   return ok;
}

bool show_simple_nc_feature_value(
        DDCA_Display_Handle   dh,
        DDCA_Vcp_Feature_Code feature_code,
        uint8_t               feature_value)
{
    char * feature_value_name = NULL;
    bool ok = false;

    DDCA_Status rc =
    ddca_get_simple_nc_feature_value_name(
          dh,    // needed because value lookup mccs version dependent
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
    bool ok = true;
    set_standard_settings();
    printf("Disabling automatic verification by calling ddca_enable_verify(false)\n");
    ddca_enable_verify(false);   // we'll do the check ourselves

    DDCA_Version_Feature_Info * info;
    rc = ddca_get_feature_info_by_display(
            dh,    // needed because in rare cases feature info is MCCS version dependent
            feature_code,
            &info);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_feature_info", rc);
       ok = false;
       goto bye;
    }
    assert(info->feature_flags & DDCA_SIMPLE_NC);

#ifdef OLD
    Single_Vcp_Value * valrec;
    rc =
    ddca_get_vcp_value(
          dh,
          feature_code,
          DDCA_NON_TABLE_VCP_VALUE,   // why is this needed?   look it up from dh and feature_code
          &valrec);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
       ok = false;
       goto bye;
    }

    printf("Feature 0x%02x current value = 0x%02x\n",
              feature_code,
              valrec->val.nc.sl);
    uint8_t old_value = valrec->val.nc.sl;
#endif
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

    printf("Feature 0x%02x current value = 0x%02x\n",
              feature_code,
              valrec->val.c_nc.sl);
    uint8_t old_value = valrec->val.c_nc.sl;

    ok = show_simple_nc_feature_value(dh, feature_code, old_value);

    printf("Setting new value 0x%02x...\n", new_value);
    rc = ddca_set_simple_nc_vcp_value(dh, feature_code, new_value);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_set_simple_nc_vcp_value", rc);
       ok = false;
       goto bye;
    }
    else {
       // Checking for demonstration purposes.
       // if ddca_enable_verify(true) is in effect (the default)
       // ddca_...() functions that set feature values also read them for verification
       printf("Setting new value succeeded.  Verifying...\n");
       bool verified = verify_simple_nc_value(dh, feature_code, new_value);
       if (!verified)
          ok = false;
       else {
          ok = show_simple_nc_feature_value(dh, feature_code, new_value) && ok;
       }

       printf("Resetting original value 0x%02x...\n", old_value);
       rc = ddca_set_simple_nc_vcp_value(dh, feature_code, old_value);
       if (rc != 0) {
          FUNCTION_ERRMSG("ddca_set_simple_nc_vcp_value", rc);
          ok = false;
       }
       else {
          printf("Resetting original value succeeded.  Verifying...\n");
          ok = verify_simple_nc_value(dh, feature_code, old_value) && ok;
       }
    }


bye:
    if (info)
       ddca_free_feature_info(info);
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
    bool ok = true;
    set_standard_settings();

    DDCA_Version_Feature_Info * info;
    rc = ddca_get_feature_info_by_display(
            dh,    // needed because in rare cases feature info is MCCS version dependent
            feature_code,
            &info);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_feature_info", rc);
       ok = false;
       goto bye;
    }
    assert(info->feature_flags & (DDCA_COMPLEX_NC|DDCA_NC_CONT));

#ifdef OLD
    Single_Vcp_Value * valrec;
    rc =
    ddca_get_vcp_value(
          dh,
          feature_code,
          DDCA_NON_TABLE_VCP_VALUE,   // why is this needed?   look it up from dh and feature_code
          &valrec);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
       ok = false;
       goto bye;
    }

    printf("Feature 0x%02x current value: mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x\n",
              feature_code,
              valrec->val.nc.mh,
              valrec->val.nc.ml,
              valrec->val.nc.sh,
              valrec->val.nc.sl);
#endif

    DDCA_Any_Vcp_Value * valrec;
    rc =
    ddca_get_any_vcp_value_using_explicit_type(
          dh,
          feature_code,
          DDCA_NON_TABLE_VCP_VALUE_PARM,   // why is this needed?   look it up from dh and feature_code
          &valrec);
    if (rc != 0) {
       FUNCTION_ERRMSG("ddca_get_any_vcp_value", rc);
       ok = false;
       goto bye;
    }

    printf("Feature 0x%02x current value: mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x\n",
              feature_code,
              valrec->val.c_nc.mh,
              valrec->val.c_nc.ml,
              valrec->val.c_nc.sh,
              valrec->val.c_nc.sl);


bye:
    if (info)
       ddca_free_feature_info(info);
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


#ifdef OBSOLETE

// Register an abort function.
// If libddcutil encounters an unexpected, unrecoverable error, it will
// normally exit, causing the calling program to fail.  If the caller registers an
// abort function, that function will be called instead.
void handle_library_abort() {
   // For aborting out of shared library
   static jmp_buf abort_buf;
   int jmprc = setjmp(abort_buf);
   if (jmprc) {
      DDCA_Global_Failure_Information * finfo = ddca_get_global_failure_information();
      if (finfo)
         fprintf(stderr, "(%s) Error %d (%s) in function %s at line %d in file %s\n",
                         __func__, finfo->status, ddca_rc_name(finfo->status), finfo->funcname, finfo->lineno, finfo->fn);
      fprintf(stderr, "(%s) Aborting. Internal status code = %d\n", __func__, jmprc);
      exit(EXIT_FAILURE);
   }
   ddca_register_jmp_buf(&abort_buf);
}

#endif



int main(int argc, char** argv) {
   printf("\n(%s) Starting.\n", __func__);

#ifdef OBSOLETE
   handle_library_abort();
#endif

   // ddca_reset_stats();

   DDCA_Status rc;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning
   int MAX_DISPLAYS = 4;           // limit the number of displays

   DDCA_Display_Info_List * dlist = ddca_get_display_info_list();
   for (int ndx = 0; ndx <  dlist->ct && ndx < MAX_DISPLAYS; ndx++) {
      DDCA_Display_Info * dinfo = &dlist->info[ndx];
      printf("\n(%s) ===> Test loop for display %d\n", __func__, dinfo->dispno);
      // For all the gory details:
      // ddca_report_display_info(dinfo, /* depth=*/ 1);
      dref = dinfo->dref;

      printf("Open display reference %s, creating a display handle...\n", ddca_dref_repr(dref));
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_open_display", rc);
         continue;
      }
      printf("Opened display handle: %s\n", ddca_dh_repr(dh));

      // Comment out the tests you'd like to skip:
      // test_cont_value(dh, 0x10);
      // test_get_set_profile_related_values(dh);

      // feature 0xcc = OSD language, value 0x03 = French
      // test_simple_nc_value(dh, 0xcc, 0x03);

      test_complex_nc_value(dh, 0xDF);    // VCP version

      rc = ddca_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddca_close_display", rc);
      dh = NULL;
   }

   ddca_free_display_info_list(dlist);


// bye:
   // uncomment if you want to see stats:
   // dca_show_stats(DDCA_STATS_ALL, 0);
   return 0;
}
