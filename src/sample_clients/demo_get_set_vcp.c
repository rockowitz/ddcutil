/* demo_get_set_vcp.c
 *
 * Demonstrates getting, setting, and interpreting VCP feature values.
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


static bool saved_report_ddc_errors = false;
static bool saved_verify_setvcp = false;

void
set_standard_settings() {
   printf("Seting standard settings in function %s()\n", __func__);
   saved_report_ddc_errors = ddca_is_report_ddc_errors_enabled();
   // printf("   Calling ddca_enable_report_ddc_errors(true)...\n");
   // ddca_enable_report_ddc_errors(true);
   printf("   Calling ddca_enable_verify(true)...\n");
   saved_verify_setvcp = ddca_enable_verify(true);
   // ddca_report_error_info(true);
}

void
restore_standard_settings() {
   ddca_enable_verify(saved_verify_setvcp);
   ddca_enable_report_ddc_errors(saved_report_ddc_errors);
}


void show_any_value(
        DDCA_Display_Handle     dh,
        DDCA_Vcp_Value_Type     value_type,
        DDCA_Vcp_Feature_Code   feature_code)
{
    DDCA_Status ddcrc;
    DDCA_Any_Vcp_Value * valrec;

    ddcrc = ddca_get_any_vcp_value_using_explicit_type(
            dh,
            feature_code,
            value_type,
            &valrec);
    if (ddcrc != 0) {
        DDC_ERRMSG("ddca_get_any_vcp_value_using_explicit_type", ddcrc);
        goto bye;
    }

    if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
       printf("Non-Table value: mh=0x%02x, ml=0x%02x, sh=0x%02x, ml=0x%02x\n",
              valrec->val.c_nc.mh,
              valrec->val.c_nc.ml,
              valrec->val.c_nc.sh,
              valrec->val.c_nc.sl);
       printf("As continuous value (if applicable): max value = %d, cur value = %d\n",
             valrec->val.c_nc.mh << 8 | valrec->val.c_nc.ml,    // or use macro VALREC_MAX_VAL()
             valrec->val.c_nc.sh << 8 | valrec->val.c_nc.sl);   // or use macro VALREC_CUR_VAL()
    }
    else {
       assert(valrec->value_type == DDCA_TABLE_VCP_VALUE);
       printf("Table value: 0x");
       for (int ndx=0; ndx<valrec->val.t.bytect; ndx++)
          printf("%02x", valrec->val.t.bytes[ndx]);
       puts("");
    }

 bye:
    return;
}


DDCA_Status perform_set_non_table_vcp_value(
      DDCA_Display_Handle    dh,
      DDCA_Vcp_Feature_Code  feature_code,
      uint8_t                hi_byte,
      uint8_t                lo_byte)
{
    bool saved_enable_verify = ddca_enable_verify(true);

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(dh, feature_code, hi_byte, lo_byte);
    if (ddcrc == DDCRC_VERIFY) {
        printf("Value verification failed.  Current value is now:\n");
        show_any_value(dh, DDCA_NON_TABLE_VCP_VALUE, feature_code);
     }
     else if (ddcrc != 0) {
        DDC_ERRMSG("ddca_set_non_table_vcp_value", ddcrc);
     }
     else {
        printf("Setting new value succeeded.\n");
     }

     ddca_enable_verify(saved_enable_verify);
     return ddcrc;
}


bool
test_continuous_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   char * feature_name = ddca_get_feature_name(feature_code);
   printf("\nTesting get and set continuous value. dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, feature_name);
   DDCA_Status ddcrc;
   bool ok = false;

   printf("Resetting statistics...\n");
   ddca_reset_stats();

   bool create_default_if_not_found = false;
   DDCA_Feature_Metadata info;
   ddcrc = ddca_get_feature_metadata_by_dh(
           feature_code,
           dh,
           create_default_if_not_found,
           &info);
   if (ddcrc != 0) {
      DDC_ERRMSG("ddca_get_feature_metadata_by_display", ddcrc);
      goto bye;
   }
   if ( !(info.feature_flags & DDCA_CONT) ) {
      printf("Feature 0x%02x is not Continuous\n", feature_code);
      goto bye;
   }

   DDCA_Non_Table_Vcp_Value valrec;
   ddcrc = ddca_get_non_table_vcp_value(
            dh,
            feature_code,
            &valrec);
   if (ddcrc != 0) {
      DDC_ERRMSG("ddca_get_non_table_vcp_value", ddcrc);
      ok = false;
      goto bye;
   }
   uint16_t max_val = valrec.mh << 8 | valrec.ml;
   uint16_t cur_val = valrec.sh << 8 | valrec.sl;

   printf("Feature 0x%02x (%s) current value = %d, max value = %d\n",
          feature_code, feature_name, max_val, cur_val);

   uint16_t old_value = cur_val;
   uint16_t new_value = old_value/2;
   printf("Setting new value %d,,,\n", new_value);
   uint8_t new_sh = new_value >> 8;
   uint8_t new_sl = new_value & 0xff;
   DDCA_Status ddcrc1 = perform_set_non_table_vcp_value(dh, feature_code, new_sh, new_sl);
   if (ddcrc1 != 0 && ddcrc1 != DDCRC_VERIFY)
      goto bye;

   printf("Resetting original value %d...\n", old_value);
   DDCA_Status ddcrc2 = perform_set_non_table_vcp_value(dh, feature_code, old_value>>8, old_value&0xff);
   if (ddcrc2 != 0 && ddcrc2 != DDCRC_VERIFY)
      goto bye;

    if (ddcrc1 == 0 && ddcrc2 == 0)
       ok = true;

bye:

   // Uncomment to see statistics:
   // printf("\nStatistics for one execution of %s()", __func__);
   // ddca_show_stats(DDCA_STATS_ALL, 0);

   // printf("(%s) Done. Returning: %d\n", __func__, ok);
   return ok;
}


bool
show_simple_nc_feature_value_by_vspec(
        DDCA_MCCS_Version_Spec vspec,
        DDCA_Vcp_Feature_Code  feature_code,
        uint8_t                feature_value)
{
    char * feature_value_name = NULL;
    bool ok = false;

    // Uses vspec and feature_code to get the appropriate feature name table,
    // then finds the value in the table.
    printf("Performing value lookup using ddca_get_simple_nc_feature_value_name_by_vspec\n");
    DDCA_Status rc =
    ddca_get_simple_nc_feature_value_name_by_vspec(
          feature_code,
          vspec,          // needed because value lookup mccs version dependent
          &DDCA_UNDEFINED_MONITOR_MODEL_KEY,
          feature_value,
          &feature_value_name);
    if (rc != 0) {
       DDC_ERRMSG("ddca_get_nc_feature_value_name_by_vspec", rc);
       printf("Unable to get interpretation of value 0x%02x\n",  feature_value);
       printf("Current value: 0x%02x\n", feature_value);
       ok = false;
    }
    else {
       printf("Current value: 0x%02x - %s\n", feature_value, feature_value_name);
       ok = true;
    }

    return ok;
}


// This variant assumes the appropriate feature value table has already
// been looked up.
bool
show_simple_nc_feature_value_by_table(
        DDCA_Feature_Value_Table feature_table,
        uint8_t                  feature_value)
{
    char * feature_value_name = NULL;
    bool ok = false;

    printf("Performing value lookup using ddca_get_simple_nc_feature_value_name_by_table\n");
    DDCA_Status rc =
    ddca_get_simple_nc_feature_value_name_by_table(
          feature_table,
          feature_value,
          &feature_value_name);
    if (rc != 0) {
       DDC_ERRMSG("ddca_get_nc_feature_value_name_by_table", rc);
       printf("Unable to get interpretation of value 0x%02x\n",  feature_value);
       printf("Current value: 0x%02x\n", feature_value);
       ok = false;
    }
    else {
       printf("Current value: 0x%02x - %s\n", feature_value, feature_value_name);
       ok = true;
    }

    return ok;
}


bool
test_simple_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code,
      uint8_t                 new_value)
{
    printf("\nTesting get and set of simple NC value: dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, ddca_get_feature_name(feature_code));

    printf("Resetting statistics...\n");
    ddca_reset_stats();
    DDCA_Status ddcrc;
    bool ok = false;

    DDCA_Feature_Metadata info;
    ddcrc = ddca_get_feature_metadata_by_dh(
            feature_code,
            dh,
            false,              // create_default_if_not_found
            &info);
    if (ddcrc != 0) {
       DDC_ERRMSG("ddca_get_feature_metadata_by_display", ddcrc);
       goto bye;
    }
    // Issue: currently synthesized values are Complex-Continuous, synthesized
    //        metadata would fail test if create_default_if_not_found == true
    if ( !(info.feature_flags & DDCA_SIMPLE_NC) ) {
       printf("Feature 0x%02x is not simple NC\n", feature_code);
       goto bye;
    }

    DDCA_Non_Table_Vcp_Value valrec;
    ddcrc = ddca_get_non_table_vcp_value(
               dh,
               feature_code,
               &valrec);
    if (ddcrc != 0) {
       DDC_ERRMSG("ddca_get_non_table_vcp_value", ddcrc);
       goto bye;
    }
    printf("Feature 0x%02x current value = 0x%02x\n",
              feature_code,
              valrec.sl);
    uint8_t old_value = valrec.sl;

    // Pick one or the other. Both work.
    // ok = show_simple_nc_feature_value_by_vspec(info.vspec, feature_code, old_value);
    ok = show_simple_nc_feature_value_by_table(info.sl_values, old_value);
    if (!ok)
       goto bye;

    printf("Setting new value 0x%02x...\n", new_value);
    DDCA_Status ddcrc1 = perform_set_non_table_vcp_value(dh, feature_code, 0, new_value);
    if (ddcrc1 != 0 && ddcrc1 != DDCRC_VERIFY)
       goto bye;

    printf("Resetting original value 0x%02x...\n", old_value);
    DDCA_Status ddcrc2 = perform_set_non_table_vcp_value(dh, feature_code, 0, old_value);
    if (ddcrc2 != 0 && ddcrc2 != DDCRC_VERIFY)
       goto bye;

    if (ddcrc1 == 0 && ddcrc2 == 0)
       ok = true;

bye:
    // uncomment to show statistics:
    // printf("\nStatistics for one execution of %s()", __func__);
    // ddca_show_stats(DDCA_STATS_ALL, 0);

    // printf("(%s) Done. Returning: %s\n", __func__, SBOOL(ok) );
    return ok;
}


// there's no commonly implemented complex NC feature that's writable.  Just read.

bool
test_complex_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   printf("\nTesting query of complex NC value: dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, ddca_get_feature_name(feature_code));

    printf("Resetting statistics...\n");
    ddca_reset_stats();

    DDCA_Status ddcrc;
    bool ok = false;

    DDCA_Feature_Metadata info;
    ddcrc = ddca_get_feature_metadata_by_dh(
           feature_code,
            dh,              // feature info can be MCCS version dependent
            false,           // create_default_if_not_found
            &info);
    if (ddcrc != 0) {
       DDC_ERRMSG("ddca_get_feature_metadata_by_display", ddcrc);
       goto bye;
    }
    assert(info.feature_flags & (DDCA_COMPLEX_NC|DDCA_NC_CONT));

    DDCA_Non_Table_Vcp_Value valrec;
    ddcrc = ddca_get_non_table_vcp_value(
            dh,
            feature_code,
            &valrec);
    if (ddcrc != 0) {
       DDC_ERRMSG("ddca_non_table_vcp_value", ddcrc);
       goto bye;
    }

    printf("Feature 0x%02x current value: mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x\n",
              feature_code,
              valrec.mh,
              valrec.ml,
              valrec.sh,
              valrec.sl);

    char * formatted_value;
#ifdef ALT
    ddcrc = ddca_format_non_table_vcp_value(
                feature_code,
                info.vspec,
                info.mmid,
                &valrec,
                &formatted_value);
#endif
    ddcrc = ddca_format_non_table_vcp_value_by_dref(
                feature_code,
                ddca_display_ref_from_handle(dh),
                &valrec,
                &formatted_value);
    if (ddcrc != 0) {
       DDC_ERRMSG("ddca_format_non_table_vcp_value", ddcrc);
       goto bye;
    }
    printf("Formatted value: %s\n", formatted_value);
    free(formatted_value);

    ok = true;


bye:
    // uncomment to show statistics:
    // printf("\nStatistics for one execution of %s()", __func__);
    // ddca_show_stats(DDCA_STATS_ALL, 0);

    // printf("(%s) Done. Returning: %d\n", __func__, ok);
    return ok;
}


int
main(int argc, char** argv) {
   // printf("\n(%s) Starting. argc = %d\n", __func__, argc);

   int which_test = 0;
   if (argc > 1) {
      which_test = atoi(argv[1]);   // live dangerously, it's test code
   }

   ddca_reset_stats();
   set_standard_settings();

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
      printf("\n===> Test loop for display %d\n", dinfo->dispno);
      // For all the gory details:
      ddca_dbgrpt_display_info(dinfo, /* depth=*/ 1);
      dref = dinfo->dref;

      // printf("Open display reference %s, creating a display handle...\n", ddca_dref_repr(dref));
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         DDC_ERRMSG("ddca_open_display", rc);
         continue;
      }
      printf("Opened display handle: %s\n", ddca_dh_repr(dh));

      if (which_test == 0 || which_test == 1)
         test_continuous_value(dh, 0x10);

      if (which_test == 0 || which_test == 2) {
         // feature 0xcc = OSD language, value 0x03 = French
         test_simple_nc_value(dh, 0xcc, 0x03);
      }

      if (which_test == 0 || which_test == 3)
         test_complex_nc_value(dh, 0xDF);    // VCP version

      rc = ddca_close_display(dh);
      if (rc != 0)
         DDC_ERRMSG("ddca_close_display", rc);
      dh = NULL;
   }

   ddca_free_display_info_list(dlist);

   restore_standard_settings();
   return 0;
}
