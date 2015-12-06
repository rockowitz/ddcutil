/*
 * clmain.c
 *
 *  Created on: Dec 4, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdio.h>

#include "libmain/ddct_public.h"

#define FUNCTION_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddct_status_code_name(status_code),      \
          ddct_status_code_desc(status_code))


bool test_cont_value(DDCT_Display_Handle dh, Byte feature_code) {

   int rc;
   bool ok;
   char * feature_name = ddct_get_feature_name(feature_code);
   DDCT_Non_Table_Value_Response non_table_response;
   rc = ddct_get_nontable_vcp_value(dh, feature_code, &non_table_response);

   if (rc != 0)
      FUNCTION_ERRMSG("ddct_set_continuous_vcp_value", rc);
   else {
      rc = 0;
      printf("(%s) Feature 0x%02x (%s) current value = %d, max value = %d\n",
             __func__, feature_code, feature_name,
             non_table_response.cur_value,
             non_table_response.max_value);
      int cur_value = non_table_response.cur_value;
      int new_value = cur_value/2;
      rc = ddct_set_nontable_vcp_value(dh, feature_code, new_value);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddct_set_continuous_vcp_value", rc);
      }
      else {
         printf("(%s) Setting new value succeeded.\n", __func__);
         rc = ddct_get_nontable_vcp_value(dh, feature_code, &non_table_response);
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
         rc = ddct_set_nontable_vcp_value(dh, feature_code, cur_value);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddct_set_continuous_vcp_value", rc);
         }
         else {
            printf("(%s) Resetting original value succeeded.\n", __func__);
            rc = ddct_get_nontable_vcp_value(dh, feature_code, &non_table_response);
            if (rc != 0) {
               FUNCTION_ERRMSG("ddct_get_continuous_vcp_value", rc);
            }
            else {
               rc = ddct_get_nontable_vcp_value(dh, feature_code, &non_table_response);
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
   return (rc == 0 && ok);
}

bool test_get_capabilities_string(DDCT_Display_Handle dh) {
   char * capabilities = NULL;
   DDCT_Status rc =  ddct_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      FUNCTION_ERRMSG("ddct_get_capabilities_string", rc);
   else
      printf("(%s) Capabilities: %s\n", __func__, capabilities);
   printf("(%s) Second call should be fast\n", __func__);
   rc =  ddct_get_capabilities_string(dh, &capabilities);
   if (rc != 0)
      FUNCTION_ERRMSG("ddct_get_capabilities_string", rc);
   else
      printf("(%s) Capabilities: %s\n", __func__, capabilities);
   return false;
}



int main(int argc, char** argv) {
   printf("(%s) Starting.\n", __func__);

   DDCT_Status rc;
   DDCT_Display_Identifier did;
   DDCT_Display_Ref dref;
   DDCT_Display_Handle dh;


   ddct_init();

   printf("(%s) Built with ADL support: %s\n", __func__, (ddct_built_with_adl()) ? "yes" : "no");

   rc = ddct_set_max_tries(DDCT_WRITE_READ_TRIES, 16);
   printf("(%s) ddct_set_max_tries(..,16) returned: %d\n", __func__, rc);
   rc = ddct_set_max_tries(DDCT_WRITE_READ_TRIES, 15);
   if (rc != 0)
      FUNCTION_ERRMSG("DDCT_WRITE_READ_TRIES:ddct_set_max_tries", rc);
   rc = ddct_set_max_tries(DDCT_MULTI_PART_TRIES, 15);
   if (rc != 0)
      FUNCTION_ERRMSG("DDCT_MULTI_PART_TRIES:ddct_set_max_tries", rc);


   printf("(%s) max write only tries: %d\n", __func__, ddct_get_max_tries(DDCT_WRITE_ONLY_TRIES));
   printf("(%s) max write read tries: %d\n", __func__, ddct_get_max_tries(DDCT_WRITE_READ_TRIES));
   printf("(%s) max multi part tries: %d\n", __func__, ddct_get_max_tries(DDCT_MULTI_PART_TRIES));


   rc = ddct_create_dispno_display_identifier(2, &did);
   assert(rc == 0);

   char * did_repr = NULL;
   rc = ddct_repr_display_identifier(did, &did_repr);
   assert(rc == 0);
   printf("(%s) did=%s\n", __func__, did_repr);

   rc = ddct_get_display_ref(did, &dref);

   if (rc != 0) {
      printf("(%s) ddct_get_display_ref() returned %d (%s): %s\n",
             __func__, rc, ddct_status_code_name(rc), ddct_status_code_desc(rc));
   }
   else {
      char * dref_repr;
      rc = ddct_repr_display_ref(dref, &dref_repr);
      assert(rc == 0);
      printf("(%s) dref=%s\n", __func__, dref_repr);

      rc = ddct_open_display(dref, &dh);
      if (rc != 0) {
         FUNCTION_ERRMSG("ddct_open_display", rc);
      }
      else {
         char * dh_repr;
         rc = ddct_repr_display_handle(dh, &dh_repr);
         printf("(%s) display handle: %s\n", __func__, dh_repr);

         DDCT_MCCS_Version_Spec vspec;
         rc = ddct_get_mccs_version(dh, &vspec);
         if (rc != 0) {
            FUNCTION_ERRMSG("ddct_get_mccs_version_spec", rc);
         }
         else {
            printf("(%s) VCP version: %d.%d\n", __func__, vspec.major, vspec.minor);
         }

         test_cont_value(dh, 0x10);
         test_get_capabilities_string(dh);

      }
   }

   if (dh) {
      rc = ddct_close_display(dh);
      if (rc != 0)
         FUNCTION_ERRMSG("ddct_close_display", rc);
   }
   if (dref) {
      rc = ddct_free_display_ref(dref);
      printf("(%s) ddct_free_display_ref() returned %d\n", __func__, rc);
   }
   if (did) {
      rc = ddct_free_display_identifier(did);
      printf("(%s) ddct_free_display_identifier() returned %d\n", __func__, rc);
   }
}
