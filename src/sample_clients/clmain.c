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
          ddca_status_code_name(status_code),      \
          ddca_status_code_desc(status_code))


#ifdef REF
// Exactly 1 of DDCA_RO, DDCA_WO, DDCA_RW is set
#define DDCA_RO           0x0400               /**< Read only feature */
#define DDCA_WO           0x0200               /**< Write only feature */
#define DDCA_RW           0x0100               /**< Feature is both readable and writable */
#define DDCA_READABLE     (DDCA_RO | DDCA_RW)  /**< Feature is either RW or RO */
#define DDCA_WRITABLE     (DDCA_WO | DDCA_RW)  /**< Feature is either RW or WO */

// Further refine the C/NC/TABLE categorization of the MCCS spec
// Exactly 1 of the following 7 bits is set
#define DDCA_STD_CONT       0x80       /**< Normal continuous feature */
#define DDCA_COMPLEX_CONT   0x40       /**< Continuous feature with special interpretation */
#define DDCA_SIMPLE_NC      0x20       /**< Non-continuous feature, having a defined list of values in byte SL */
#define DDCA_COMPLEX_NC     0x10       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define DDCA_WO_NC          0x08       // TODO: CHECK USAGE
#define DDCA_NORMAL_TABLE 0x04       /**< Normal table type feature */
#define DDCA_WO_TABLE       0x02       /**< Write only table feature */

#define DDCA_CONT           (DDCA_STD_CONT|DDCA_COMPLEX_CONT)            /**< Continuous feature, of any subtype */
#define DDCA_NC             (DDCA_SIMPLE_NC|DDCA_COMPLEX_NC|DDCA_WO_NC)  /**< Non-continuous feature of any subtype */
#define DDCA_NON_TABLE      (DDCA_CONT | DDCA_NC)                        /**< Non-table feature of any type */

#define DDCA_TABLE          (DDCA_NORMAL_TABLE | DDCA_WO_TABLE)        /**< Table type feature, of any subtype */
#define DDCA_KNOWN          (DDCA_CONT | DDCA_NC | DDCA_TABLE)           // TODO: Usage??? Check

// Additional bits:
#define DDCA_DEPRECATED     0x01     /**< Feature is deprecated in the specified VCP version */

#endif


/* Create a string representation of Version_Feature_Flags bitfield.
 * The representation is returned in a buffer provided.
 *
 * Arguments:
 *    data       flags
 *    buffer     where to save formatted response
 *    bufsz      buffer size
 *
 * Returns:      buffer
 */
char * interpret_version_feature_flags_r(DDCA_Version_Feature_Flags flags, char * buffer, int bufsz) {
   assert(buffer);
   // assert(buffer && bufsz > 150);
   // printf("(%s) bufsz=%d\n", __func__, bufsz);

   snprintf(buffer, bufsz, "%s%s%s%s%s%s%s%s%s%s%s",
       flags & DDCA_RO             ? "Read-Only, "                   : "",
       flags & DDCA_WO             ? "Write-Only, "                  : "",
       flags & DDCA_RW             ? "Read-Write, "                  : "",
       flags & DDCA_STD_CONT       ? "Continuous (standard), "       : "",
       flags & DDCA_COMPLEX_CONT   ? "Continuous (complex), "        : "",
       flags & DDCA_SIMPLE_NC      ? "Non-Continuous (simple), "     : "",
       flags & DDCA_COMPLEX_NC     ? "Non-Continuous (complex), "    : "",
       flags & DDCA_WO_NC          ? "Non-Continuous (write-only), " : "",
       flags & DDCA_NORMAL_TABLE ? "Table (readable), "            : "",
       flags & DDCA_WO_TABLE       ? "Table (write-only), "          : "",
       flags & DDCA_DEPRECATED     ? "Deprecated, "                  : ""
       );
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   // printf("(%s) returning |%s|\n", __func__, buffer);
   return buffer;
}


char * interpret_global_feature_flags_r(uint8_t flags, char * buffer, int bufsz) {
   assert(buffer);
   // assert(buffer && bufsz > 150);
   // printf("(%s) bufsz=%d\n", __func__, bufsz);

   snprintf(buffer, bufsz, "%s",
       flags & DDCA_SYNTHETIC             ? "Dummy Info, "                   : ""
       );
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   // printf("(%s) returning |%s|\n", __func__, buffer);
   return buffer;
}




void my_report_version_feature_info(DDCA_Version_Feature_Info * info) {
   printf("\nVersion Sensitive Feature Information for VCP Feature: 0x%02x - %s\n",
           info->feature_code, info->feature_name);
   printf("   VCP version:          %d.%d\n",   info->vspec.major, info->vspec.minor);
   printf("   VCP version id:       %d (%s) - %s \n",
                  info->version_id,
                  ddca_mccs_version_id_name(info->version_id),
                  ddca_mccs_version_id_desc(info->version_id)
             );
   printf("   Description:          %s\n",  info->desc);
   // printf("info->sl_values = %p\n", info->sl_values);
#define WORKBUF_SZ 100
   char workbuf[WORKBUF_SZ];
   printf("   Version insensitive flags: %s\n",
          interpret_global_feature_flags_r(info->global_flags, workbuf, WORKBUF_SZ));
   printf("   Version sensitive flags: %s\n",
          interpret_version_feature_flags_r(info->feature_flags, workbuf, WORKBUF_SZ));
#undef WORKBUF_SZ
   if (info->sl_values) {
      printf("   SL values: \n");
      DDCA_Feature_Value_Entry * cur_entry = info->sl_values;
      while (cur_entry->value_name) {
         printf("      0x%02x - %s\n", cur_entry->value_code,  cur_entry->value_name);
         cur_entry++;
         // printf("cur_entry=%p\n", cur_entry);
      }
   }


}



void test_get_single_feature_info(DDCA_MCCS_Version_Id version_id, DDCA_Vcp_Feature_Code feature_code) {
   printf("\n(%s) Getting metadata for feature 0x%02x, mccs version = %s\n", __func__,
          feature_code, ddca_mccs_version_id_desc(version_id));
   printf("Feature name: %s\n", ddca_get_feature_name(feature_code));
   // DDCA_Version_Feature_Flags feature_flags;
   DDCA_Version_Feature_Info * info;
  DDCA_Status rc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, &info);
  if (rc != 0)
     FUNCTION_ERRMSG("ddct_get_feature_info", rc);
  else {
     // TODO: Version_Specific_Feature_Info needs a report function
    //  report_ddca_version_feature_flags(feature_code, info->feature_flags);
     // report_version_feature_info(info, 1);
     my_report_version_feature_info(info);
  }
  printf("%s) Done.\n", __func__);
}


void test_get_feature_info(DDCA_MCCS_Version_Id version_id) {
   printf("\n(%s) ===> Starting.  version_id = %s\n", __func__, ddca_mccs_version_id_name(version_id));

   DDCA_Vcp_Feature_Code feature_codes[] = {0x02, 0x03, 0x10, 0x43, 0x60};
   int feature_code_ct = sizeof(feature_codes)/sizeof(DDCA_Vcp_Feature_Code);
   int ndx = 0;
   for (; ndx < feature_code_ct; ndx++)
      test_get_single_feature_info(version_id, feature_codes[ndx]);

   printf("%s) Done.\n", __func__);
}


int
test_cont_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   printf("\n(%s) ===> Starting. dh=%s, feature_code=0x%02x\n",
          __func__, ddca_repr_display_handle(dh), feature_code);
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


void test_get_capabilities(DDCA_Display_Handle dh) {
   printf("\n(%s) ===> Starting.  dh = %s\n", __func__, ddca_repr_display_handle(dh));
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
   printf("(%s) Done.\n", __func__);
}



DDCA_Status test_get_set_profile_related_values(DDCA_Display_Handle dh) {
   printf("\n(%s) ===> Starting.  dh = %s\n", __func__, ddca_repr_display_handle(dh));
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



#ifdef WRONG
void my_abort_func(Public_Status_Code psc) {
   fprintf(stderr, "(%s) Aborting. Internal status code = %d\n", __func__, psc);
   printf("(%s)!!! returning instead\n", __func__);
   // exit(EXIT_FAILURE);
}
#endif


void test_build_information() {
   printf("\n(%s) ===> Starting. \n", __func__);
   printf("Probe static build information...\n");
   // Get the ddcutil version as a string in the form "major.minor.micro".
   printf("ddcutil version by ddca_ddcutil_version_string(): %s\n", ddca_ddcutil_version_string() );

   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   printf("ddcutil version by ddca_ddcutil_version():  %d.%d.%d\n", vspec.major, vspec.minor, vspec.micro);


   // old way
   // printf("(%s) Built with ADL support: %s\n", __func__, (ddca_built_with_adl()) ? "yes" : "no");
   // printf("(%s) Built with USB support: %s\n", __func__, (ddca_built_with_usb()) ? "yes" : "no");

   uint8_t build_options = ddca_get_build_options();
   printf("(%s) Built with ADL support: %s\n", __func__, (build_options & DDCA_BUILT_WITH_ADL) ? "yes" : "no");
   printf("(%s) Built with USB support: %s\n", __func__, (build_options & DDCA_BUILT_WITH_USB) ? "yes" : "no");
}


void test_retry_management() {

  printf("\n(%s) ===> Starting. Exercise retry management functions...\n", __func__);

  int rc = 0;

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
}

int test_monitor_detection() {
   printf("\n(%s) ===> Starting.\n", __func__);
   printf("Check for monitors using ddca_get_displays()...\n");
   // Inquire about detected monitors.
   DDCA_Display_Info_List * dlist = ddca_get_displays();
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


int main(int argc, char** argv) {
   printf("\n(%s) Starting.\n", __func__);

   ddca_reset_stats();

   DDCA_Status rc;
   DDCA_Display_Identifier did;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning

   // Initialize libddcutil.   Must be called first
   // printf("(%s) Calling ddca_init()...\n", __func__);
   // ddca_init();

#ifdef WRONG
   // Register an abort function.
   // If libddcutil encounters an unexpected, unrecoverable error, it will
   // normally exit, causing the calling program to fail.  If the caller registers an
   // abort function, that function will be called instead.
   ddca_register_abort_func(my_abort_func);
#endif

   // For aborting out of shared library
   jmp_buf abort_buf;
   int jmprc = setjmp(abort_buf);
   if (jmprc) {
      DDCA_Global_Failure_Information * finfo = ddca_get_global_failure_information();
      if (finfo)
         fprintf(stderr, "(%s) Error %d (%s) in function %s at line %d in file %s\n",
                         __func__, finfo->status, ddca_status_code_name(finfo->status), finfo->funcname, finfo->lineno, finfo->fn);
      fprintf(stderr, "(%s) Aborting. Internal status code = %d\n", __func__, jmprc);
      exit(EXIT_FAILURE);
   }
   ddca_register_jmp_buf(&abort_buf);

   // Query library build settings.
   test_build_information();

   // Retry management
   // test_retry_management();


   // Monitor detection
   /* int displayct = */ test_monitor_detection();
   // goto bye;  // *** TEMP ***

   test_display_id_ref_handle_use();



   DDCA_Display_Info_List * dlist = ddca_get_displays();
   printf("ddca_get_displays() returned %p\n", dlist);


   for (int dispno = 1; dispno <= dlist->ct; dispno++) {
      printf("\n(%s) ===> Test loop for display %d\n", __func__, dispno);

      DDCA_Display_Info * dinfo = &dlist->info[dispno-1];
      ddca_report_display_info(dinfo, 1);

      did = NULL;
#ifdef ALT
      printf("Create a Display Identifier for display %d...\n", dispno);
      rc = ddca_create_dispno_display_identifier(dispno, &did);

      printf("Create a display reference from the display identifier...\n");
      rc = ddca_get_display_ref(did, &dref);
      assert(rc == 0);
#endif

      dref = dlist->info[dispno-1].ddca_dref;    // *** ugh ***

      printf("Open the display reference, creating a display handle...\n");
      rc = ddca_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddca_open_display", rc);
         continue;
      }
      char * dh_repr = ddca_repr_display_handle(dh);
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
   if (dref) {
      rc = ddca_free_display_ref(dref);
      printf("(%s) ddca_free_display_ref() returned %d\n", __func__, rc);
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
