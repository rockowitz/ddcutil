/* ddct_public.c
 *
 * Created on: Dec 1, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/ddc_errno.h"
#include "base/displays.h"
#include "base/ddc_packets.h"
#include "base/parms.h"
#include "base/msg_control.h"

#include "adl/adl_shim.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"
#include "ddc/vcp_feature_codes.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_edid.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_services.h"

#include "libmain/ddct_public.h"

#include "../app_ddctool/loadvcp.h"
#include "../ddc/ddc_output.h"

#define WITH_DR(ddct_dref, action) \
   do { \
      if (!library_initialized) \
         return DDCL_UNINITIALIZED; \
      DDCT_Status rc = 0; \
      Display_Ref * dref = (Display_Ref *) ddct_dref; \
      if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  { \
         rc = DDCL_ARG; \
      } \
      else { \
         (action); \
      } \
      return rc; \
   } while(0);


#define WITH_DH(_ddct_dh_, _action_) \
   do { \
      if (!library_initialized) \
         return DDCL_UNINITIALIZED; \
      DDCT_Status rc = 0; \
      Display_Handle * dh = (Display_Handle *) _ddct_dh_; \
      if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  { \
         rc = DDCL_ARG; \
      } \
      else { \
         (_action_); \
      } \
      return rc; \
   } while(0);



static bool library_initialized = false;

void ddct_init() {
   DBGMSG("Starting. library_initialized=%s", bool_repr(library_initialized));
   if (!library_initialized) {
      DBGMSG("Calling init_ddc_services...");
      init_ddc_services();
      library_initialized = true;
   }
}

bool ddct_built_with_adl() {
   return adlshim_is_available();
}


char * ddct_status_code_name(DDCT_Status status_code) {
   char * result = NULL;
   Status_Code_Info * code_info = find_global_status_code_info(status_code);
   if (code_info)
      result = code_info->name;
   return result;
}

char * ddct_status_code_desc(DDCT_Status status_code) {
   char * result = "unknown status code";
   Status_Code_Info * code_info = find_global_status_code_info(status_code);
   if (code_info)
   result = code_info->description;
   return result;
}

// typedef enum{DDCT_WRITE_ONLY_TRIES, DDCT_WRITE_READ_TRIES, DDCT_MULTIPART_TRIES} DDCT_Retry_Type;

int  ddct_get_max_tries(DDCT_Retry_Type retry_type) {
   int result = 0;
   switch(retry_type) {
      case (DDCT_WRITE_ONLY_TRIES):
         result = ddc_get_max_write_only_exchange_tries();
      break;
   case (DDCT_WRITE_READ_TRIES):
      result = ddc_get_max_write_read_exchange_tries();
      break;
   case (DDCT_MULTI_PART_TRIES):
      result = ddc_get_max_multi_part_read_tries();
      break;
   }
   return result;
}


DDCT_Status ddct_set_max_tries(DDCT_Retry_Type retry_type, int max_tries) {
   DDCT_Status rc = 0;
   if (max_tries > MAX_MAX_TRIES)
      rc = DDCL_ARG;
   else {
      switch(retry_type) {
      case (DDCT_WRITE_ONLY_TRIES):
         ddc_set_max_write_only_exchange_tries(max_tries);
         break;
      case (DDCT_WRITE_READ_TRIES):
         ddc_set_max_write_read_exchange_tries(max_tries);
         break;
      case (DDCT_MULTI_PART_TRIES):
         ddc_set_max_multi_part_read_tries(max_tries);
         break;
      }
   }
   return rc;
}



DDCT_Status ddct_create_dispno_display_identifier(int dispno, DDCT_Display_Identifier* pdid) {
   Display_Identifier* did = create_dispno_display_identifier(dispno);
   *pdid = did;
   return 0;
}

DDCT_Status ddct_create_busno_display_identifier(
               int busno,
               DDCT_Display_Identifier* pdid) {
   Display_Identifier* did = create_busno_display_identifier(busno);
   *pdid = did;
   return 0;
}

DDCT_Status ddct_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex,
               DDCT_Display_Identifier* pdid) {
   Display_Identifier* did = create_adlno_display_identifier(iAdapterIndex, iDisplayIndex);
   *pdid = did;
   return 0;
}

DDCT_Status ddct_create_mon_ser_display_identifier(
      char* model_name,
      char* serial_ascii,
      DDCT_Display_Identifier* pdid
     )
{
   *pdid = NULL;
   DDCT_Status rc = 0;
   if (model_name == NULL  ||
       strlen(model_name) >= EDID_MODEL_NAME_FIELD_SIZE ||
       serial_ascii == NULL ||
       strlen(serial_ascii) >= EDID_SERIAL_ASCII_FIELD_SIZE
      )
   {
      rc = DDCL_ARG;
      *pdid = NULL;
   }
   else {
      *pdid = create_mon_ser_display_identifier(model_name, serial_ascii);
   }
   return rc;
}

DDCT_Status ddct_create_edid_display_identifier(
               Byte * edid,
               DDCT_Display_Identifier * pdid)    // 128 byte EDID
{
   *pdid = NULL;
   DDCT_Status rc = 0;
   if (edid == NULL) {
      rc = DDCL_ARG;
      *pdid = NULL;
   }
   else {
      *pdid = create_edid_display_identifier(edid);
   }
   return rc;
}

DDCT_Status ddct_free_display_identifier(DDCT_Display_Identifier did) {
   DDCT_Status rc = 0;
   Display_Identifier * pdid = (Display_Identifier *) did;
   if (pdid == NULL || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
     rc = DDCL_ARG;
   }
   else {
     free_display_identifier(pdid);
   }
   return rc;
}


static char did_work_buf[100];

DDCT_Status ddct_repr_display_identifier(DDCT_Display_Identifier ddct_did, char **repr) {
   DDCT_Status rc = 0;
   Display_Identifier * pdid = (Display_Identifier *) ddct_did;
   if (pdid == NULL || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
     rc = DDCL_ARG;
     *repr = "invalid display identifier";
   }
   else {
      char * did_type_name = display_id_type_name(pdid->id_type);
      switch (pdid->id_type) {
      case(DISP_ID_BUSNO):
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, bus=/dev/i2c-%d", did_type_name, pdid->busno);
            break;
      case(DISP_ID_ADL):
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, adlno=%d.%d", did_type_name, pdid->iAdapterIndex, pdid->iDisplayIndex);
            break;
      case(DISP_ID_MONSER):
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, model=%s, sn=%s", did_type_name, pdid->model_name, pdid->serial_ascii);
            break;
      case(DISP_ID_EDID):
      {
            char * hs = hexstring(pdid->edidbytes, 128);
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, edid=%8s...%8s", did_type_name, hs, hs+248);
            break;
      }
      case(DISP_ID_DISPNO):
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, dispno=%d", did_type_name, pdid->dispno);

      } // switch
      *repr = did_work_buf;
   }
   return rc;
}


DDCT_Status ddct_get_display_ref(DDCT_Display_Identifier did, DDCT_Display_Ref* ddct_dref) {
   bool debug = false;
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Identifier * pdid = (Display_Identifier *) did;
   if (pdid == NULL || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
     rc = DDCL_ARG;
   }
   else {
      Display_Ref* dref = get_display_ref_for_display_identifier(pdid, true /* emit_error_msg */);
      if (debug)
         DBGMSG("get_display_ref_for_display_identifier() returned %p", dref);
      if (dref)
         *ddct_dref = dref;
      else
         rc = DDCL_ARG;
   }
   return rc;
}


DDCT_Status ddct_free_display_ref(DDCT_Display_Ref ddct_dref) {
   WITH_DR(ddct_dref,
         {
         free_display_ref(dref);
         }
   );
}


// static char dref_work_buf[100];

DDCT_Status ddct_repr_display_ref(DDCT_Display_Ref ddct_dref, char** repr){
   DDCT_Status rc = 0;
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
      *repr = "invalid display reference";
   }
   else {
#ifdef TOO_MUCH_WORK
      char * dref_type_name = ddc_io_mode_name(dref->ddc_io_mode);
      switch (dref->ddc_io_mode) {
      case(DISP_ID_BUSNO):
         snprintf(dref_work_buf, 100,
                  "Display Ref Type: %s, bus=/dev/i2c-%d", dref_type_name, dref->busno);
         break;
      case(DISP_ID_ADL):
         snprintf(dref_work_buf, 100,
                  "Display Ref Type: %s, adlno=%d.%d", dref_type_name, dref->iAdapterIndex, dref->iDisplayIndex);
         break;
      }
      *repr = did_work_buf;
#endif
      *repr = display_ref_short_name(dref);
   }
   return rc;
}

void        ddct_report_display_ref(DDCT_Display_Ref ddct_dref, int depth) {
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   rpt_vstring(depth, "DDCT_Display_Ref at %p:", dref);
   report_display_ref(dref, depth+1);
}


DDCT_Status ddct_open_display(DDCT_Display_Ref ddct_dref, DDCT_Display_Handle * pdh) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
   }
   else {
     // TODO: fix ddc_open_display for RETURN_ERROR_IF_FAILURE
     Display_Handle* dh = ddc_open_display(dref,  RETURN_ERROR_IF_FAILURE);
     if (dh)
        *pdh = dh;
     else
        rc = DDCL_ARG;     //  TEMP, need a proper status code
   }
   return rc;
}

DDCT_Status ddct_close_display(DDCT_Display_Handle ddct_dh) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddct_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
   }
   else {
      // TODO: ddc_close_display() needs an action if failure parm,
      // should return status code
      ddc_close_display(dh);
      rc = 0;    // is this what to do?
   }
   return rc;
}


static char dh_work_buf[100];

DDCT_Status ddct_repr_display_handle(DDCT_Display_Handle ddct_dh, char ** repr) {
   DDCT_Status rc = 0;
   Display_Ref * dh = (Display_Ref *) ddct_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
      *repr = "invalid display handle";
   }
   else {
      char * dh_type_name = ddc_io_mode_name(dh->ddc_io_mode);
      switch (dh->ddc_io_mode) {
      case(DISP_ID_BUSNO):
         snprintf(dh_work_buf, 100,
                  "Display Handle Type: %s, bus=/dev/i2c-%d", dh_type_name, dh->busno);
         break;
      case(DISP_ID_ADL):
         snprintf(dh_work_buf, 100,
                  "Display Handle Type: %s, adlno=%d.%d", dh_type_name, dh->iAdapterIndex, dh->iDisplayIndex);
         break;
      }
      *repr = dh_work_buf;
   }
   // DBGMSG("repr=%p, *repr=%p, dh_work_buf=%p", repr, *repr, dh_work_buf);
   // DBGMSG("dh_work_buf=|%s|", dh_work_buf);
   // DBGMSG("Returning rc=%d, *repr=%s", rc, *repr);
   return rc;
}


DDCT_Status ddct_get_mccs_version(DDCT_Display_Handle ddct_dh, DDCT_MCCS_Version_Spec* pspec) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddct_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
      pspec->major = 0;
      pspec->minor = 0;
   }
   else {
      // no: need to call function, may not yet be set
      Version_Spec vspec = get_vcp_version_by_display_handle(dh);
      pspec->major = vspec.major;
      pspec->minor = vspec.minor;
      rc = 0;
   }
   return rc;
}


DDCT_Status ddct_get_edid_by_display_ref(DDCT_Display_Ref ddct_dref, Byte** pbytes) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
   }
   else {
      Parsed_Edid*  edid = ddc_get_parsed_edid_by_display_ref(dref);
      *pbytes = edid->bytes;
   }
   return rc;

}


// or return a struct?
DDCT_Status ddct_get_feature_info(
      DDCT_Display_Handle ddct_dh,    // needed because in rare cases feature info is MCCS version dependent
      VCP_Feature_Code feature_code,
      unsigned long * flags)
{
   WITH_DH(
      ddct_dh,
      {
         VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
         if (!pentry)
            *flags = 0;
         else {
            Version_Spec vspec = get_vcp_version_by_display_handle(dh);
            Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec);
            *flags = 0;
            // TODO handle subvariants REWORK
            if (vflags & VCP2_RO)
               *flags |= DDCT_RO;
            if (vflags & VCP2_WO)
               *flags |= DDCT_WO;
            if (vflags & VCP2_RW)
               *flags |= DDCT_RW;
            if (vflags & VCP2_CONT)
               *flags |= DDCT_CONTINUOUS;
#ifdef OLD
            if (pentry->flags & VCP_TYPE_V2NC_V3T) {
               if (vspec.major < 3)
                  *flags |= DDCT_SIMPLE_NC;
               else
                  *flags |= DDCT_TABLE;
            }
#endif
            else if (vflags & VCP2_TABLE)
               *flags |= DDCT_TABLE;
            else if (vflags & VCP2_NC) {
               if (vspec.major < 3)
                  *flags |= DDCT_SIMPLE_NC;
               else {
                  // TODO: In V3, some features use combination of high and low bytes
                  // for now, mark all as simple
                  *flags |= DDCT_SIMPLE_NC;
                  // alt: DDCT_COMPLEX_NC
               }
            }
         }
      }
   );
}



// static char  default_feature_name_buffer[40];
char * ddct_get_feature_name(VCP_Feature_Code feature_code) {
   // do we want get_feature_name()'s handling of mfg specific and unrecognized codes?
   char * result = get_feature_name_by_id_only(feature_code);
   // snprintf(default_feature_name_buffer, sizeof(default_feature_name_buffer), "VCP Feature 0x%02x", feature_code);
   // return default_feature_name_buffer;
   return result;
}

typedef void * Feature_Value_Table;   // temp

DDCT_Status ddct_get_feature_sl_value_table(
               DDCT_Display_Handle   ddct_dh,
               VCP_Feature_Code      feature_code,
               Feature_Value_Table * value_table)
{
   return DDCL_UNIMPLEMENTED;
}

// or:
DDCT_Status ddct_get_nc_feature_value_name(
               DDCT_Display_Handle ddct_dh,    // needed because value lookup mccs version dependent
               VCP_Feature_Code    feature_code,
               Byte                feature_value,
               char**              pfeature_name)
{
   WITH_DH(ddct_dh,  {
         // this should be a function in vcp_feature_codes:
         char * feature_name = NULL;
         Version_Spec vspec = dh->vcp_version;
         Feature_Value_Entry * feature_value_entries = find_feature_values_new(feature_code, vspec);
         if (feature_value_entries == NULL) {
            rc = DDCL_ARG;
         }
         else {
            feature_name = get_feature_value_name(feature_value_entries, feature_value);
            if (feature_name == NULL)
               rc = DDCL_ARG;               // correct handling for value not found?
            else
               *pfeature_name = feature_name;
         }
   }
   );
}


DDCT_Status ddct_get_nontable_vcp_value(
               DDCT_Display_Handle             ddct_dh,
               VCP_Feature_Code                feature_code,
               DDCT_Non_Table_Value_Response * response)
{
   WITH_DH(ddct_dh,  {
       Parsed_Nontable_Vcp_Response * code_info;
       Global_Status_Code gsc = get_nontable_vcp_value_by_display_handle(dh, feature_code,&code_info);
       // DBGMSG(" get_nontable_vcp_value_by_display_handle() returned %s", gsc_desc(gsc));
       if (gsc == 0) {
          response->cur_value = code_info->cur_value;
          response->max_value = code_info->max_value;
          response->mh        = code_info->mh;
          response->ml        = code_info->ml;
          response->sh        = code_info->sh;
          response->sl        = code_info->sl;
       }
       else rc = gsc;    // ??
    } );
}

// untested
DDCT_Status ddct_get_table_vcp_value(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code    feature_code,
               int *               value_len,
               Byte**              value_bytes)
{
   WITH_DH(ddct_dh,
      {
         Buffer * p_table_bytes;
         rc =  get_table_vcp_value_by_display_handle(dh, feature_code, &p_table_bytes);
         if (rc == 0) {
            int len = p_table_bytes->len;
            *value_len = len;
            *value_bytes = malloc(len);
            memcpy(*value_bytes, p_table_bytes->bytes, len);
            buffer_free(p_table_bytes, __func__);
         }
      }
     );
}




DDCT_Status ddct_set_continuous_vcp_value(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code feature_code,
               int              new_value)
{
   WITH_DH(ddct_dh,  {
         Global_Status_Code gsc = set_nontable_vcp_value_by_dh(dh, feature_code, new_value);
         rc = gsc;
      } );
}


DDCT_Status ddct_set_simple_nc_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               Byte                 new_value)
{
   return ddct_set_continuous_vcp_value(ddct_dh, feature_code, new_value);
}


DDCT_Status ddct_set_raw_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               Byte                 hi_byte,
               Byte                 lo_byte)
{
   return ddct_set_continuous_vcp_value(ddct_dh, feature_code, hi_byte << 8 | lo_byte);
}



/* Retrieves the capabilities string for the monitor.
 *
 * Arguments:
 *   ddct_dh     display handle
 *   pcaps       address at which to return pointer to capabilities string.
 *               This string is in an internal DDC data structure and should
 *               not be freed by the caller.
 *
 * Returns:
 *   status code
 */
DDCT_Status ddct_get_capabilities_string(DDCT_Display_Handle ddct_dh, char** pcaps)
{
   WITH_DH(ddct_dh,
      {
         Global_Status_Code gsc = get_capabilities_string_by_display_handle(dh, pcaps);
         rc = gsc;
      }
   );
}

DDCT_Status ddct_get_profile_related_values(DDCT_Display_Handle ddct_dh, char** pprofile_values_string)
{
   WITH_DH(ddct_dh,
      {
         set_output_level(OL_PROGRAM);
         // DBGMSG("Before dumpvcp_to_string_by_display_handle()");
         rc = dumpvcp_to_string_by_display_handle(dh, pprofile_values_string);
         // DBGMSG("After dumpvcp_to_string_by_display_handle(), catenated=%p", catenated);
         // printf("(%s) strlen(catenated)=%ld, catenated=|%s|\n",
         //       __func__,
         //       strlen(catenated),
         //       catenated);
      }
   );
}

DDCT_Status ddct_set_profile_related_values(char * profile_values_string) {
   Global_Status_Code gsc = loadvcp_from_string(profile_values_string);
   return gsc;
}
