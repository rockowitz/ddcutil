/* ddcutil_c_api.c
 *
 * <copyright>
 * Copyright (C) 2015-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <config.h>

#include <assert.h>
#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/build_info.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/base_services.h"

#include "vcp/vcp_feature_codes.h"

#include "adl/adl_shim.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_edid.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "public/ddcutil_c_api.h"


#define WITH_DR(ddca_dref, action) \
   do { \
      if (!library_initialized) \
         return DDCL_UNINITIALIZED; \
      DDCA_Status psc = 0; \
      Display_Ref * dref = (Display_Ref *) ddca_dref; \
      if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  { \
         psc = DDCL_ARG; \
      } \
      else { \
         (action); \
      } \
      return psc; \
   } while(0);


#define WITH_DH(_ddca_dh_, _action_) \
   do { \
      if (!library_initialized) \
         return DDCL_UNINITIALIZED; \
      DDCA_Status psc = 0; \
      Display_Handle * dh = (Display_Handle *) _ddca_dh_; \
      if ( !dh || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  { \
         psc = DDCL_ARG; \
      } \
      else { \
         (_action_); \
      } \
      return psc; \
   } while(0);


//
// Build information
//

/*  Returns the ddcutil version as a string in the form "major.minor.micro".
 */
const char * ddca_ddcutil_version_string() {
   return BUILD_VERSION;
}


/* Indicates whether the ddcutil library was built with ADL support. .
 */
bool ddca_built_with_adl() {
   return adlshim_is_available();
}

/* Indicates whether the ddcutil library was built with support for USB connected monitors. .
 */
bool ddca_built_with_usb() {
#ifdef USE_USB
   return true;
#else
   return false;
#endif
}

// Alternative to individual ddca_built_with...() functions.
// conciseness vs documentatbility
// how to document bits?   should doxygen doc be in header instead?

/** Queries ddcutil library build options.
 *
 */
unsigned long ddca_get_build_options() {
   Byte result = 0x00;
#ifdef HAVE_ADL
   result |= DDCA_BUILT_WITH_ADL;
#endif
#ifdef USE_USB
         result |= DDCA_BUILT_WITH_USB;
#endif
#ifdef FAILSIM_ENABLED
         result |= DDCA_BUILT_WITH_FAILSIM;
#endif
   return result;
}


//
// Initialization
//

static bool library_initialized = false;

/* Initializes the ddcutil library module.
 *
 * It is not an error if this function is called more than once.
 */
void ddca_init() {
   // Note: Until init_msg_control() is called within init_base_services(),
   // FOUT is null, so DBGMSG() causes a segfault
   bool debug = true;
   if (!library_initialized) {
      init_base_services();
      init_ddc_services();
      set_output_level(OL_NORMAL);
      show_recoverable_errors = false;
      library_initialized = true;
      DBGMSF(debug, "library initialization executed");
   }
   else {
      DBGMSF(debug, "library was already initialized");
   }
}



static DDCA_Abort_Func  abort_func = NULL;
static jmp_buf abort_buf;

void ddca_register_abort_func(DDCA_Abort_Func func) {
   abort_func = func;

   int jmprc = setjmp(abort_buf);
   if (jmprc) {
      fprintf(stderr, "Aborting. Internal status code = %d\n", jmprc);
      Public_Status_Code psc = global_to_public_status_code(jmprc);
      abort_func(psc);
      // exit(EXIT_FAILURE);
   }
   register_jmp_buf(&abort_buf);
}




//
// Status Code Management
//

static Global_Status_Code ddca_to_global_status_code(DDCA_Status ddca_status) {
   return global_to_public_status_code(ddca_status);
}


// should be static, but not currently used, if static get warning
DDCA_Status global_to_ddca_status_code(Global_Status_Code gsc) {
   return global_to_public_status_code(gsc);
}


char * ddca_status_code_name(DDCA_Status status_code) {
   char * result = NULL;
   Global_Status_Code gsc = ddca_to_global_status_code(status_code);
   Status_Code_Info * code_info = find_global_status_code_info(gsc);
   if (code_info)
      result = code_info->name;
   return result;
}


char * ddca_status_code_desc(DDCA_Status status_code) {
   char * result = "unknown status code";
   Global_Status_Code gsc = ddca_to_global_status_code(status_code);
   Status_Code_Info * code_info = find_global_status_code_info(gsc);
   if (code_info)
   result = code_info->description;
   return result;
}


//
// Message Control
//

/* Redirects output that normally would go to STDOUT
 */
void ddca_set_fout(
      FILE * fout   /** where to write normal messages, if NULL suppress */
     )
{
   // DBGMSG("Starting. fout=%p", fout);
   if (!library_initialized)
      ddca_init();

   set_fout(fout);
}

void ddca_set_fout_to_default() {
   if (!library_initialized)
      ddca_init();
   set_fout_to_default();
}




/* Redirects output that normally would go to STDERR
 */
void ddca_set_ferr(
      FILE * ferr   /** where to write error messages, if NULL suppress */
      )
{
   if (!library_initialized)
      ddca_init();

   set_ferr(ferr);
}

void ddca_set_ferr_to_default() {
   if (!library_initialized)
      ddca_init();
   set_ferr_to_default();
}




DDCA_Output_Level ddca_get_output_level() {
   return get_output_level();
}

void ddca_set_output_level(
       DDCA_Output_Level newval
       )
{
      set_output_level(newval);
}

char * ddca_output_level_name(
      DDCA_Output_Level val
      )
{
   return output_level_name(val);
}

void ddca_enable_report_ddc_errors(bool onoff) {
   // global variable in core.c:
   show_recoverable_errors = onoff;
}

bool ddca_is_report_ddc_errors_enabled() {
   return show_recoverable_errors;
}


//
// Global Settings
//

int
ddca_get_max_max_tries() {
   return MAX_MAX_TRIES;
}

int  ddca_get_max_tries(DDCA_Retry_Type retry_type) {
   int result = 0;
   switch(retry_type) {
      case (DDCA_WRITE_ONLY_TRIES):
         result = ddc_get_max_write_only_exchange_tries();
      break;
   case (DDCA_WRITE_READ_TRIES):
      result = ddc_get_max_write_read_exchange_tries();
      break;
   case (DDCA_MULTI_PART_TRIES):
      result = ddc_get_max_multi_part_read_tries();
      break;
   }
   return result;
}


DDCA_Status ddca_set_max_tries(DDCA_Retry_Type retry_type, int max_tries) {
   DDCA_Status rc = 0;
   if (max_tries < 1 || max_tries > MAX_MAX_TRIES)
      rc = DDCL_ARG;
   else {
      switch(retry_type) {
      case (DDCA_WRITE_ONLY_TRIES):
         ddc_set_max_write_only_exchange_tries(max_tries);
         break;
      case (DDCA_WRITE_READ_TRIES):
         ddc_set_max_write_read_exchange_tries(max_tries);
         break;
      case (DDCA_MULTI_PART_TRIES):
         ddc_set_max_multi_part_read_tries(max_tries);
         break;
      }
   }
   return rc;
}





//
// Display Identifiers
//

DDCA_Status ddca_create_dispno_display_identifier(int dispno, DDCA_Display_Identifier* pdid) {
   Display_Identifier* did = create_dispno_display_identifier(dispno);
   *pdid = did;
   return 0;
}

DDCA_Status ddca_create_busno_display_identifier(
               int busno,
               DDCA_Display_Identifier* pdid) {
   Display_Identifier* did = create_busno_display_identifier(busno);
   *pdid = did;
   return 0;
}

DDCA_Status ddca_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex,
               DDCA_Display_Identifier* pdid) {
   Display_Identifier* did = create_adlno_display_identifier(iAdapterIndex, iDisplayIndex);
   *pdid = did;
   return 0;
}

DDCA_Status ddca_create_model_sn_display_identifier(
      const char* model_name,
      const char* serial_ascii,
      DDCA_Display_Identifier* pdid
     )
{
   *pdid = NULL;
   DDCA_Status rc = 0;
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
      *pdid = create_model_sn_display_identifier(model_name, serial_ascii);
   }
   return rc;
}

DDCA_Status ddca_create_edid_display_identifier(
               const Byte * edid,
               DDCA_Display_Identifier * pdid)    // 128 byte EDID
{
   *pdid = NULL;
   DDCA_Status rc = 0;
   if (edid == NULL) {
      rc = DDCL_ARG;
      *pdid = NULL;
   }
   else {
      *pdid = create_edid_display_identifier(edid);
   }
   return rc;
}

DDCA_Status ddca_create_usb_display_identifier(
               int bus,
               int device,
               DDCA_Display_Identifier* pdid) {
   Display_Identifier* did = create_usb_display_identifier(bus, device);
   *pdid = did;
   return 0;
}



DDCA_Status ddca_free_display_identifier(DDCA_Display_Identifier did) {
   DDCA_Status rc = 0;
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

DDCA_Status ddca_repr_display_identifier(DDCA_Display_Identifier ddct_did, char **repr) {
   DDCA_Status rc = 0;
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
            free(hs);
            break;
      }
      case(DISP_ID_DISPNO):
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, dispno=%d", did_type_name, pdid->dispno);
            break;
      case DISP_ID_USB:
            snprintf(did_work_buf, 100,
                     "Display Id Type: %s, dispno=%d", did_type_name, pdid->dispno);
            break;

      } // switch
      *repr = did_work_buf;
   }
   return rc;
}


//
// Display References
//

DDCA_Status ddca_create_display_ref(DDCA_Display_Identifier did, DDCA_Display_Ref* ddct_dref) {
   bool debug = false;
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCA_Status rc = 0;
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


DDCA_Status ddca_free_display_ref(DDCA_Display_Ref ddct_dref) {
   WITH_DR(ddct_dref,
         {
         free_display_ref(dref);
         }
   );
}


// static char dref_work_buf[100];

DDCA_Status ddca_repr_display_ref(DDCA_Display_Ref ddct_dref, char** repr){
   DDCA_Status rc = 0;
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
      *repr = "invalid display reference";
   }
   else {
#ifdef TOO_MUCH_WORK
      char * dref_type_name = mccs_io_mode_name(dref->ddc_io_mode);
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
      *repr = dref_short_name(dref);
   }
   return rc;
}

void        ddca_report_display_ref(DDCA_Display_Ref ddct_dref, int depth) {
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   rpt_vstring(depth, "DDCT_Display_Ref at %p:", dref);
   report_display_ref(dref, depth+1);
}


DDCA_Status ddca_open_display(DDCA_Display_Ref ddct_dref, DDCA_Display_Handle * pdh) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCA_Status rc = 0;
   *pdh = NULL;        // in case of error
   Display_Ref * dref = (Display_Ref *) ddct_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
   }
   else {
     Display_Handle* dh = NULL;
     rc = ddc_open_display(dref,  CALLOPT_ERR_MSG, &dh);
     if (rc == 0)
        *pdh = dh;
     else
        rc = DDCL_ARG;     //  TEMP, need a proper status code
   }
   return rc;
}


DDCA_Status ddca_close_display(DDCA_Display_Handle ddct_dh) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCA_Status rc = 0;
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

DDCA_Status ddca_repr_display_handle(DDCA_Display_Handle ddct_dh, char ** repr) {
   DDCA_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddct_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
      *repr = "invalid display handle";
   }
   else {
      char * dh_type_name = mccs_io_mode_name(dh->io_mode);
      switch (dh->io_mode) {
      case(DISP_ID_BUSNO):
         snprintf(dh_work_buf, 100,
                  "Display Handle Type: %s, bus=/dev/i2c-%d",
                  dh_type_name, dh->busno);
         break;
      case(DISP_ID_ADL):
         snprintf(dh_work_buf, 100,
                  "Display Handle Type: %s, adlno=%d.%d",
                  dh_type_name, dh->iAdapterIndex, dh->iDisplayIndex);
         break;
      case USB_IO:
         snprintf(dh_work_buf, 100,
                  "Display Handle Type: %s, bus=%d, device=%d",
                  dh_type_name, dh->usb_bus, dh->usb_device);
         break;
      }
      *repr = dh_work_buf;
   }
   // DBGMSG("repr=%p, *repr=%p, dh_work_buf=%p", repr, *repr, dh_work_buf);
   // DBGMSG("dh_work_buf=|%s|", dh_work_buf);
   // DBGMSG("Returning rc=%d, *repr=%s", rc, *repr);
   return rc;
}




DDCA_Status ddca_get_mccs_version(
               DDCA_Display_Handle     ddca_dh,
               DDCA_MCCS_Version_Spec* pspec) {
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCA_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
      pspec->major = 0;
      pspec->minor = 0;
   }
   else {
      // need to call function, may not yet be set
      DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(dh);
      pspec->major = vspec.major;
      pspec->minor = vspec.minor;
      rc = 0;
   }
   return rc;
}


DDCA_Status ddca_get_mccs_version_id(
               DDCA_Display_Handle   ddca_dh,
               DDCA_MCCS_Version_Id*  p_id)
{
   DDCA_MCCS_Version_Spec vspec;
   DDCA_Status rc = ddca_get_mccs_version(ddca_dh, &vspec);
   if (rc == 0) {
      DDCA_MCCS_Version_Id  version_id = mccs_version_spec_to_id(vspec);
      *p_id = version_id;
   }
   else {
      *p_id = 0;
   }
   return rc;
}


DDCA_Display_Info_List *
ddca_get_displays()
{
   Display_Info_List * info_list = ddc_get_valid_displays();
   int true_ct = 0;
   for (int ndx = 0; ndx < info_list->ct; ndx++) {
      Display_Info drec = info_list->info_recs[ndx];
      if (drec.dispno != -1)    // ignore invalid displays
         true_ct++;
   }

   int reqd_size =   offsetof(DDCA_Display_Info_List,info) + true_ct * sizeof(DDCA_Display_Info);
   DDCA_Display_Info_List * result_list = calloc(1,reqd_size);
   result_list->ct = true_ct;
   // DBGMSG("sizeof(DDCA_Display_Info) = %d, sizeof(Display_Info_List) = %d, reqd_size=%d, true_ct=%d, offsetof(DDCA_Display_Info_List,info) = %d",
   //       sizeof(DDCA_Display_Info), sizeof(DDCA_Display_Info_List), reqd_size, true_ct, offsetof(DDCA_Display_Info_List,info));

   int true_ctr = 0;
   for (int ndx = 0; ndx < info_list->ct; ndx++) {
      Display_Info drec = info_list->info_recs[ndx];
      if (drec.dispno != -1) {
         DDCA_Display_Info * curinfo = &result_list->info[true_ctr++];
         memcpy(curinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4);
         curinfo->dispno        = drec.dispno;
         Display_Ref * dref     = drec.dref;
         curinfo->io_mode       = dref->io_mode;
         curinfo->i2c_busno     = dref->busno;
         curinfo->iAdapterIndex = dref->iAdapterIndex;
         curinfo->iDisplayIndex = dref->iDisplayIndex;
         curinfo->usb_bus       = dref->usb_bus;
         curinfo->usb_device    = dref->usb_device;
         curinfo->edid_bytes    = drec.edid->bytes;
         // or should these be memcpy'd instead of just pointers, can edid go away?
         curinfo->mfg_id        = drec.edid->mfg_id;
         curinfo->model_name    = drec.edid->model_name;
         curinfo->sn            = drec.edid->serial_ascii;
      }
   }

   return result_list;
}

void ddca_report_display_info(DDCA_Display_Info * dinfo, int depth) {
   assert(dinfo);
   assert(memcmp(dinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0);
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0, "Display number:  %d", dinfo->dispno);
   rpt_vstring(d1, "IO mode:         %s", mccs_io_mode_name(dinfo->io_mode));
   switch(dinfo->io_mode) {
   case (DDC_IO_DEVI2C):
         rpt_vstring(d1, "I2C bus number:     %d", dinfo->i2c_busno);
         break;
   case (DDC_IO_ADL):
         rpt_vstring(d1, "ADL adapter.display:  %d.%d", dinfo->iAdapterIndex, dinfo->iDisplayIndex);
         break;
   case (USB_IO):
         rpt_vstring(d1, "USB bus.device:       %d.%d", dinfo->usb_bus, dinfo->usb_device);
         break;
   }
   rpt_vstring(d1, "Mfg Id:         %s", dinfo->mfg_id);
   rpt_vstring(d1, "Model:          %s", dinfo->model_name);
   rpt_vstring(d1, "Serial number:  %s", dinfo->sn);
   rpt_vstring(d1, "EDID:           %s", hexstring(dinfo->edid_bytes, 128));
}


void ddca_report_display_info_list(DDCA_Display_Info_List * dlist, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Found %d displays", dlist->ct);
   for (int ndx=0; ndx<dlist->ct; ndx++) {
      ddca_report_display_info(&dlist->info[ndx], d1);
   }
}





DDCA_Status
ddca_get_edid_by_display_ref(
      DDCA_Display_Ref ddca_dref,
      uint8_t** pbytes)
{
   if (!library_initialized)
      return DDCL_UNINITIALIZED;
   DDCA_Status rc = 0;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCL_ARG;
   }
   else {
      Parsed_Edid*  edid = ddc_get_parsed_edid_by_display_ref(dref);
      *pbytes = edid->bytes;
   }
   return rc;

}


#ifdef OLD
// or return a struct?
DDCA_Status ddca_get_feature_flags_by_vcp_version(
      VCP_Feature_Code         feature_code,
      DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Version_Feature_Flags *  flags)
{
   DDCA_Status rc = 0;
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (!pentry) {
      *flags = 0;
      rc = DDCL_ARG;
   }
   else {
      DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec);
      *flags = 0;
      // TODO handle subvariants REWORK
      if (vflags & VCP2_RO)
         *flags |= DDCA_RO;
      if (vflags & VCP2_WO)
         *flags |= DDCA_WO;
      if (vflags & VCP2_RW)
         *flags |= DDCA_RW;
      if (vflags & VCP2_CONT)
         *flags |= DDCA_CONTINUOUS;
#ifdef OLD
      if (pentry->flags & VCP_TYPE_V2NC_V3T) {
         if (vspec.major < 3)
            *flags |= DDCA_SIMPLE_NC;
         else
            *flags |= DDCA_TABLE;
      }
#endif
      else if (vflags & DDCA_TABLE)
         *flags |= DDCA_TABLE;
      else if (vflags & VCP2_NC) {
         if (vspec.major < 3)
            *flags |= DDCA_SIMPLE_NC;
         else {
            // TODO: In V3, some features use combination of high and low bytes
            // for now, mark all as simple
            *flags |= DDCA_SIMPLE_NC;
            // alt: DDCT_COMPLEX_NC
         }
      }
   }
   return rc;
}
#endif


DDCA_Status ddca_get_feature_info_by_vcp_version(
      VCP_Feature_Code      feature_code,
      // DDCT_MCCS_Version_Spec  vspec,
      DDCA_MCCS_Version_Id       mccs_version_id,
      Version_Feature_Info  ** p_info)
{
   DDCA_Status psc = 0;
   *p_info = NULL;
   // DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   Version_Feature_Info * info =  get_version_specific_feature_info(
         feature_code,
         false,                        // with_default
         mccs_version_id);
   if (!info)
      psc = DDCL_ARG;
   else
      *p_info = info;
   return psc;

}

// or return a struct?
DDCA_Status ddca_get_feature_info_by_display(
      DDCA_Display_Handle     ddca_dh,    // needed because in rare cases feature info is MCCS version dependent
      VCP_Feature_Code   feature_code,
      Version_Feature_Info **         p_info)
{
   WITH_DH(
      ddca_dh,
      {
         DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_display_handle(ddca_dh);
         // DDCT_MCCS_Version_Spec vspec2;           // = {vspec.major, vspec.minor};
         // vspec2.major = vspec.major;
         // vspec2.minor = vspec.minor;
         DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);

         psc = ddca_get_feature_info_by_vcp_version(feature_code, version_id, p_info);
      }
   );
}



// static char  default_feature_name_buffer[40];
char * ddca_get_feature_name(VCP_Feature_Code feature_code) {
   // do we want get_feature_name()'s handling of mfg specific and unrecognized codes?
   char * result = get_feature_name_by_id_only(feature_code);
   // snprintf(default_feature_name_buffer, sizeof(default_feature_name_buffer), "VCP Feature 0x%02x", feature_code);
   // return default_feature_name_buffer;
   return result;
}


//
// Display Inquiry
//


DDCA_Status ddca_get_simple_sl_value_table(
               VCP_Feature_Code      feature_code,
               DDCA_MCCS_Version_Id       mccs_version_id,
               DDCA_Feature_Value_Entry** pvalue_table)
{
   DDCA_Status rc = 0;
   *pvalue_table = NULL;
   DDCA_MCCS_Version_Spec vspec = mccs_version_id_to_spec(mccs_version_id);

   VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
   if (!pentry) {
        *pvalue_table = NULL;
        rc = DDCL_ARG;
  }
  else {
     DDCA_MCCS_Version_Spec vspec2 = {vspec.major, vspec.minor};
     DDCA_Version_Feature_Flags vflags = get_version_specific_feature_flags(pentry, vspec2);
     if (!(vflags & DDCA_SIMPLE_NC)) {
        *pvalue_table = NULL;
        rc = DDCL_ARG;    // need better code
     }
     else  {
        DDCA_Feature_Value_Entry * table = get_version_specific_sl_values(pentry, vspec2);
        DDCA_Feature_Value_Entry * table2 = (DDCA_Feature_Value_Entry*) table;    // identical definitions
        *pvalue_table = table2;
        rc = 0;
     }
  }
   return rc;
}



typedef void * Feature_Value_Table;   // temp



// or:
DDCA_Status ddct_get_nc_feature_value_name(
               DDCA_Display_Handle    ddct_dh,    // needed because value lookup mccs version dependent
               VCP_Feature_Code   feature_code,
               Byte                    feature_value,
               char**                  pfeature_name)
{
   WITH_DH(ddct_dh,  {
         // this should be a function in vcp_feature_codes:
         char * feature_name = NULL;
         DDCA_MCCS_Version_Spec vspec = dh->vcp_version;
         DDCA_Feature_Value_Entry * feature_value_entries = find_feature_values(feature_code, vspec);
         if (feature_value_entries == NULL) {
            psc = DDCL_ARG;
         }
         else {
            feature_name = get_feature_value_name(feature_value_entries, feature_value);
            if (feature_name == NULL)
               psc = DDCL_ARG;               // correct handling for value not found?
            else
               *pfeature_name = feature_name;
         }
   }
   );
}

// n.b. fills in the response buffer provided by the caller, does not allocate
DDCA_Status ddca_get_nontable_vcp_value(
               DDCA_Display_Handle             ddct_dh,
               VCP_Feature_Code                feature_code,
               DDCA_Non_Table_Value_Response * response)
{
   WITH_DH(ddct_dh,  {
       Parsed_Nontable_Vcp_Response * code_info;
       Global_Status_Code gsc = get_nontable_vcp_value(dh, feature_code,&code_info);
       // DBGMSG(" get_nontable_vcp_value() returned %s", gsc_desc(gsc));
       if (gsc == 0) {
          response->cur_value = code_info->cur_value;
          response->max_value = code_info->max_value;
          response->mh        = code_info->mh;
          response->ml        = code_info->ml;
          response->sh        = code_info->sh;
          response->sl        = code_info->sl;
       }
       else psc = global_to_public_status_code(gsc);
    } );
}

// Partial code for getting formatted value
//
//Parsed_Vcp_Response pvr;
//pvr.response_type = NON_TABLE_VCP_VALUE;
//pvr.non_table_response = response;
//
//Single_Vcp_Value * valrec =
// create_single_vcp_value_by_parsed_vcp_response(
//       feature_code,
//       *pvr);
//
//bool ok = vcp_format_feature_detail(
//       VCP_Feature_Table_Entry * vcp_entry,
//       DDCA_MCCS_Version_Spec              vcp_version,
//       Single_Vcp_Value *        valrec,
//#ifdef OLD
//       Parsed_Vcp_Response *     raw_data,
//#endif
//       char * *                  aformatted_data
//     )
//



// untested
DDCA_Status ddca_get_table_vcp_value(
               DDCA_Display_Handle ddca_dh,
               VCP_Feature_Code    feature_code,
               int *               value_len,
               Byte**              value_bytes)
{
   WITH_DH(ddca_dh,
      {
         Buffer * p_table_bytes = NULL;
         Global_Status_Code gsc;
         gsc =  get_table_vcp_value(dh, feature_code, &p_table_bytes);
         if (gsc == 0) {
            assert(p_table_bytes);  // avoid coverity warning
            int len = p_table_bytes->len;
            *value_len = len;
            *value_bytes = malloc(len);
            memcpy(*value_bytes, p_table_bytes->bytes, len);
            buffer_free(p_table_bytes, __func__);
         }
         psc = global_to_public_status_code(gsc);
      }
     );
}


// alt

DDCA_Status ddca_get_vcp_value(
       DDCA_Display_Handle  ddca_dh,
       VCP_Feature_Code     feature_code,
       Vcp_Value_Type       call_type,   // why is this needed?   look it up from dh and feature_code
       Single_Vcp_Value **  pvalrec)
{
   WITH_DH(ddca_dh,
         {
               *pvalrec = NULL;
               Global_Status_Code gsc = get_vcp_value(dh, feature_code, call_type, pvalrec);
               psc = global_to_public_status_code(gsc);
         }
   );
}


DDCA_Status ddca_get_formatted_vcp_value(
       DDCA_Display_Handle *     ddca_dh,
       VCP_Feature_Code     feature_code,
       char**                    p_formatted_value)
{
   WITH_DH(ddca_dh,
         {
               *p_formatted_value = NULL;
               DDCA_MCCS_Version_Spec vspec      = get_vcp_version_by_display_handle(dh);
               // DDCA_MCCS_Version_Id   version_id = mccs_version_spec_to_id(vspec);

               VCP_Feature_Table_Entry * pentry = vcp_find_feature_by_hexid(feature_code);
               if (!pentry) {
#ifdef ALT
               Version_Feature_Info * feature_info =
               get_version_specific_feature_info(
                     feature_code,
                     true,                    //    with_default
                     version_id);
               assert(feature_info);
               if (!feature_info) {
#endif
                  psc = DDCL_ARG;
               }
               else {
                  // TODO: fix to get version sensitive flags
                  DDCA_Version_Feature_Flags flags = get_version_specific_feature_flags(pentry, vspec);
                  // Version_Feature_Flags flags = feature_info->internal_feature_flags;
                   // n. will default to NON_TABLE_VCP_VALUE if not a known code
                   Vcp_Value_Type call_type = (flags & DDCA_TABLE) ?  TABLE_VCP_VALUE : NON_TABLE_VCP_VALUE;
                   Single_Vcp_Value * pvalrec;
                   Global_Status_Code gsc = get_vcp_value(dh, feature_code, call_type, &pvalrec);
                   if (gsc == 0) {
                      bool ok =
                      vcp_format_feature_detail(
                             pentry,
                             vspec,
                             pvalrec,
                             p_formatted_value
                           );
                      if (!ok) {
                         gsc = DDCL_ARG;    // ** WRONG CODE ***
                         assert(!p_formatted_value);
                      }
                   }

                   psc = global_to_public_status_code(gsc);
               }
         }
   )
}


DDCA_Status ddca_set_continuous_vcp_value(
               DDCA_Display_Handle   ddca_dh,
               VCP_Feature_Code feature_code,
               int                   new_value)
{
   WITH_DH(ddca_dh,  {
         Global_Status_Code gsc = set_nontable_vcp_value(dh, feature_code, new_value);
         psc = global_to_public_status_code(gsc);
      } );
}


DDCA_Status ddca_set_simple_nc_vcp_value(
               DDCA_Display_Handle     ddca_dh,
               VCP_Feature_Code   feature_code,
               Byte                    new_value)
{
   return ddca_set_continuous_vcp_value(ddca_dh, feature_code, new_value);
}


DDCA_Status ddca_set_raw_vcp_value(
               DDCA_Display_Handle    ddca_dh,
               VCP_Feature_Code  feature_code,
               Byte                   hi_byte,
               Byte                   lo_byte)
{
   return ddca_set_continuous_vcp_value(ddca_dh, feature_code, hi_byte << 8 | lo_byte);
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
DDCA_Status ddca_get_capabilities_string(DDCA_Display_Handle ddct_dh, char** pcaps)
{
   WITH_DH(ddct_dh,
      {
         Global_Status_Code gsc = get_capabilities_string(dh, pcaps);
         psc = public_to_global_status_code(gsc);
      }
   );
}


DDCA_Status ddca_get_profile_related_values(
               DDCA_Display_Handle ddca_dh,
               char**              pprofile_values_string)
{
   WITH_DH(ddca_dh,
      {
         bool debug = false;
         // set_output_level(OL_PROGRAM);  // not needed for _new() variant
         DBGMSF(debug, "Before dumpvcp_to_string_by_display_handle(), pprofile_values_string=%p, *pprofile_values_string=%p",
               pprofile_values_string, *pprofile_values_string);
         Global_Status_Code gsc = dumpvcp_as_string(dh, pprofile_values_string);
         DBGMSF(debug, "After dumpvcp_to_string_by_display_handle(), pprofile_values_string=%p, *pprofile_values_string=%p",
               pprofile_values_string, *pprofile_values_string);
         DBGMSF(debug, "*pprofile_values_string = |%s|", *pprofile_values_string);
         psc = public_to_global_status_code(gsc);
      }
   );
}


// TODO: handle display as optional argument
DDCA_Status ddca_set_profile_related_values(char * profile_values_string) {
   Global_Status_Code gsc = loadvcp_by_string(profile_values_string, NULL);
   return gsc;
}


//
// Reports
//

int ddca_report_active_displays(int depth) {
   return ddc_report_active_displays(depth);
}
