/* ddc_swig.c
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <Python.h>   // must be before stdio.h
// #include <fileobject.h>

#include <stdio.h>
#include <string.h>

#include "swig/ddc_swig.h"


//
// Macros
//

#define ERROR_CHECK(impl) \
   do { \
      clear_exception();        \
      DDCT_Status rc = impl;    \
      if (rc != 0)              \
         throw_exception_from_status_code(rc); \
   } while(0);


//
// Convert ddcutil status codes to exceptions
//

static DDCT_Status error_status = 0;
static char error_msg[256];

void clear_exception() {
   error_status = 0;
}

static void throw_exception_from_status_code(DDCT_Status rc) {
   error_status = rc;
   snprintf(error_msg, sizeof(error_msg),
            "%s (%d): %s",
            ddct_status_code_name(rc), rc, ddct_status_code_desc(rc)
           );
}

char * check_exception() {
   char * result = NULL;
   if (error_status)
      result = error_msg;
   return result;
}


//
// General
//

const char * ddcutil_version(void) {
   return ddct_ddcutil_version_string();
}

bool ddcs_built_with_adl(void) {
   return ddct_built_with_adl();
}


// #ifdef FUTURE
void ddc_set_fout( /* PyFileObject */ void *fpy) {
   printf("(%s) fpy = %p\n", __func__, fpy);
   int is_pyfile = PyFile_Check(fpy);
   printf("(%s) is_pyfile=%d\n", __func__, is_pyfile);
}
// #endif

//
// Reports
//

int ddcs_report_active_displays(int depth) {
   clear_exception();
   return ddct_report_active_displays(depth);
}


//
// VCP Feature Information
//

unsigned long ddcs_get_feature_info_by_vcp_version(
               DDCS_VCP_Feature_Code    feature_code,
               DDCS_MCCS_Version_Spec   vspec)
{
   unsigned long result = 0;
   ERROR_CHECK( ddct_get_feature_info_by_vcp_version(feature_code, vspec, &result) );
   return result;
}

char *      ddcs_get_feature_name(DDCS_VCP_Feature_Code feature_code) {
   return ddct_get_feature_name(feature_code);
}


//
// Display Identifiers
//

DDCS_Display_Identifier_p
ddcs_create_dispno_display_identifier(int dispno){
   DDCT_Display_Identifier pdid = NULL;
   DDCT_Status rc = ddct_create_dispno_display_identifier(dispno, &pdid);
   clear_exception();
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return pdid;
}

DDCS_Display_Identifier_p ddcs_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex)
{
   DDCT_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddct_create_adlno_display_identifier(iAdapterIndex, iDisplayIndex, &pdid) );
   return pdid;
}

DDCS_Display_Identifier_p
ddcs_create_busno_display_identifier(int busno)
{
   DDCT_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddct_create_busno_display_identifier(busno, &pdid) );
   return pdid;
}

DDCS_Display_Identifier_p
ddcs_create_model_sn_display_identifier(const char * model, const char * sn)
{
   DDCT_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddct_create_model_sn_display_identifier(model, sn, &pdid) );
   return pdid;
}

DDCS_Display_Identifier_p ddcs_create_edid_display_identifier(const Byte * edid)
{
   DDCT_Display_Identifier pdid = NULL;
   return pdid;
}

DDCS_Display_Identifier_p ddcs_create_usb_display_identifier(int bus,int device)
{
   DDCT_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddct_create_usb_display_identifier(bus, device, &pdid) );
   return pdid;
}

void ddcs_free_display_identifier(DDCS_Display_Identifier_p ddcs_did){
   clear_exception();
   DDCT_Status rc = ddct_free_display_identifier(ddcs_did);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

char * ddcs_repr_display_identifier(DDCS_Display_Identifier_p ddcs_did){
   clear_exception();
   char * result = NULL;
   DDCT_Status  rc = ddct_repr_display_identifier(ddcs_did, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}


//
// Display References
//

DDCS_Display_Ref_p ddcs_get_display_ref(DDCS_Display_Identifier_p did){
   DDCS_Display_Ref_p result = NULL;
   DDCT_Status rc = ddct_get_display_ref(did, &result);
   clear_exception();
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void ddcs_free_display_ref(DDCS_Display_Ref_p dref) {
   clear_exception();
   DDCT_Status rc = ddct_free_display_ref(dref);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

char *  ddcs_repr_display_ref(DDCS_Display_Ref_p dref) {
   clear_exception();
   char * result = NULL;
   DDCT_Status  rc = ddct_repr_display_ref(dref, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void        ddcs_report_display_ref(DDCS_Display_Ref_p dref, int depth) {
   clear_exception();
   ddct_report_display_ref(dref, depth);
}


//
// Display Handles
//

DDCS_Display_Handle_p ddcs_open_display(DDCS_Display_Ref_p dref) {
   DDCS_Display_Handle_p result = NULL;
   DDCT_Status rc = ddct_open_display(dref, &result);
   clear_exception();
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void ddcs_close_display(DDCS_Display_Handle_p dh) {
   clear_exception();
   DDCT_Status rc = ddct_close_display(dh);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

char * ddcs_repr_display_handle(DDCS_Display_Handle_p dh) {
   clear_exception();
   char * result = NULL;
   DDCT_Status  rc = ddct_repr_display_handle(dh, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}


//
// Miscellaneous Monitor Specific Functions
//

DDCS_MCCS_Version_Spec ddcs_get_mccs_version(DDCS_Display_Handle_p dh) {
   DDCS_MCCS_Version_Spec result = {0};
   ERROR_CHECK( ddct_get_mccs_version(dh, &result) );
   return result;
}

// DEPRECATED
unsigned long ddcs_get_feature_info_by_display(
               DDCS_Display_Handle_p dh,
               DDCS_VCP_Feature_Code    feature_code)
{
   unsigned long result = 0;
   ERROR_CHECK( ddct_get_feature_info_by_display(dh, feature_code, &result) );
   return result;
}


//
// Monitor Capabilities
//

char * ddcs_get_capabilities_string(DDCS_Display_Handle_p dh){
   clear_exception();
   char * result = NULL;
   DDCT_Status  rc = ddct_get_capabilities_string(dh, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}


//
// Get and Set VCP Feature Values
//

DDCS_Non_Table_Value_Response ddcs_get_nontable_vcp_value(
               DDCS_Display_Handle_p           dh,
               DDCS_VCP_Feature_Code                feature_code) {

   clear_exception();
   DDCT_Non_Table_Value_Response resp = {0};
   DDCT_Status  rc = ddct_get_nontable_vcp_value(dh, feature_code, &resp);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   DDCS_Non_Table_Value_Response result;
   memcpy(&result, &resp, sizeof(resp));
   return result;
}


void ddcs_set_nontable_vcp_value(
               DDCS_Display_Handle_p  dh,
               VCP_Feature_Code     feature_code,
               int                  new_value)
{
   clear_exception();
   DDCT_Status  rc = ddct_set_continuous_vcp_value(dh, feature_code, new_value);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}


char * ddcs_get_profile_related_values(DDCS_Display_Handle_p dh){
   clear_exception();
   char * result = NULL;
   DDCT_Status  rc = ddct_get_profile_related_values(dh, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void ddcs_set_profile_related_values(char * profile_values_string) {
   clear_exception();
   DDCT_Status  rc = ddct_set_profile_related_values(profile_values_string);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

