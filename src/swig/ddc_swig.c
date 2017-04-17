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

#include "base/core.h"
#include "base/ddc_errno.h"

#include "swig/ddc_swig.h"


//
// Macros
//

#define ERROR_CHECK(impl) \
   do { \
      clear_exception();        \
      DDCA_Status rc = impl;    \
      if (rc != 0)              \
         throw_exception_from_status_code(rc); \
   } while(0);


//
// Convert ddcutil status codes to exceptions
//

static DDCA_Status ddcutil_error_status = 0;
static char error_msg[256];
static PyObject * PyExc_DDCUtilError = NULL;


void clear_exception() {
   ddcutil_error_status = 0;
}

static void throw_exception_from_status_code(DDCA_Status rc) {
   ddcutil_error_status = rc;
   snprintf(error_msg, sizeof(error_msg),
            "%s (%d): %s",
            ddca_status_code_name(rc), rc, ddca_status_code_desc(rc)
           );
}


// Called from %exception handler in ddc_swig.i
char * check_exception() {
   char * result = NULL;
   if (ddcutil_error_status)
      result = error_msg;
   return result;
}


bool check_exception2() {
   bool debug = true;
    bool result = false;
    if (ddcutil_error_status) {
       PyErr_SetString( PyExc_RuntimeError, error_msg);
       // PyErr_SetString( PyExc_DDCUtilError, emsg);   // future
       DBGMSF(debug, "throwing exception\n");
       result = true;
    }
   return result;
}


//
// General
//


void ddcs_init(void) {
   // ddca_init();
   PyExc_DDCUtilError = PyErr_NewException(
                            "ddc_swig.PyExc_DDCUtilError",
                            NULL,                   // PyObject* base
                            NULL);                  // PyObject* dict
   assert(PyExc_DDCUtilError);
}


const char * ddcs_ddcutil_version_string(void) {
   return ddca_ddcutil_version_string();
}

bool ddcs_built_with_adl(void) {
   return ddca_built_with_adl();
}

bool ddcs_built_with_usb(void) {
   return ddca_built_with_usb();
}

FlagsByte ddcs_get_build_options(void) {
   unsigned long feature_bits = ddca_get_build_options();
   return feature_bits;
}



#ifdef NO
void ddcs_set_fout(void * fpy) {
   printf("(%s) fpy = %p\n", __func__, fpy);
   int is_pyfile = PyFile_Check(fpy);
   printf("(%s) is_pyfile=%d\n", __func__, is_pyfile);
   FILE * extracted = PyFile_AsFile((PyObject *)fpy);
   ddca_set_fout(extracted);
}
#endif

#ifndef PYTHON3

void ddcs_set_fout(FILE * f) {
   // DBGMSG("f = %p", f);
   ddca_set_fout(f);
}


static PyFileObject * current_python_fout;

void save_current_python_fout(PyFileObject * pfy) {
   DBGMSG("pfy = %p", pfy);
   current_python_fout = pfy;
}

PyFileObject * get_current_python_fout() {
   return current_python_fout;
}

#endif

//
// Reports
//

int ddcs_report_active_displays(int depth) {
   clear_exception();
   return ddca_report_active_displays(depth);
}


//
// VCP Feature Information
//

#ifdef TO_REIMPLEMENT
Version_Feature_Flags ddcs_get_feature_info_by_vcp_version(
               DDCS_VCP_Feature_Code    feature_code,
               DDCA_MCCS_Version_Id     version_id)
  //             DDCS_MCCS_Version_Spec   vspec)
{
   Version_Feature_Flags result = 0;
   ERROR_CHECK( ddca_get_feature_flags_by_vcp_version(feature_code, version_id, &result) );
   return result;
}
#endif

char *      ddcs_get_feature_name(DDCS_VCP_Feature_Code feature_code) {
   return ddca_get_feature_name(feature_code);
}


//
// Display Identifiers
//

DDCS_Display_Identifier
ddcs_create_dispno_display_identifier(int dispno){
   DDCA_Display_Identifier pdid = NULL;
   DDCA_Status rc = ddca_create_dispno_display_identifier(dispno, &pdid);
   clear_exception();
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return pdid;
}

DDCS_Display_Identifier ddcs_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex)
{
   DDCA_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddca_create_adlno_display_identifier(iAdapterIndex, iDisplayIndex, &pdid) );
   return pdid;
}

DDCS_Display_Identifier
ddcs_create_busno_display_identifier(int busno)
{
   DDCA_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddca_create_busno_display_identifier(busno, &pdid) );
   return pdid;
}

DDCS_Display_Identifier
ddcs_create_mfg_model_sn_display_identifier(const char * mfg_id, const char * model, const char * sn)
{
   DDCA_Display_Identifier pdid = NULL;
   DBGMSG("mfg_id=%s, model=%s, sn=%s", model, sn);
   ERROR_CHECK( ddca_create_mfg_model_sn_display_identifier(mfg_id, model, sn, &pdid) );
   return pdid;
}

DDCS_Display_Identifier ddcs_create_edid_display_identifier(const Byte * edid, int bytect)
{
   DDCA_Display_Identifier pdid = NULL;
   DBGMSG("edid addr = %p, bytect = %d", edid, bytect);
   ERROR_CHECK( ddca_create_edid_display_identifier(edid, &pdid) );
   return pdid;
}

DDCS_Display_Identifier ddcs_create_usb_display_identifier(int bus,int device)
{
   DDCA_Display_Identifier pdid = NULL;
   ERROR_CHECK( ddca_create_usb_display_identifier(bus, device, &pdid) );
   return pdid;
}

void ddcs_free_display_identifier(DDCS_Display_Identifier ddcs_did){
   clear_exception();
   DDCA_Status rc = ddca_free_display_identifier(ddcs_did);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

char * ddcs_repr_display_identifier(DDCS_Display_Identifier ddcs_did){
   clear_exception();
   char * result = ddca_repr_display_identifier(ddcs_did);
   if (!result)
      throw_exception_from_status_code(DDCL_ARG);   // TODO: Python ValueError
   return result;
}


//
// Display References
//

DDCS_Display_Ref ddcs_get_display_ref(DDCS_Display_Identifier did){
   DDCS_Display_Ref result = NULL;
   DDCA_Status rc = ddca_get_display_ref(did, &result);
   clear_exception();
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void ddcs_free_display_ref(DDCS_Display_Ref dref) {
   clear_exception();
   DDCA_Status rc = ddca_free_display_ref(dref);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

char *  ddcs_repr_display_ref(DDCS_Display_Ref dref) {
   clear_exception();
   char * result =  ddca_repr_display_ref(dref);
   if (!result)
      throw_exception_from_status_code(DDCL_ARG);     // TODO: Python ValueError
   return result;
}

void        ddcs_report_display_ref(DDCS_Display_Ref dref, int depth) {
   clear_exception();
   ddca_report_display_ref(dref, depth);
}


//
// Display Handles
//

DDCS_Display_Handle ddcs_open_display(DDCS_Display_Ref dref) {
   DDCS_Display_Handle result = NULL;
   DDCA_Status rc = ddca_open_display(dref, &result);
   clear_exception();
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void ddcs_close_display(DDCS_Display_Handle dh) {
   clear_exception();
   DDCA_Status rc = ddca_close_display(dh);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

char * ddcs_repr_display_handle(DDCS_Display_Handle dh) {
   clear_exception();
   char * result = ddca_repr_display_handle(dh);
   if (!result)
      throw_exception_from_status_code(DDCL_OTHER);   // should just be Python ValueError
   return result;
}


//
// Miscellaneous Monitor Specific Functions
//

DDCS_MCCS_Version_Spec ddcs_get_mccs_version(DDCS_Display_Handle dh) {
   DDCS_MCCS_Version_Spec result = {0};
   ERROR_CHECK( ddca_get_mccs_version(dh, &result) );
   return result;
}

#ifdef OLD
// DEPRECATED
unsigned long ddcs_get_feature_info_by_display(
               DDCS_Display_Handle dh,
               DDCS_VCP_Feature_Code    feature_code)
{
   DDCA_Version_Feature_Flags result = 0;
   ERROR_CHECK( ddca_get_feature_info_by_display(dh, feature_code, &result) );
   return result;
}
#endif


//
// Monitor Capabilities
//

char * ddcs_get_capabilities_string(DDCS_Display_Handle dh){
   clear_exception();
   char * result = NULL;
   DDCA_Status  rc = ddca_get_capabilities_string(dh, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}


//
// Get and Set VCP Feature Values
//

// n. returning entire value, not pointer to value
DDCS_Non_Table_Value_Response ddcs_get_nontable_vcp_value(
               DDCS_Display_Handle    dh,
               DDCS_VCP_Feature_Code    feature_code) {

   clear_exception();
   DDCA_Non_Table_Value_Response resp = {0};
   DDCA_Status  rc = ddca_get_nontable_vcp_value(dh, feature_code, &resp);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   DDCS_Non_Table_Value_Response result;
   // memcpy(&result, &resp, sizeof(resp));
   // How best to handle union in swig?
   result.mh = resp.nc.mh;
   result.ml = resp.nc.ml;
   result.sh = resp.nc.sh;
   result.sl = resp.nc.sl;
   result.cur_value = resp.c.cur_val;
   result.max_value = resp.c.max_val;

   return result;
}


void ddcs_set_nontable_vcp_value(
               DDCS_Display_Handle  dh,
               DDCA_Vcp_Feature_Code     feature_code,
               int                  new_value)
{
   clear_exception();
   DDCA_Status  rc = ddca_set_continuous_vcp_value(dh, feature_code, new_value);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}


char * ddcs_get_profile_related_values(DDCS_Display_Handle dh){
   clear_exception();
   char * result = NULL;
   DDCA_Status  rc = ddca_get_profile_related_values(dh, &result);
   if (rc != 0)
      throw_exception_from_status_code(rc);
   return result;
}

void ddcs_set_profile_related_values(char * profile_values_string) {
   clear_exception();
   DDCA_Status  rc = ddca_set_profile_related_values(profile_values_string);
   if (rc != 0)
      throw_exception_from_status_code(rc);
}

