/** @file api_displays.c */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_c_api.h"

#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/dsa2.h"
#include "base/displays.h"
#include "base/monitor_model_key.h"
#include "base/per_display_data.h"
#include "base/rtti.h"

#include "i2c/i2c_sysfs.h"
#include "i2c/i2c_dpms.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_display_selection.h"
#include "ddc/ddc_status_events.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_watch_displays.h"

#include "libmain/api_base_internal.h"
#include "libmain/api_error_info_internal.h"
#include "libmain/api_displays_internal.h"


static inline bool valid_display_handle(Display_Handle * dh) {
   return (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) == 0);
}

#ifdef UNUSED
static inline bool valid_display_ref(Display_Ref * dref) {
   return (dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
}
#endif


DDCA_Status validate_ddca_display_ref(
      DDCA_Display_Ref ddca_dref,
      bool             basic_only,
      bool             require_not_asleep,
      Display_Ref**    dref_loc) {
   if (dref_loc)
      *dref_loc = NULL;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   DDCA_Status result = ddc_validate_display_ref(dref, basic_only, require_not_asleep);
   if (result == DDCRC_OK && dref_loc)
      *dref_loc = dref;
   return result;
}


#ifdef UNUSED
Display_Handle * validated_ddca_display_handle(DDCA_Display_Handle ddca_dh) {
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0  ||
          !ddc_is_valid_display_handle(dh) )
         dh=NULL;
   }
   return dh;
}
#endif


DDCA_Status validate_ddca_display_handle(DDCA_Display_Handle ddca_dh, Display_Handle** dh_loc) {
   if (dh_loc)
      *dh_loc = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   DDCA_Status result = DDCRC_ARG;
   if (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER,4) == 0) {
      result = ddc_validate_display_handle(dh);
   }
   if (result == DDCRC_OK && dh_loc)
      *dh_loc = dh;
   return result;
}



// forward declarations
void dbgrpt_display_info(DDCA_Display_Info * dinfo, int depth);
void dbgrpt_display_info_list(DDCA_Display_Info_List * dlist, int depth);

#ifdef REMOVED
DDCA_Status
ddca_enable_usb_display_detection(bool onoff) {
   return ddc_enable_usb_display_detection(onoff);
}

bool
ddca_ddca_is_usb_display_detection_enabled() {
   return ddc_is_usb_display_detection_enabled();
}
#endif


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
   API_PRECOND(did_loc);
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
   API_PRECOND(did_loc);
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
   API_PRECOND(did_loc);
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
   API_PRECOND(did_loc);
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
   API_PRECOND(did_loc);
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
   API_PRECOND(did_loc);
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
   // DBGMSG("Done.     Returning: %p", result);
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
   API_PROLOGX(debug, "did=%p, dref_loc=%p", did, dref_loc);
   assert(library_initialized);
   API_PRECOND_W_EPILOG(dref_loc);
   *dref_loc = NULL;
   DDCA_Status rc = 0;
   ddc_ensure_displays_detected();

   Display_Identifier * pdid = (Display_Identifier *) did;
   if (!pdid || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
      rc = DDCRC_ARG;
   }
   else {
      Display_Ref* dref = get_display_ref_for_display_identifier(pdid, CALLOPT_NONE);
      if (debug)
         DBGMSG("get_display_ref_for_display_identifier() returned %p", dref);
      if (dref)
         *dref_loc = dref;
      else
         rc = DDCRC_INVALID_DISPLAY;
   }

   API_EPILOG_WO_RETURN(debug, rc, "*dref_loc=%p", psc_name_code(rc), *dref_loc);
   TRACED_ASSERT( (rc==0 && *dref_loc) || (rc!=0 && !*dref_loc) );
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

#ifdef REMOVED
/** @deprecated All display references are persistent
 *
 *  Frees a display reference.
 *
 *  Use this function to safely release a #DDCA_Display_Ref.
 *  If the display reference was dynamically created, it is freed.
 *  If the display reference was permanently allocated (normal case), does nothing.
 *
 *  @param[in] dref  display reference to free
 *  @retval DDCRC_OK     success, or dref == NULL
 *  @retval DDCRC_ARG    dref does not point to a valid display reference
 *  @retval DDCRC_LOCKED dref is to a transient instance, and it is referenced
 *                       by an open display handle
 *
 *  @ingroup api_display_spec
 */
// __attribute__ ((deprecated ("DDCA_Display_Refs are always persistent")))
DDCA_Status
ddca_free_display_ref(
      DDCA_Display_Ref dref);
#endif

#ifdef REMOVED
// deprecated, not needed, in library there are no transient display refs
DDCA_Status
ddca_free_display_ref(DDCA_Display_Ref ddca_dref) {
   bool debug = false;
   API_PROLOG(debug, "ddca_dref=%p", ddca_dref);
   DDCA_Status psc = 0;
   free_thread_error_detail();
   if (ddca_dref) {
      WITH_VALIDATED_DR3(ddca_dref, psc,
         {
            if (dref->flags & DREF_TRANSIENT)
               psc = free_display_ref(dref);
         }
      );
   }
   API_EPILOG_WO_RETURN(debug, psc, "");
   return psc;
}
#endif


DDCA_Status
ddca_redetect_displays() {
   bool debug = false;
   API_PROLOGX(debug, "");
   ddc_redetect_displays();
   API_EPILOG(debug, 0, "");
}


const char *
ddca_dref_repr(DDCA_Display_Ref ddca_dref) {
   bool debug = false;
   DBGMSF(debug, "Starting.  ddca_dref = %p", ddca_dref);
   char * result = NULL;
   Display_Ref * dref = NULL;
   validate_ddca_display_ref(ddca_dref, /*basic_only*/ true, /* require_not_asleep */ false, &dref);
   if (dref) {
      result = dref_repr_t(dref);
   }
   DBGMSF(debug, "Done.     Returning: %s", result);
   return result;
}


void
ddca_dbgrpt_display_ref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  ddca_dref = %p, depth=%d", ddca_dref, depth);
   Display_Ref * dref = NULL;
   validate_ddca_display_ref(ddca_dref, /* basic_only*/ true, /* require_not_asleep */ false, &dref);
   rpt_vstring(depth, "DDCA_Display_Ref at %p:", dref);
   if (dref)
      dbgrpt_display_ref(dref, depth+1);
}


DDCA_Status
ddca_report_display_by_dref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, "ddca_dref=%p", ddca_dref);
   assert(library_initialized);

   Display_Ref * dref = NULL;
   DDCA_Status rc = validate_ddca_display_ref(ddca_dref,  /* basic_only*/ true, /*require_not_asleep*/ false,  &dref);
   if (rc == 0)
      ddc_report_display_by_dref(dref, depth);

   API_EPILOG_WO_RETURN(debug, rc, "");
   return rc;
}


DDCA_Status
ddca_validate_display_ref(DDCA_Display_Ref ddca_dref, bool require_not_asleep)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, "ddca_dref = %p", ddca_dref);
   assert(library_initialized);

   Display_Ref * dref = NULL;
   DDCA_Status rc = DDCRC_ARG;
   if (ddca_dref)
      rc = validate_ddca_display_ref(ddca_dref, /* basic_only*/ false,  require_not_asleep, &dref);
#ifdef REDUNDANT
   Error_Info * errinfo = NULL;
   if (rc != 0)
      errinfo = ERRINFO_NEW(rc, "");
   else {
      assert(sys_drm_connectors);
      if (!dref->drm_connector) {
         errinfo = ERRINFO_NEW(DDCRC_INTERNAL_ERROR, "dref->drm_connector == NULL");
      }
      else {
         int d = (debug) ? 1 : -1;
        bool edid_exists = RPT_ATTR_EDID(d, NULL, "/sys/class/drm", dref->drm_connector, "edid");
        if (!edid_exists) {
           errinfo = ERRINFO_NEW(DDCRC_DISCONNECTED, "/dev/i2c-%d", dref->io_path.path.i2c_busno);
        }
        else {
           if (dpms_check_drm_asleep_by_dref(dref))
              errinfo = ERRINFO_NEW(DDCRC_DPMS_ASLEEP, "/dev/i2c-%d", dref->io_path.path.i2c_busno);
        }
      }
   }
   DDCA_Status ddcrc = ERRINFO_STATUS(errinfo);
   DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(errinfo);
   errinfo_free_with_report(errinfo, debug, __func__);
   save_thread_error_detail(public_error_detail);
#endif

   API_EPILOG_WO_RETURN(debug, rc, "");
   return rc;
}


//
// Open and close display
//

#ifdef DEPRECATED

/** \deprecated Use #ddca_open_display2()
 * Open a display
 * @param[in]  ddca_dref    display reference for display to open
 * @param[out] ddca_dh_loc  where to return display handle
 * @return     status code
 *
 * Fails if display is already opened by another thread.
 * \ingroup api_display_spec
 */
// __attribute__ ((deprecated ("use ddca_open_display2()")))
DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * ddca_dh_loc);

DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * dh_loc)
{
   return ddca_open_display2(ddca_dref, false, dh_loc);
}
#endif


// unpublished extension


/** Options for opening a DDCA_Display_Ref
 */

typedef enum {
   DDCA_OPENOPT_NONE             = 0,
   DDCA_OPENOPT_WAIT             = 1,
   DDCA_OPENOPT_FORCE_SLAVE_ADDR = 2
} DDCA_Open_Options;


STATIC
DDCA_Status
ddca_open_display3(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Open_Options     options,
      DDCA_Display_Handle * dh_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug,
          "ddca_dref=%p, options=0x%02x, dh_loc=%p, on thread %d",
          ddca_dref, options, dh_loc, get_thread_id());
   DBGTRC_NOPREFIX(debug, DDCA_TRC_API,
          "library_initialized=%s, ddc_displays_already_detected() = %ld",
          sbool(library_initialized), ddc_displays_already_detected());
   TRACED_ASSERT(library_initialized);
   TRACED_ASSERT(ddc_displays_already_detected());

   API_PRECOND_W_EPILOG(dh_loc);
   Display_Ref * dref = NULL;
   *dh_loc = NULL;        // in case of error
   DDCA_Status rc = 0;
   Error_Info * err = NULL;
   rc  = validate_ddca_display_ref(ddca_dref, /* basic_only*/ false, /* require_not_asleep */ true, &dref);
   if (!rc) {
     Display_Handle* dh = NULL;
     Call_Options callopts = CALLOPT_NONE;
     if (options & DDCA_OPENOPT_WAIT)
        callopts |= CALLOPT_WAIT;
     if (options & DDCA_OPENOPT_FORCE_SLAVE_ADDR)
        callopts |= CALLOPT_FORCE_SLAVE_ADDR;
     err = ddc_open_display(dref,  callopts, &dh);
     if (!err)
        *dh_loc = dh;
     else {
        rc = err->status_code;
        DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(err);
        errinfo_free_with_report(err, debug, __func__);
        save_thread_error_detail(public_error_detail);
     }
   }

   API_EPILOG_WO_RETURN(debug, rc, "*dh_loc=%p -> %s", *dh_loc, dh_repr(*dh_loc));
   TRACED_ASSERT_IFF(rc==0, *dh_loc);
   return rc;
}


DDCA_Status
ddca_open_display2(
      DDCA_Display_Ref      ddca_dref,
      bool                  wait,
      DDCA_Display_Handle * dh_loc)
{
   return ddca_open_display3(ddca_dref,
                             (wait) ? DDCA_OPENOPT_WAIT : DDCA_OPENOPT_NONE,
                             dh_loc);
}


DDCA_Status
ddca_close_display(DDCA_Display_Handle ddca_dh) {
   bool debug = false;
   free_thread_error_detail();
   DDCA_Status rc = 0;
   Error_Info * err = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   API_PROLOGX(debug, "dh = %s", dh_repr(dh));
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
         err = errinfo_new(DDCRC_ARG, __func__, "Invalid display handle");
      }
      else {
         // TODO: ddc_close_display() needs an action if failure parm,
         err = ddc_close_display(dh);
      }
   }

   if (err) {
      rc = err->status_code;
      DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(err);
      errinfo_free_with_report(err, debug, __func__);
      save_thread_error_detail(public_error_detail);
   }

   API_EPILOG_WO_RETURN(debug, rc, "");
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


#ifdef NOT_PUBLISHED
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
#endif


#ifdef MCCS_VERSION_ID
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
#endif


//
// Monitor Model Identifier
//

#ifdef REMOVED

const Monitor_Model_Key UNDEFINED_MONITOR_MODEL_KEY = {{0}};

Monitor_Model_Key
ddca_mmk(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code)
{
   Monitor_Model_Key result = UNDEFINED_MONITOR_MODEL_KEY;
   if (mfg_id     && strlen(mfg_id)     < DDCA_EDID_MFG_ID_FIELD_SIZE &&
       model_name && strlen(model_name) < DDCA_EDID_MODEL_NAME_FIELD_SIZE)
   {
      result = monitor_model_key_value(mfg_id, model_name, product_code);
   }
   return result;
}

bool
ddca_mmk_eq(
      Monitor_Model_Key mmk1,
      Monitor_Model_Key mmk2)
{
   return monitor_model_key_eq(mmk1, mmk2);
}


bool
ddca_mmk_is_defined(
      Monitor_Model_Key mmk)
{
   return mmk.defined;
}


Monitor_Model_Key
ddca_mmk_from_dref(
      DDCA_Display_Ref   ddca_dref)
{
   Monitor_Model_Key result = UNDEFINED_MONITOR_MODEL_KEY;
   Display_Ref * dref = (Display_Ref *) ddca_dref;
   if (valid_display_ref(dref) && dref->mmid)
      result = *dref->mmid;
   return result;
}


Monitor_Model_Key
ddca_mmk_from_dh(
      DDCA_Display_Handle   ddca_dh)
{
   Monitor_Model_Key result = UNDEFINED_MONITOR_MODEL_KEY;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh) && dh->dref->mmid)
      result = *dh->dref->mmid;
   return result;
}
#endif

//
// Display Info
//

#ifdef DEPRECATED
/** @deprecated use #ddca_get_display_info_list2()
 * Gets a list of the detected displays.
 *
 *  Displays that do not support DDC are not included.
 *
 *  @return list of display summaries
 */
__attribute__ ((deprecated ("use ddca_get_display_info_list2()")))
DDCA_Display_Info_List *
ddca_get_display_info_list(void)
{
   DDCA_Display_Info_List * result = NULL;
   ddca_get_display_info_list2(false, &result);
   return result;
}
#endif


STATIC void init_display_info(Display_Ref * dref, DDCA_Display_Info * curinfo) {
   bool debug = false;
   DBGMSF(debug, "dref=%p, curinfo=%p", dref,curinfo);
   memcpy(curinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4);
   curinfo->dispno        = dref->dispno;

   curinfo->path = dref->io_path;
   if (dref->io_path.io_mode == DDCA_IO_USB) {
      curinfo->usb_bus    = dref->usb_bus;
      curinfo->usb_device = dref->usb_device;
   }

   DDCA_MCCS_Version_Spec vspec = DDCA_VSPEC_UNKNOWN;
   if (dref->dispno > 0) {
      vspec = get_vcp_version_by_dref(dref);
   }
   memcpy(curinfo->edid_bytes,    dref->pedid->bytes, 128);

#if __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
   STRLCPY(curinfo->mfg_id,     dref->pedid->mfg_id,       EDID_MFG_ID_FIELD_SIZE);
   STRLCPY(curinfo->model_name, dref->pedid->model_name,   EDID_MODEL_NAME_FIELD_SIZE);
   STRLCPY(curinfo->sn,         dref->pedid->serial_ascii, DDCA_EDID_SN_ASCII_FIELD_SIZE);
#pragma GCC diagnostic pop
#else
   STRLCPY(curinfo->mfg_id,     dref->pedid->mfg_id,       EDID_MFG_ID_FIELD_SIZE);
   STRLCPY(curinfo->model_name, dref->pedid->model_name,   EDID_MODEL_NAME_FIELD_SIZE);
   STRLCPY(curinfo->sn,         dref->pedid->serial_ascii, DDCA_EDID_SN_ASCII_FIELD_SIZE);
#endif
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

   DBGMSF(debug, "Done");
}


DDCA_Status
ddca_get_display_info(
      DDCA_Display_Ref  ddca_dref,
      DDCA_Display_Info ** dinfo_loc)
{
   bool debug = false;
   // causes return DDCRC_UNITIALIZED: called after explicit ddca_init()/init2() call failed
   API_PROLOGX(debug, "ddca_dref=%p", ddca_dref);
   // causes return DDCRC_ARG if dinfo_loc == NULL
   API_PRECOND_W_EPILOG(dinfo_loc);
   DDCA_Status ddcrc = 0;

   // if ddc_validate_display_ref() fails, returns its status code
   WITH_BASIC_VALIDATED_DR3(
         ddca_dref, ddcrc,
         {
            DDCA_Display_Info * info = calloc(1, sizeof(DDCA_Display_Info));
            init_display_info(dref, info);
            *dinfo_loc = info;
         }
   )

   API_EPILOG_WO_RETURN(debug, ddcrc, "");
   return ddcrc;
}


STATIC DDCA_Status
set_ddca_error_detail_from_open_errors() {
   bool debug = false;
   GPtrArray * errs = ddc_get_bus_open_errors();
   DDCA_Status master_rc = 0;
   if (errs && errs->len > 0) {
      Error_Info * master_error = errinfo_new(DDCRC_OTHER, __func__, "Error(s) opening ddc devices");
      for (int ndx = 0; ndx < errs->len; ndx++) {
         Bus_Open_Error * cur = g_ptr_array_index(errs, ndx);
         Error_Info * errinfo = NULL;
         if (cur->io_mode == DDCA_IO_I2C)
            errinfo = errinfo_new(cur->error, __func__, "Error %s opening /dev/i2c-%d",
                                             psc_desc(cur->error), cur->devno);
         else
            errinfo = errinfo_new(cur->error, __func__, "Error %s opening /dev/usb/hiddev%d %s",
                                             psc_desc(cur->error), cur->devno, (cur->detail) ? cur->detail : "");
         errinfo_add_cause(master_error, errinfo);
      }
      master_rc = master_error->status_code;
      DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(master_error);
      errinfo_free_with_report(master_error, debug, __func__);
      save_thread_error_detail(public_error_detail);
   }
   return master_rc;
}



DDCA_Status
ddca_get_display_refs(
      bool                include_invalid_displays,
      DDCA_Display_Ref**  drefs_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, "include_invalid_displays=%s", SBOOL(include_invalid_displays));

   API_PRECOND_W_EPILOG(drefs_loc);
   int dref_ct = 0;
   DDCA_Status ddcrc = 0;
   ddc_ensure_displays_detected();
   GPtrArray * filtered_displays = ddc_get_filtered_display_refs(include_invalid_displays);  // array of Display_Ref
   DDCA_Display_Ref * result_list = calloc(filtered_displays->len + 1,sizeof(DDCA_Display_Ref));
   DDCA_Display_Ref * cur_ddca_dref = result_list;
   for (int ndx = 0; ndx < filtered_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(filtered_displays, ndx);
         *cur_ddca_dref = (DDCA_Display_Ref*) dref;
         cur_ddca_dref++;
   }
   *cur_ddca_dref = NULL; // terminating NULL ptr, redundant since calloc()
   g_ptr_array_free(filtered_displays, true);

   dref_ct = 0;
   if (IS_DBGTRC(debug, DDCA_TRC_API|DDCA_TRC_DDC )) {
      DBGMSG("          *drefs_loc=%p");
      DDCA_Display_Ref * cur_ddca_dref = result_list;
      while (*cur_ddca_dref) {
         Display_Ref * dref = (Display_Ref*) *cur_ddca_dref;
         DBGMSG("          DDCA_Display_Ref %p -> display %d", *cur_ddca_dref, dref->dispno);
         cur_ddca_dref++;
         dref_ct++;
      }
   }

   *drefs_loc = result_list;
   assert(*drefs_loc);

   ddcrc = set_ddca_error_detail_from_open_errors();

   API_EPILOG(debug, ddcrc, "Returned list has %d displays", dref_ct);
}


DDCA_Status
ddca_get_display_info_list2(
      bool                      include_invalid_displays,
      DDCA_Display_Info_List**  dlist_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, "");

   int filtered_ct = 0;
   API_PRECOND_W_EPILOG(dlist_loc);

   DDCA_Status ddcrc = 0;
   ddc_ensure_displays_detected();
   GPtrArray * filtered_displays = ddc_get_filtered_display_refs(include_invalid_displays);  // array of Display_Ref

   filtered_ct = filtered_displays->len;

   int reqd_size =   offsetof(DDCA_Display_Info_List,info) + filtered_ct * sizeof(DDCA_Display_Info);
   DBGMSF(debug, "reqd_size=%d", reqd_size);
   DDCA_Display_Info_List * result_list = calloc(1,reqd_size);
   result_list->ct = filtered_ct;
   DBGMSF(debug, "sizeof(DDCA_Display_Info) = %zu,"
                 " sizeof(Display_Info_List) = %zu, reqd_size=%d, filtered_ct=%d, offsetof(DDCA_Display_Info_List,info) = %zu",
           sizeof(DDCA_Display_Info),
           sizeof(DDCA_Display_Info_List),
           reqd_size, filtered_ct, offsetof(DDCA_Display_Info_List,info));

   DDCA_Display_Info * curinfo = &result_list->info[0];
   for (int ndx = 0; ndx < filtered_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(filtered_displays, ndx);
      // DDCA_Display_Info * curinfo = &result_list->info[ndx++];

      DBGMSF(debug, "dref=%p, curinfo=%p", dref, curinfo);
      init_display_info(dref, curinfo);
      curinfo++;
   }
   g_ptr_array_free(filtered_displays, true);

   if (IS_DBGTRC(debug, DDCA_TRC_API|DDCA_TRC_DDC )) {
      DBGMSG("Final result list %p", result_list);
      dbgrpt_display_info_list(result_list, 2);
   }

   ddcrc = set_ddca_error_detail_from_open_errors();
   *dlist_loc = result_list;
   assert(*dlist_loc);

   API_EPILOG(debug, ddcrc, "Returned list has %d displays", filtered_ct);
}


void
ddca_free_display_info(DDCA_Display_Info * info_rec) {
   bool debug = false;
   API_PROLOG(debug, "info_rec->%p", info_rec);
   // DDCA_Display_Info contains no pointers, can simply be free'd
   // data structures.  Nothing to free.
   if (info_rec && memcmp(info_rec->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0) {
      info_rec->marker[3] = 'x';
      free(info_rec);
   }
   DBGTRC_DONE(debug, DDCA_TRC_API,"");
   DISABLE_API_CALL_TRACING();
}


void
ddca_free_display_info_list(DDCA_Display_Info_List * dlist) {
   bool debug = false;
   API_PROLOG(debug, "dlist=%p", dlist);
   if (dlist) {
      // n. DDCA_Display_Info contains no pointers,
      // DDCA_Display_Info_List can simply be free'd.
      for (int ndx = 0; ndx < dlist->ct; ndx++) {
          DDCA_Display_Info * info_rec = &dlist->info[ndx];
          if (memcmp(info_rec->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0)
             info_rec->marker[3] = 'x';
      }
      free(dlist);
   }
   DBGTRC_DONE(debug, DDCA_TRC_API, "");
   DISABLE_API_CALL_TRACING();
}


DDCA_Status
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   bool debug = false;
   API_PROLOGX(debug, "Starting. dinfo=%p, dinfo->dispno=%d, depth=%d", dinfo, dinfo->dispno, depth);
   DDCA_Status rc = 0;
   API_PRECOND_W_EPILOG(dinfo);
   API_PRECOND_W_EPILOG(memcmp(dinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0);
   if (rc == 0) {
      int d0 = depth;
      int d1 = depth+1;
      int d2 = depth+2;
      if (dinfo->dispno > 0)
         rpt_vstring(d0, "Display number:  %d", dinfo->dispno);
      else if (dinfo->dispno == DISPNO_BUSY)
         rpt_vstring(d0, "Busy display - Cannot communicate DDC");
      else
         rpt_label(  d0, "Invalid display - Does not support DDC");
      // rpt_vstring(      d1, "Display ref:         %p -> %s", dinfo->dref, dref_repr_t(dinfo->dref) );
      // rpt_vstring(d1, "IO mode:             %s", io_mode_name(dinfo->path.io_mode));
      switch(dinfo->path.io_mode) {
      case (DDCA_IO_I2C):
            rpt_vstring(d1, "I2C bus:              /dev/i2c-%d", dinfo->path.path.i2c_busno);
            break;
      case (DDCA_IO_USB):
            rpt_vstring(d1, "USB bus.device:       %d.%d",
                            dinfo->usb_bus, dinfo->usb_device);
            rpt_vstring(d1, "USB hiddev device:   /dev/usb/hiddev%d", dinfo->path.path.hiddev_devno);
            break;
      }

      rpt_vstring(d1, "Mfg Id:               %s", dinfo->mfg_id);
      rpt_vstring(d1, "Model:                %s", dinfo->model_name);
      rpt_vstring(d1, "Product code:         %u", dinfo->product_code);
      rpt_vstring(d1, "Serial number:        %s", dinfo->sn);

      // binary SN is not part of DDCA_Display_Info
      Parsed_Edid * edid = create_parsed_edid(dinfo->edid_bytes);
      if (edid) {     // should never fail, but being ultra-cautious
         // Binary serial number is typically 0x00000000 or 0x01010101, but occasionally
         // useful for differentiating displays that share a generic ASCII "serial number"
         rpt_vstring(d1,"Binary serial number: %"PRIu32" (0x%08x)", edid->serial_binary, edid->serial_binary);
         free_parsed_edid(edid);
      }

#ifdef NOT_WORKING
      if (dinfo->path.io_mode == DDCA_IO_I2C) {
         I2C_Sys_Info * info = get_i2c_sys_info(dinfo->path.path.i2c_busno, -1);
         rpt_vstring(d1, "DRM Connector:        %s", (info->connector) ? info->connector : "");
      }
#endif

      // rpt_label(  d1, "Monitor Model Id:");
      // rpt_vstring(d2, "Mfg Id:           %s", dinfo->mmid.mfg_id);
      // rpt_vstring(d2, "Model name:       %s", dinfo->mmid.model_name);
      // rpt_vstring(d2, "Product code:     %d", dinfo->mmid.product_code);
      rpt_vstring(d1, "EDID:");
      rpt_hex_dump(dinfo->edid_bytes, 128, d2);
      // rpt_vstring(d1, "dref:                %p", dinfo->dref);
      rpt_vstring(d1, "VCP Version:          %s", format_vspec(dinfo->vcp_version));
   // rpt_vstring(d1, "VCP Version Id:      %s", format_vcp_version_id(dinfo->vcp_version_id) );


      if (dinfo->dispno == DISPNO_BUSY) {
   #ifdef OLD
         rpt_nl();
         char fn[20];
         int busno =  dinfo->path.path.i2c_busno;
         g_snprintf(fn, 20, "/dev/bus/ddcci/%d", busno);
         struct stat statrec;
         if (stat(fn, &statrec) == 0 )
            rpt_vstring(d1, "Driver ddcci is hogging I2C slave address x37 (DDC) on /dev/i2c-%d", busno);
   #endif
         Display_Ref * dref = (Display_Ref *) dinfo->dref;
         int busno = dref->io_path.path.i2c_busno;
         GPtrArray * conflicts = collect_conflicting_drivers(busno, -1);
         if (conflicts && conflicts->len > 0) {
            rpt_vstring(d1, "I2C bus is busy. Likely conflicting driver(s): %s",
                            conflicting_driver_names_string_t(conflicts));
            free_conflicting_drivers(conflicts);
         }
         else {
            struct stat stat_buf;
            char buf[20];
            g_snprintf(buf, 20, "/dev/bus/ddcci/%d", busno);
            // DBGMSG("buf: %s", buf);
            int rc = stat(buf, &stat_buf);
            // DBGMSG("stat returned %d", rc);
            if (rc == 0)
               rpt_label(d1, "I2C bus is busy. Likely conflict with driver ddcci.");
         }
         rpt_vstring(d1, "Consider using option --force-slave-address.");
      }
   }
   API_EPILOG(debug, rc, "");
}


void
dbgrpt_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dinfo=%p");
   ddca_report_display_info(dinfo, depth);
   int d1 = depth+1;

   rpt_vstring(d1, "dref:                %s", dref_repr_t(dinfo->dref));
   if (dinfo->dref) {  // paranoid, should never be NULL
      rpt_vstring(d1, "VCP Version (dref xdf): %s", format_vspec_verbose(((Display_Ref*)dinfo->dref)->vcp_version_xdf));
   }
   DBGMSF(debug, "Done.");
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
   DBGMSF(debug, "Done.");
}


//
// Miscellaneous
//

#ifdef DEPRECATED

// /** \deprecated */
__attribute__ ((deprecated))
DDCA_Status
ddca_get_edid_by_dref(
      DDCA_Display_Ref ddca_dref,
      uint8_t **       pbytes_loc);   // pointer into ddcutil data structures, do not free


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
#endif


#ifdef UNIMPLEMENTED
// Use ddca_get_edid_by_dref() instead
// n. edid_buffer must be >= 128 bytes

DDCA_Status
ddca_get_edid(DDCA_Display_Handle * dh, uint8_t* edid_buffer);
#endif


//
// Reports
//

#ifdef DEPRECATED
/** \deprecated use #ddca_report_displays()
 * Reports on all active displays.
 *  This function hooks into the code used by command "ddcutil detect"
 *
 *  @param[in] depth  logical indentation depth
 *  @return    number of MCCS capable displays
 */
__attribute__ ((deprecated ("use ddca_report_displays()")))
int
ddca_report_active_displays(
      int depth);


// deprecated, use ddca_report_displays()
int
ddca_report_active_displays(int depth) {
   bool debug = false;
   API_PROLOG(debug, "");
   int display_ct = ddc_report_displays(false, depth);
   DBGTRC_DONE(debug, DDCA_TRC_API, "Returning %d", display_ct);
   DISABLE_API_CALL_TRACING();
   return display_ct;
}
#endif


int
ddca_report_displays(bool include_invalid_displays, int depth) {
   bool debug = false;
   API_PROLOG(debug, "");
   int display_ct = 0;
   if (!library_initialization_failed) {
      display_ct = ddc_report_displays(include_invalid_displays, depth);
   }
   DBGTRC_DONE(debug, DDCA_TRC_API, "Returning: %d", display_ct);
   DISABLE_API_CALL_TRACING();
   return display_ct;
}

#ifdef DETAILED_DISPLAY_CHANGE_HANDLING

typedef enum {
   DDCA_DISPLAY_ADDED   = 0,
   DDCA_DISPLAY_REMOVED = 1,
} DDCA_Display_Detection_Op;

typedef struct {
   DDCA_Display_Ref dref;
   DDCA_Display_Detection_Op operation;
} DDCA_Display_Detection_Report;


typedef void (*DDCA_Display_Status_Callback_Func)(DDCA_Display_Detection_Report);

DDCA_Status
ddca_register_display_status_callback(DDCA_Display_Status_Callback_Func func);
#endif



//
// Display Status Change Communication
//

DDCA_Status
ddca_register_display_status_callback(DDCA_Display_Status_Callback_Func func) {
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, "func=%p", func);

   DDCA_Status result = DDCRC_INVALID_OPERATION;
 #ifdef ENABLE_UDEV
    result = (all_sysfs_i2c_info_drm(/*rescan=*/false))
                       ? ddc_register_display_status_callback(func)
                       : DDCRC_INVALID_OPERATION;
 #endif


   API_EPILOG(debug, result, "");
   return result;
}


DDCA_Status
ddca_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func) {
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, "func=%p", func);

   DDCA_Status result = ddc_unregister_display_status_callback(func);

   API_EPILOG(debug, result, "");
   return result;
}


const char *
   ddca_display_event_type_name(DDCA_Display_Event_Type event_type) {
      return ddc_display_event_type_name(event_type);
}

//
// Sleep Multiplier Control
//

DDCA_Status
ddca_set_display_sleep_multiplier(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Sleep_Multiplier multiplier)
{
   bool debug = false;
   free_thread_error_detail();
    API_PROLOGX(debug, "ddca_dref=%p", ddca_dref);

    assert(library_initialized);
    Display_Ref * dref = NULL;
    DDCA_Status rc = validate_ddca_display_ref(ddca_dref, /* basic_only*/ true, /*require_not_asleep*/false, &dref);
    if (rc == 0)  {
       Per_Display_Data * pdd = dref->pdd;
       if (multiplier >= 0.0 && multiplier <= 10.0) {
          pdd_reset_multiplier(pdd, multiplier);
       }
       else
          rc = DDCRC_ARG;
    }
    API_EPILOG_WO_RETURN(debug, rc, "");
    return rc;
}


DDCA_Status
ddca_get_current_display_sleep_multiplier(
      DDCA_Display_Ref        ddca_dref,
      DDCA_Sleep_Multiplier*  multiplier_loc)
{
   bool debug = false;
   free_thread_error_detail();
    API_PROLOGX(debug, "ddca_dref=%p", ddca_dref);

    assert(library_initialized);
    Display_Ref * dref = NULL;
    DDCA_Status rc = validate_ddca_display_ref(ddca_dref, true, false, &dref);
    if (rc == 0) {
       Per_Display_Data * pdd = dref->pdd;
       *multiplier_loc        = pdd->final_successful_adjusted_sleep_multiplier;
    }
    API_EPILOG_WO_RETURN(debug, rc, "");
    return rc;
}


bool
ddca_enable_dynamic_sleep(bool onoff)
{
   bool debug = false;
   API_PROLOG(debug, "");
   free_thread_error_detail();

   bool old = pdd_is_dynamic_sleep_enabled();
   pdd_enable_dynamic_sleep_all(onoff);

   API_EPILOG_NO_RETURN(debug, "Returning %s", sbool(old));
   return old;
}


bool ddca_is_dynamic_sleep_enabled()
{
   bool debug = false;
   API_PROLOG(debug, "");
   free_thread_error_detail();

   bool result = pdd_is_dynamic_sleep_enabled();

   API_EPILOG_NO_RETURN(debug, "Returning %s", sbool(result));
   return result;
}


//
// Module initialization
//

void init_api_displays() {
   RTTI_ADD_FUNC(ddca_close_display);
   RTTI_ADD_FUNC(ddca_get_display_info_list2);
   RTTI_ADD_FUNC(ddca_get_display_info);
   RTTI_ADD_FUNC(ddca_get_display_ref);
   RTTI_ADD_FUNC(ddca_get_display_refs);
   RTTI_ADD_FUNC(ddca_open_display2);
   RTTI_ADD_FUNC(ddca_open_display3);
   RTTI_ADD_FUNC(ddca_redetect_displays);
   RTTI_ADD_FUNC(ddca_report_display_by_dref);
   RTTI_ADD_FUNC(ddca_register_display_status_callback);
   RTTI_ADD_FUNC(ddca_unregister_display_status_callback);
   RTTI_ADD_FUNC(validate_ddca_display_ref);
   RTTI_ADD_FUNC(ddca_validate_display_ref);
}

