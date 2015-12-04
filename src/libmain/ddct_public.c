/*
 * ddct_public.c
 *
 *  Created on: Dec 1, 2015
 *      Author: rock
 */

#include <string.h>

#include "base/ddc_errno.h"
#include "base/displays.h"
#include "base/ddc_packets.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_vcp.h"
#include "ddc/vcp_feature_codes.h"

#include "libmain/ddct_public.h"

static bool library_initialized = false;

void init_ddct() {
   init_ddc_services();
   library_initialized = true;
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

DDCT_Status ddct_get_display_ref(DDCT_Display_Identifier did, DDCT_Display_Ref* ddct_dref) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Identifier * pdid = (Display_Identifier *) did;
   if (pdid == NULL || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
     rc = DDCL_ARG;
   }
   else {
      Display_Ref* dref = get_display_ref_for_display_identifier(pdid, false /* emit_error_msg */);
      if (dref)
         *ddct_dref = dref;
      else
         rc = DDCL_ARG;
   }
   return rc;
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

#define WITH_DH(action) \
   do { \
      if (!library_initialized) \
         return DDCL_UNINITIALIZED; \
      DDCT_Status rc = 0; \
      Display_Handle * dh = (Display_Handle *) ddct_dh; \
      if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  { \
         rc = DDCL_ARG; \
      } \
      else { \
         (action); \
      } \
      return rc; \
   } while(0);


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



DDCT_Status ddct_get_mccs_version(DDCT_Display_Handle ddct_dh, Version_Spec* pspec) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCT_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddct_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
   }
   else {
      Version_Spec vspec = dh->vcp_version;
      *pspec = vspec;
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

// static char  default_feature_name_buffer[40];

// or return a struct?
DDCT_Status ddct_get_feature_info(VCP_Feature_Code feature_code, unsigned long * flags) {
   return DDCL_UNIMPLEMENTED;
}

char *      ddct_get_feature_name(VCP_Feature_Code feature_code) {
   // do we want get_feature_name()'s handling of mfg specific and unrecognzied codes?
   char * result = get_feature_name(feature_code);
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


   WITH_DH( {
         // this should be a function in vcp_feature_codes:
         char * feature_name = NULL;
         Version_Spec vspec = dh->vcp_version;
         Feature_Value_Entry * feature_value_entries = find_feature_values_new(feature_code, vspec);
         if (feature_value_entries == NULL) {
            rc = DDCL_ARG;
         }
         else {
            feature_name = find_value_name_new(feature_value_entries, feature_value);
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
   WITH_DH( {
       Interpreted_Vcp_Code * code_info;
       Global_Status_Code gsc = get_vcp_by_display_handle(dh, feature_code,&code_info);
       if (gsc != 0) {
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


DDCT_Status ddct_set_nontable_vcp_value(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code feature_code,
               int              new_value)
{
   WITH_DH( {
         Global_Status_Code gsc = set_vcp_by_display_handle(dh, feature_code, new_value);
         rc = gsc;
      } );

}

// caller allocate buffer, or should function?
// for now function allocates buffer, caller needs to free
// todo: lower level functions should cache capabilities string;
DDCT_Status ddct_get_capabilities_string(DDCT_Display_Ref ddct_dref, char** buffer)
{
   WITH_DR(ddct_dref,
         (   {
             Buffer * pCapabilitiesBuffer = NULL;
             Global_Status_Code gsc = get_capabilities(dref, &pCapabilitiesBuffer);
             if (gsc != 0) {
                char * capstring = strdup((char *)pCapabilitiesBuffer->bytes);
                *buffer = capstring;


             }
             else
                rc = gsc;
              }
         )
   );
}
