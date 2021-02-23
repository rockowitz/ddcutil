// api_displays.c

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_c_api.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"

#include "base/displays.h"
#include "base/monitor_model_key.h"

#include "libmain/api_base_internal.h"
#include "libmain/api_displays_internal.h"


static inline bool valid_display_handle(Display_Handle * dh) {
   return (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) == 0);
}

static inline bool valid_display_ref(Display_Ref * dref) {
   return (dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
}

// forward declarations
void dbgrpt_display_info(DDCA_Display_Info * dinfo, int depth);
void dbgrpt_display_info_list(DDCA_Display_Info_List * dlist, int depth);


DDCA_Status
ddca_enable_usb_display_detection(bool onoff) {
   return ddc_enable_usb_display_detection(onoff);
}

bool
ddca_ddca_is_usb_display_detection_enabled() {
   return ddc_is_usb_display_detection_enabled();
}


//
// Display Identifiers
//

DDCA_Status
ddca_create_dispno_display_identifier(
      int                      dispno,
      DDCA_Display_Identifier* did_loc)
{
   free_thread_error_detail();
   // assert(did_loc);
   PRECOND(did_loc);
   Display_Identifier* did = create_dispno_display_identifier(dispno);
   *did_loc = did;
   assert(*did_loc);
   return 0;
}


DDCA_Status
ddca_create_busno_display_identifier(
      int busno,
      DDCA_Display_Identifier* did_loc)
{
   free_thread_error_detail();
   // assert(did_loc);
   PRECOND(did_loc);
   Display_Identifier* did = create_busno_display_identifier(busno);
   *did_loc = did;
   assert(*did_loc);
   return 0;
}


DDCA_Status
ddca_create_mfg_model_sn_display_identifier(
      const char*              mfg_id,
      const char*              model_name,
      const char*              serial_ascii,
      DDCA_Display_Identifier* did_loc)
{
   free_thread_error_detail();
   // assert(did_loc);
   PRECOND(did_loc);
   *did_loc = NULL;
   DDCA_Status rc = 0;

   // break up the invalid argument tests for clarity

   // At least 1 argument must be specified
   if (  ( !mfg_id       || strlen(mfg_id)       == 0)  &&
         ( !model_name   || strlen(model_name)   == 0)  &&
         ( !serial_ascii || strlen(serial_ascii) == 0)
      )
      rc = DDCRC_ARG;

   // check if any arguments are too long
   else if (
        (model_name   && strlen(model_name)   >= EDID_MODEL_NAME_FIELD_SIZE)  ||
        (mfg_id       && strlen(mfg_id)       >= EDID_MFG_ID_FIELD_SIZE)      ||
        (serial_ascii && strlen(serial_ascii) >= EDID_SERIAL_ASCII_FIELD_SIZE)
      )
      rc = DDCRC_ARG;

   else {
      *did_loc = create_mfg_model_sn_display_identifier(
                     mfg_id, model_name, serial_ascii);
   }
   assert( (rc==0 && *did_loc) || (rc!=0 && !*did_loc));
   return rc;
}


DDCA_Status
ddca_create_edid_display_identifier(
      const Byte *              edid,
      DDCA_Display_Identifier * did_loc)    // 128 byte EDID
{
   // assert(did_loc);
   free_thread_error_detail();
   PRECOND(did_loc);
   *did_loc = NULL;
   DDCA_Status rc = 0;
   if (edid == NULL) {
      rc = DDCRC_ARG;
      *did_loc = NULL;
   }
   else {
      *did_loc = create_edid_display_identifier(edid);
   }
   assert( (rc==0 && *did_loc) || (rc!=0 && !*did_loc));
   return rc;
}


DDCA_Status
ddca_create_usb_display_identifier(
      int                      bus,
      int                      device,
      DDCA_Display_Identifier* did_loc)
{
   // assert(did_loc);
   free_thread_error_detail();
   PRECOND(did_loc);
   Display_Identifier* did = create_usb_display_identifier(bus, device);
   *did_loc = did;
   assert(*did_loc);
   return 0;
}


DDCA_Status
ddca_create_usb_hiddev_display_identifier(
      int                      hiddev_devno,
      DDCA_Display_Identifier* did_loc)
{
   // assert(did_loc);
   free_thread_error_detail();
   PRECOND(did_loc);
   Display_Identifier* did = create_usb_hiddev_display_identifier(hiddev_devno);
   *did_loc = did;
   assert(*did_loc);
   return 0;
}


DDCA_Status
ddca_free_display_identifier(
      DDCA_Display_Identifier did)
{
   free_thread_error_detail();
   DDCA_Status rc = 0;
   Display_Identifier * pdid = (Display_Identifier *) did;
   if (pdid) {
      if ( memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )
         rc = DDCRC_ARG;
      else
         free_display_identifier(pdid);
   }
   return rc;
}


const char *
ddca_did_repr(DDCA_Display_Identifier ddca_did) {
   // DBGMSG("Starting.  ddca_did=%p", ddca_did);
   char * result = NULL;
   Display_Identifier * pdid = (Display_Identifier *) ddca_did;
   if (pdid != NULL && memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) == 0 )  {
      result = did_repr(pdid);
   }
   // DBGMSG("Done.  Returning: %p", result);
   return result;
}


//
// Display References
//

DDCA_Status
ddca_get_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc)
{
   free_thread_error_detail();
   bool debug = false;
   DBGMSF(debug, "Starting.  did=%p, dref_loc=%p", did, dref_loc);
   assert(library_initialized);
   // assert(dref_loc);
   PRECOND(dref_loc);
   DBGMSF(debug,"    *dref_loc=%p", *dref_loc);
   DDCA_Status rc = 0;

   ddc_ensure_displays_detected();

   Display_Identifier * pdid = (Display_Identifier *) did;
   if (!pdid || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
     rc = DDCRC_ARG;
   }
   else {
      Display_Ref* dref = get_display_ref_for_display_identifier(pdid, CALLOPT_ERR_MSG);
      if (debug)
         DBGMSG("get_display_ref_for_display_identifier() returned %p", dref);
      if (dref)
         *dref_loc = dref;
      else
         rc = DDCRC_INVALID_DISPLAY;
   }

   DBGMSF(debug, "Done.  Returning: %d", rc);
   if (rc == 0)
      DBGMSF(debug,"    *dref_loc=%p", *dref_loc);

   assert( (rc==0 && *dref_loc) || (rc!=0 && !*dref_loc));
   return rc;
}

// deprecated
DDCA_Status
ddca_create_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc)
{
   return ddca_get_display_ref(did, dref_loc);
}


// deprecated, not needed, in library there are no transient display refs
DDCA_Status
ddca_free_display_ref(DDCA_Display_Ref ddca_dref) {
   if (!ddca_dref) {
      free_thread_error_detail();
      return DDCRC_OK;
   }
   WITH_DR(ddca_dref,
         {
            if (dref->flags & DREF_TRANSIENT)
               psc = free_display_ref(dref);
         }
   );
}


// static char dref_work_buf[100];

const char *
ddca_dref_repr(DDCA_Display_Ref ddca_dref) {
   bool debug = false;
   DBGMSF(debug, "Starting.  ddca_dref = %p", ddca_dref);
   char * result = NULL;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (dref != NULL && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 )  {
#ifdef TOO_MUCH_WORK
      char * dref_type_name = io_mode_name(dref->ddc_io_mode);
      switch (dref->ddc_io_mode) {
      case(DISP_ID_BUSNO):
         snprintf(dref_work_buf, 100,
                  "Display Ref Type: %s, bus=/dev/i2c-%d", dref_type_name, dref->io_path.i2c_busno);
         break;
      case(DISP_ID_ADL):
         snprintf(dref_work_buf, 100,
                  "Display Ref Type: %s, adlno=%d.%d", dref_type_name, dref->io_path.adlno.iAdapterIndex, dref->io_path.adlno.iDisplayIndex);
         break;
      }
      *repr = did_work_buf;
#endif
      // result = dref_short_name(dref);
      result = dref_repr_t(dref);
   }
   DBGMSF(debug, "Done. Returning: %s", result);
   return result;
}


void
ddca_dbgrpt_display_ref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  ddca_dref = %p, depth=%d", ddca_dref, depth);
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   rpt_vstring(depth, "DDCA_Display_Ref at %p:", dref);
   dbgrpt_display_ref(dref, depth+1);
}


DDCA_Status
ddca_report_display_by_dref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   free_thread_error_detail();
   DDCA_Status rc = 0;

    assert(library_initialized);

    Display_Ref * dref = (Display_Ref *) ddca_dref;
    if ( !valid_display_ref(dref) )  {
       rc = DDCRC_ARG;
       goto bye;
    }

    ddc_report_display_by_dref(dref, depth);

bye:
   return rc;
}


//
// Open and close display

DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * dh_loc)
{
   return ddca_open_display2(ddca_dref, false, dh_loc);
}

DDCA_Status
ddca_open_display2(
      DDCA_Display_Ref      ddca_dref,
      bool                  wait,
      DDCA_Display_Handle * dh_loc)
{
   bool debug = false;
   free_thread_error_detail();
   assert(library_initialized);
   // assert(dh_loc);
   PRECOND(dh_loc);

   assert(ddc_displays_already_detected());
   // ddc_ensure_displays_detected();

   // pid_t thread_id = syscall(SYS_gettid);
   pid_t thread_id = get_thread_id(); 

   DDCA_Status rc = 0;
   *dh_loc = NULL;        // in case of error
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   DBGTRC(debug, DDCA_TRC_API, "ddca_dref=%s, wait=%s, on thread %d", dref_repr_t(dref), sbool(wait), thread_id);
   if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
      rc = DDCRC_ARG;
   }
   else {
     Display_Handle* dh = NULL;
     Call_Options callopts = CALLOPT_NONE;
     if (wait)
        callopts |= CALLOPT_WAIT;
     rc = ddc_open_display(dref,  callopts, &dh);
     if (rc == 0)
        *dh_loc = dh;
   }
   assert( (rc==0 && *dh_loc) || (rc!=0 && !*dh_loc));
   DBGTRC(debug, DDCA_TRC_API,  "Returning rc=%s, dh_loc=%p -> %s",
                                psc_desc(rc), dh_loc, dh_repr_t(*dh_loc));
   return rc;
}


DDCA_Status
ddca_close_display(DDCA_Display_Handle ddca_dh) {
   bool debug = false;
   free_thread_error_detail();
   assert(library_initialized);
   DDCA_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   DBGTRC(debug, DDCA_TRC_API, "Starting. dh = %s", dh_repr_t(dh));
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
         rc = DDCRC_ARG;
      }
      else {
         // TODO: ddc_close_display() needs an action if failure parm,
         rc = ddc_close_display(dh);
      }
   }
   DBGTRC(debug, DDCA_TRC_API, "Done.     Returning %s", psc_desc(rc));
   return rc;
}


//
// Display Handle
//

const char *
ddca_dh_repr(DDCA_Display_Handle ddca_dh) {
   char * repr = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh))
      repr = dh_repr(dh);
   return repr;
}


DDCA_Display_Ref
ddca_display_ref_from_handle(
      DDCA_Display_Handle   ddca_dh)
{
   DDCA_Display_Ref result = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh))
      result = dh->dref;
   return result;
}


DDCA_Status
ddca_get_mccs_version_by_dh(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec* p_spec)
{
   free_thread_error_detail();
   assert(library_initialized);
   DDCA_Status rc = 0;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (dh == NULL || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
      rc = DDCRC_ARG;
      p_spec->major = 0;
      p_spec->minor = 0;
   }
   else {
      // need to call function, may not yet be set
      DDCA_MCCS_Version_Spec vspec = get_vcp_version_by_dh(dh);
      p_spec->major = vspec.major;
      p_spec->minor = vspec.minor;
      rc = 0;
   }
   return rc;
}


// not published
DDCA_Status
ddca_get_mccs_version_with_default(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec  default_spec,
      DDCA_MCCS_Version_Spec* p_spec)
{
   DDCA_Status rc = ddca_get_mccs_version_by_dh(ddca_dh, p_spec);
   if (rc == 0 && vcp_version_eq(*p_spec, DDCA_VSPEC_UNKNOWN))
      *p_spec = default_spec;
   return rc;
}


//
// DDCA_MCCS_Version_Id functions - Deprecated
//

DDCA_Status
ddca_get_mccs_version_id(
      DDCA_Display_Handle    ddca_dh,
      DDCA_MCCS_Version_Id*  p_id)
{
   DDCA_MCCS_Version_Spec vspec;
   DDCA_Status rc = ddca_get_mccs_version_by_dh(ddca_dh, &vspec);
   if (rc == 0) {
      DDCA_MCCS_Version_Id  version_id = mccs_version_spec_to_id(vspec);
      *p_id = version_id;
   }
   else {
      *p_id = DDCA_MCCS_VNONE;
   }
   return rc;
}


char *
ddca_mccs_version_id_name(DDCA_MCCS_Version_Id version_id) {
   return vcp_version_id_name(version_id);
}


#ifdef DEFINED_BUT_NOT_RELEASED
/**  Returns the descriptive name of a #DDCA_MCCS_Version_Id,
 *  e.g. "2.0".
 *
 *  @param[in]  version_id  version id value
 *  @return descriptive name (do not free)
 *
 *  @remark added to replace ddca_mccs_version_id_desc() during 0.9
 *  development, but then use of DDCA_MCCS_Version_Id deprecated
 */

char *
ddca_mccs_version_id_string(DDCA_MCCS_Version_Id version_id) {
   return format_vcp_version_id(version_id);
}
#endif

char *
ddca_mccs_version_id_desc(DDCA_MCCS_Version_Id version_id) {
   return format_vcp_version_id(version_id);
}


//
// Monitor Model Identifier
//

const DDCA_Monitor_Model_Key DDCA_UNDEFINED_MONITOR_MODEL_KEY = {{0}};

DDCA_Monitor_Model_Key
ddca_mmk(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code)
{
   DDCA_Monitor_Model_Key result = DDCA_UNDEFINED_MONITOR_MODEL_KEY;
   if (mfg_id     && strlen(mfg_id)     < DDCA_EDID_MFG_ID_FIELD_SIZE &&
       model_name && strlen(model_name) < DDCA_EDID_MODEL_NAME_FIELD_SIZE)
   {
      result = monitor_model_key_value(mfg_id, model_name, product_code);
   }
   return result;
}

bool
ddca_mmk_eq(
      DDCA_Monitor_Model_Key mmk1,
      DDCA_Monitor_Model_Key mmk2)
{
   return monitor_model_key_eq(mmk1, mmk2);
}


bool
ddca_mmk_is_defined(
      DDCA_Monitor_Model_Key mmk)
{
   return mmk.defined;
}


DDCA_Monitor_Model_Key
ddca_mmk_from_dref(
      DDCA_Display_Ref   ddca_dref)
{
   DDCA_Monitor_Model_Key result = DDCA_UNDEFINED_MONITOR_MODEL_KEY;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (valid_display_ref(dref) && dref->mmid)
      result = *dref->mmid;
   return result;
}


DDCA_Monitor_Model_Key
ddca_mmk_from_dh(
      DDCA_Display_Handle   ddca_dh)
{
   DDCA_Monitor_Model_Key result = DDCA_UNDEFINED_MONITOR_MODEL_KEY;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh) && dh->dref->mmid)
      result = *dh->dref->mmid;
   return result;
}


//
// Display Info
//

DDCA_Display_Info_List *
ddca_get_display_info_list(void)
{
   DDCA_Display_Info_List * result = NULL;
   ddca_get_display_info_list2(false, &result);
   return result;
}


DDCA_Status
ddca_get_display_info_list2(
      bool                      include_invalid_displays,
      DDCA_Display_Info_List**  dlist_loc)
{
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_API|DDCA_TRC_DDC, "Starting");
   free_thread_error_detail();
   // assert(dlist_loc);
   PRECOND(dlist_loc);

   ddc_ensure_displays_detected();
   GPtrArray * all_displays = ddc_get_all_displays();

   int true_ct = all_displays->len;
   if (!include_invalid_displays) {
      true_ct = 0;         // number of valid displays
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         if (dref->dispno != -1)    // ignore invalid displays
            true_ct++;
      }
   }

   int reqd_size =   offsetof(DDCA_Display_Info_List,info) + true_ct * sizeof(DDCA_Display_Info);
   DDCA_Display_Info_List * result_list = calloc(1,reqd_size);
   result_list->ct = true_ct;
   DBGMSF(debug, "sizeof(DDCA_Display_Info) = %zu, sizeof(Display_Info_List) = %zu, reqd_size=%d, true_ct=%d, offsetof(DDCA_Display_Info_List,info) = %zu",
           sizeof(DDCA_Display_Info), sizeof(DDCA_Display_Info_List), reqd_size, true_ct, offsetof(DDCA_Display_Info_List,info));

   int true_ctr = 0;
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);

      if (dref->dispno != -1 || include_invalid_displays) {
         DDCA_Display_Info * curinfo = &result_list->info[true_ctr++];
         memcpy(curinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4);
         curinfo->dispno        = dref->dispno;

         curinfo->path = dref->io_path;
         if (dref->io_path.io_mode == DDCA_IO_USB) {
            curinfo->usb_bus    = dref->usb_bus;
            curinfo->usb_device = dref->usb_device;
         }

         DDCA_MCCS_Version_Spec vspec = DDCA_VSPEC_UNKNOWN;
         if (dref->dispno != -1) {
            vspec = get_vcp_version_by_dref(dref);
         }
         memcpy(curinfo->edid_bytes,    dref->pedid->bytes, 128);
         g_strlcpy(curinfo->mfg_id,     dref->pedid->mfg_id,       EDID_MFG_ID_FIELD_SIZE);
         g_strlcpy(curinfo->model_name, dref->pedid->model_name,   EDID_MODEL_NAME_FIELD_SIZE);
         g_strlcpy(curinfo->sn,         dref->pedid->serial_ascii, DDCA_EDID_SN_ASCII_FIELD_SIZE);
         curinfo->product_code  = dref->pedid->product_code;
         curinfo->vcp_version    = vspec;
         curinfo->dref           = dref;

#ifdef MMID
         curinfo->mmid = monitor_model_key_value(
                                        dref->pedid->mfg_id,
                                        dref->pedid->model_name,
                                        dref->pedid->product_code);
// #ifdef OLD
         assert(streq(curinfo->mfg_id,     curinfo->mmid.mfg_id));
         assert(streq(curinfo->model_name, curinfo->mmid.model_name));
         assert(curinfo->product_code == curinfo->mmid.product_code);
// #endif
#endif
      }
   }

   if (debug || IS_TRACING_GROUP( DDCA_TRC_API|DDCA_TRC_DDC )) {
      DBGMSG("Done. Returning %p", result_list);
      dbgrpt_display_info_list(result_list, 2);
   }

   *dlist_loc = result_list;
   assert(*dlist_loc);
   return 0;
}


static void
ddca_free_display_info(DDCA_Display_Info * info_rec) {
   // All pointers in DDCA_Display_Info are to permanently allocated
   // data structures.  Nothing to free.
   if (info_rec && memcmp(info_rec->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0) {
      info_rec->marker[3] = 'x';
      // free(info_rec);
   }
}


void
ddca_free_display_info_list(DDCA_Display_Info_List * dlist) {
   if (dlist) {
      for (int ndx = 0; ndx < dlist->ct; ndx++) {
         ddca_free_display_info(&dlist->info[ndx]);
      }
      free(dlist);
   }
}


void
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dinfo=%p, depth=%d", dinfo, depth);

   // assert(dinfo);
   PRECOND_NORC(dinfo);
   assert(memcmp(dinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0);
   int d0 = depth;
   int d1 = depth+1;
   int d2 = depth+2;
   if (dinfo->dispno > 0)
      rpt_vstring(d0, "Display number:  %d", dinfo->dispno);
   else
      rpt_label(  d0, "Invalid display - Does not support DDC");
   // rpt_vstring(d1, "IO mode:             %s", io_mode_name(dinfo->path.io_mode));
   switch(dinfo->path.io_mode) {
   case (DDCA_IO_I2C):
         rpt_vstring(d1, "I2C bus:             /dev/i2c-%d", dinfo->path.path.i2c_busno);
         break;
   case (DDCA_IO_ADL):
         rpt_vstring(d1, "ADL adapter.display: %d.%d",
                         dinfo->path.path.adlno.iAdapterIndex, dinfo->path.path.adlno.iDisplayIndex);
         break;
   case (DDCA_IO_USB):
         rpt_vstring(d1, "USB bus.device:      %d.%d",
                         dinfo->usb_bus, dinfo->usb_device);
         rpt_vstring(d1, "USB hiddev device:   /dev/usb/hiddev%d", dinfo->path.path.hiddev_devno);
         break;
   }

   rpt_vstring(d1, "Mfg Id:              %s", dinfo->mfg_id);
   rpt_vstring(d1, "Model:               %s", dinfo->model_name);
   rpt_vstring(d1, "Product code:        %u", dinfo->product_code);
   rpt_vstring(d1, "Serial number:       %s", dinfo->sn);
   // rpt_label(  d1, "Monitor Model Id:");
   // rpt_vstring(d2, "Mfg Id:           %s", dinfo->mmid.mfg_id);
   // rpt_vstring(d2, "Model name:       %s", dinfo->mmid.model_name);
   // rpt_vstring(d2, "Product code:     %d", dinfo->mmid.product_code);
   rpt_vstring(d1, "EDID:");
   rpt_hex_dump(dinfo->edid_bytes, 128, d2);
   // rpt_vstring(d1, "dref:                %p", dinfo->dref);
   rpt_vstring(d1, "VCP Version:         %s", format_vspec(dinfo->vcp_version));
// rpt_vstring(d1, "VCP Version Id:      %s", format_vcp_version_id(dinfo->vcp_version_id) );
   DBGMSF(debug, "Done");
}


void
dbgrpt_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   ddca_report_display_info(dinfo, depth);
   int d1 = depth+1;

   rpt_vstring(d1, "dref:                %s", dref_repr_t(dinfo->dref));
   if (dinfo->dref) {  // paranoid, should never be NULL
      rpt_vstring(d1, "VCP Version (dref xdf): %s", format_vspec_verbose(((Display_Ref*)dinfo->dref)->vcp_version_xdf));
   }
}


void
ddca_report_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  dlist=%p, depth=%d", dlist, depth);

   int d1 = depth+1;
   rpt_vstring(depth, "Found %d displays", dlist->ct);
   for (int ndx=0; ndx<dlist->ct; ndx++) {
      ddca_report_display_info(&dlist->info[ndx], d1);
   }
}


void
dbgrpt_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  dlist=%p, depth=%d", dlist, depth);

   int d1 = depth+1;
   rpt_vstring(depth, "Found %d displays", dlist->ct);
   for (int ndx=0; ndx<dlist->ct; ndx++) {
      dbgrpt_display_info(&dlist->info[ndx], d1);
   }
}


//
// Miscellaneous
//

// deprecated
DDCA_Status
ddca_get_edid_by_dref(
      DDCA_Display_Ref ddca_dref,
      uint8_t**        p_bytes)
{
   DDCA_Status rc = 0;
   *p_bytes = NULL;
   free_thread_error_detail();

   assert(library_initialized);

   Display_Ref * dref = (Display_Ref *) ddca_dref;
   // if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  {
   if ( !valid_display_ref(dref) )  {
      rc = DDCRC_ARG;
      goto bye;
   }

   // Parsed_Edid*  edid = ddc_get_parsed_edid_by_dref(dref);
   Parsed_Edid * edid = dref->pedid;
   assert(edid);
   *p_bytes = edid->bytes;

bye:
   return rc;
}


#ifdef UNIMPLEMENTED
// Use ddca_get_edid_by_dref() instead
// n. edid_buffer must be >= 128 bytes

DDCA_Status
ddca_get_edid(DDCA_Display_Handle * dh, uint8_t* edid_buffer);
#endif


//
// Reports
//

// deprecated, use ddca_report_displays()
int
ddca_report_active_displays(int depth) {
   return ddc_report_displays(false, depth);
}

int
ddca_report_displays(bool include_invalid_displays, int depth) {
   return ddc_report_displays(include_invalid_displays, depth);
}

