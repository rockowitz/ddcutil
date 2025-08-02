/** @file api_displays.c */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "sysfs/sysfs_conflicting_drivers.h"
#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_sys_drm_connector.h"

#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_display_selection.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"

#include "dw/dw_common.h"
#include "dw/dw_main.h"
#include "dw/dw_status_events.h"
#include "dw/dw_udev.h"

#include "libmain/api_base_internal.h"
#include "libmain/api_error_info_internal.h"

#include "libmain/api_displays_internal.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_API;


static inline bool valid_display_handle(Display_Handle * dh) {
   return (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) == 0);
}

#ifdef UNUSED
static inline bool valid_display_ref(Display_Ref * dref) {
   return (dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
}
#endif

#ifdef OLD
DDCA_Status ddci_validate_ddca_display_ref(
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
#endif


/** Validates an opaque #DDCA_Display_Ref, returning the corresponding
 *  #Display_Ref if successful.
 *
 *  @param  ddca_dref DDCA_Display_Ref
 *  @param  dh_loc    address at which to return the underlying Display_Handle.
 *  @return
 */
DDCA_Status ddci_validate_ddca_display_ref2(
      DDCA_Display_Ref        ddca_dref,
      Dref_Validation_Options validation_options,
      Display_Ref**           dref_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "ddca_dref=%p=%d, validation_options=0x%02x, dref_loc=%p",
                                         ddca_dref, ddca_dref, validation_options, dref_loc);
   DDCA_Status result = DDCRC_OK;
   if (dref_loc)
      *dref_loc = NULL;
   if (debug)
      dbgrpt_published_dref_hash("published_dref_hash", 1);
   Display_Ref * dref = dref_from_published_ddca_dref(ddca_dref);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "dref_from_ddca_dref() returned %s", dref_reprx_t(dref));
   if (!dref) {
      result = DDCRC_ARG;
   }
   else {
      // should be redundant with ddc_validate_display_ref2(), but something not being caught
      if (dref->flags & DREF_REMOVED) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "DREF_REMOVED set!");
         SYSLOG2(DDCA_SYSLOG_WARNING, "DREF_REMOVED set for %s", dref_reprx_t(dref));
         result = DDCRC_DISCONNECTED;
      }
      else if ( !(dref->flags & DREF_DDC_COMMUNICATION_WORKING) &&
                !(validation_options & DREF_VALIDATE_DDC_COMMUNICATION_FAILURE_OK)
              )
      {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "DREF_DDC_COMMUNICATION_WORKING not set!");
         result = DDCRC_INVALID_DISPLAY;
      }
      else {
         result =  ddc_validate_display_ref2(dref, validation_options);
      }
   }

   if (result == DDCRC_OK && dref_loc) {
      *dref_loc = dref;
      DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result, "ddca_dref=%p=%d. *dref_loc=%p -> %s",
            ddca_dref, ddca_dref, *dref_loc, dref_reprx_t(*dref_loc));
   }
   else
      DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result, "ddca_dref=%p=%d", ddca_dref, ddca_dref);
   return result;
}


/** Validates an opaque #DDCA_Display_Handle, returning the corresponding
 *  #Display_Handle if successful.
 *
 *  @param  ddca_dh  DDCA_Display_Handle
 *  @param  dh_loc   address at which to return the underlying Display_Handle.
 *  @return
 */
DDCA_Status validate_ddca_display_handle(DDCA_Display_Handle ddca_dh, Display_Handle** dh_loc) {
   if (dh_loc)
      *dh_loc = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   DDCA_Status result = DDCRC_ARG;
   if (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER,4) == 0) {
      result = ddc_validate_display_handle2(dh);
   }
   if (result == DDCRC_OK && dh_loc)
      *dh_loc = dh;
   return result;
}



// forward declarations
STATIC void dbgrpt_display_info(DDCA_Display_Info * dinfo, int depth);
STATIC void dbgrpt_display_info_list(DDCA_Display_Info_List * dlist, int depth);

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
   if (traced_function_stack_enabled)
     reset_current_traced_function_stack();
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
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
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
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
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
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
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
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
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
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
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
ddci_get_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "did=%s, dref_loc=%p", did_repr(did), dref_loc);

   *dref_loc = NULL;
   DDCA_Status rc = 0;
   ddc_ensure_displays_detected();

   Display_Identifier * pdid = (Display_Identifier *) did;
   if (!pdid || memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) != 0 )  {
      rc = DDCRC_ARG;
   }
   else {
      Display_Ref* dref = get_display_ref_for_display_identifier(pdid, CALLOPT_NONE);
      DBGMSF(debug, "get_display_ref_for_display_identifier() returned %p", dref);
      if (dref) {
         DDCA_Display_Ref ddca_dref = dref_to_ddca_dref(dref);
         add_published_dref_id_by_dref(dref);
         *dref_loc = ddca_dref;
      }
      else
         rc = DDCRC_INVALID_DISPLAY;
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc, "*dref_loc=%p", *dref_loc);
   TRACED_ASSERT( (rc==0 && *dref_loc) || (rc!=0 && !*dref_loc) );
   return rc;
}


DDCA_Status
ddca_get_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "did=%p, dref_loc=%p", did, dref_loc);
   assert(library_initialized);
   API_PRECOND_W_EPILOG(dref_loc);

  DDCA_Status rc = ddci_get_display_ref(did, dref_loc);

  API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, rc,
                       "*dref_loc=%p", psc_name_code(rc), *dref_loc);
  return rc;
}


// deprecated
DDCA_Status
ddca_create_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "did=%p, dref_loc=%p", did, dref_loc);
   assert(library_initialized);
   API_PRECOND_W_EPILOG(dref_loc);

   DDCA_Status rc = ddci_get_display_ref(did, dref_loc);

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, rc,
                        "*dref_loc=%p", psc_name_code(rc), *dref_loc);
   return rc;
}


// GMutex ddca_redetect_active_mutex;

DDCA_Status
ddca_redetect_displays() {
   bool debug = false;
   API_PROLOGX(debug, NORESPECT_QUIESCE, "");

   DDCA_Status ddcrc = 0;
#ifdef WATCH_DISPLAYS
   static bool ddca_redetect_active = false;
   bool perform_detect = true;

//   g_mutex_lock(ddca_redetect_active_mutex);
   if (ddca_redetect_active) {
	   SYSLOG2(DDCA_SYSLOG_ERROR, "Calling ddca_redetect_displays() when already active");
	   perform_detect = false;
	   ddcrc = DDCRC_INVALID_OPERATION;
   }
   // g_mutex_unlock(ddca_redetect_active_mutex);

   if (perform_detect) {
      if (active_callback_thread_ct() > 0) {
         SYSLOG2(DDCA_SYSLOG_ERROR, "Calling ddca_redetect_displays() when callback threads are active");
         SYSLOG2(DDCA_SYSLOG_ERROR, "Behavior is indeterminate.");
         // ddcrc = DDCRC_INVALID_OPERATION;
         // perform_detect = false;
      }
   }

   if (perform_detect) {
      ddca_redetect_active = true;
      quiesce_api();
      dw_redetect_displays();
      unquiesce_api();
      ddca_redetect_active = false;
   }
#else
#ifdef FUTURE
   ddc_discard_detected_displays();
   ddc_ensure_displays_detected();
   set_ddca_error_detail_from_open_errors();
#endif

   ddcrc = DDCRC_INVALID_OPERATION;
   SYSLOG2(DDCA_SYSLOG_ERROR, "ddca_redetect_displays() unsupported - libddcutil not built with support for watching display connection changes");
#endif

   API_EPILOG_RET_DDCRC(debug, NORESPECT_QUIESCE, ddcrc, "");
}


const char *
ddca_dref_repr(DDCA_Display_Ref ddca_dref) {
   bool debug = false;
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "ddca_dref=%p", ddca_dref);

   Display_Ref * dref = dref_from_published_ddca_dref(ddca_dref);
   char * result = (dref) ? dref_reprx_t(dref) : "Invalid DDCA_Display_Ref";

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "ddca_dref=%p, returning: %s", ddca_dref, result);
   return result;
}


void
ddca_dbgrpt_display_ref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   bool debug = false;
   if (traced_function_stack_enabled)
      reset_current_traced_function_stack();
   DBGMSF(debug, "Starting.  ddca_dref = %p, depth=%d", ddca_dref, depth);
   Display_Ref * dref = ddca_dref;
   if (dref && memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0) {
      rpt_vstring(depth, "DDCA_Display_Ref at %p:", dref);
      dbgrpt_display_ref(dref, true, depth+1);
   }
   else {
      rpt_vstring(depth, "Not a display ref: %p", dref);
   }
}


DDCA_Status
ddca_report_display_by_dref(
      DDCA_Display_Ref ddca_dref,
      int              depth)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "ddca_dref=%p", ddca_dref);
   assert(library_initialized);

   Display_Ref * dref = NULL;
   // DDCA_Status rc = ddci_validate_ddca_display_ref(ddca_dref,  /* basic_only*/ true, /*require_not_asleep*/ false,  &dref);
   DDCA_Status rc = ddci_validate_ddca_display_ref2(ddca_dref,  DREF_VALIDATE_EDID,  &dref);
   if (rc == 0)
      ddc_report_display_by_dref(dref, depth);

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, rc, "");
   return rc;
}


DDCA_Status
ddca_validate_display_ref(DDCA_Display_Ref ddca_dref, bool require_not_asleep)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "ddca_dref = %p", ddca_dref);
   assert(library_initialized);

   Display_Ref * dref = NULL;
   DDCA_Status rc = DDCRC_ARG;
   if (ddca_dref) {
      // rc = ddci_validate_ddca_display_ref(ddca_dref, /* basic_only*/ false,  require_not_asleep, &dref);
      Dref_Validation_Options opts = DREF_VALIDATE_EDID;
      if (require_not_asleep)
         opts |= DREF_VALIDATE_AWAKE;
      rc = ddci_validate_ddca_display_ref2(ddca_dref, opts, &dref);
   }
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

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, rc, "");
   return rc;
}


//
// Open and close display
//

/** Options for opening a DDCA_Display_Ref
 *
 *  This is a vestigial remnant of what was once a larger set of options.
 */

typedef enum {
   DDC_OPENOPT_NONE             = 0,
   DDC_OPENOPT_WAIT             = 1,
} DDC_Open_Options;


STATIC
Error_Info *
ddci_open_display3(
      DDCA_Display_Ref      ddca_dref,
      DDC_Open_Options      options,
      DDCA_Display_Handle * dh_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API,
          "ddca_dref=%p, options=0x%02x, dh_loc=%p, on thread %d",
          ddca_dref, options, dh_loc, get_thread_id());

   DBGTRC_NOPREFIX(debug, DDCA_TRC_API,
          "library_initialized=%s, ddc_displays_already_detected() = %s",
          sbool(library_initialized), sbool(ddc_displays_already_detected()));
   TRACED_ASSERT(library_initialized);
   TRACED_ASSERT(ddc_displays_already_detected());

   Display_Ref * dref = NULL;
   *dh_loc = NULL;        // in case of error
   DDCA_Status rc = 0;
   Error_Info * err = NULL;
   Display_Ref * dref0 = dref_from_published_ddca_dref(ddca_dref);
   if (!dref0) {
      err = ERRINFO_NEW(DDCRC_INVALID_DISPLAY, "Unknown display ref");
   }
   else {
      rc = ddci_validate_ddca_display_ref2(
            ddca_dref, DREF_VALIDATE_EDID | DREF_VALIDATE_AWAKE, &dref);
      if (rc) {
         err = ERRINFO_NEW(DDCRC_INVALID_DISPLAY,
               "Invalid display ref, ddci_validate_ddca_display_ref2 returned %s", psc_desc(rc));
      }
      else {
         Display_Handle * dh;
         Call_Options callopts = CALLOPT_NONE;
         if (options & DDC_OPENOPT_WAIT)
            callopts |= CALLOPT_WAIT;
         err = ddc_open_display(dref,  callopts, &dh);
         if (err) {
            char * detail2 = g_strdup_printf("%s, Internal display ref: %s",
                                             err->detail, dref_reprx_t(dref));
            free(err->detail);
            err->detail = detail2;
         }
         else {
            *dh_loc = dh;
         }
      }
   }

   DBGTRC_RET_ERRINFO(debug, DDCA_TRC_API, err, "*dh_loc=%p -> %s", *dh_loc, dh_repr(*dh_loc));
   TRACED_ASSERT_IFF(!err, *dh_loc);
   return err;
}


DDCA_Status
ddca_open_display2(
      DDCA_Display_Ref      ddca_dref,
      bool                  wait,
      DDCA_Display_Handle * dh_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE,
           "ddca_dref=%p, wait=%s, dh_loc=%p, on thread %d",
           ddca_dref, SBOOL(wait), dh_loc, get_thread_id());
   API_PRECOND_W_EPILOG(dh_loc);

   Error_Info * err = ddci_open_display3(ddca_dref,
                             (wait) ? DDC_OPENOPT_WAIT : DDC_OPENOPT_NONE,
                             dh_loc);
   int ddcrc = 0;
   if (err) {
      ddcrc = err->status_code;
      DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(err);
      errinfo_free_with_report(err, debug, __func__);
      save_thread_error_detail(public_error_detail);
   }

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc,
                        "*dh_loc=%p -> %s", *dh_loc, dh_repr(*dh_loc));
   TRACED_ASSERT_IFF(ddcrc==0, *dh_loc);
   return ddcrc;

}


DDCA_Status
ddca_close_display(DDCA_Display_Handle ddca_dh) {
   bool debug = false;
   free_thread_error_detail();
   DDCA_Status rc = 0;
   Error_Info * err = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   API_PROLOGX(debug, NORESPECT_QUIESCE, "dh = %s", dh_repr(dh));
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  {
         err = ERRINFO_NEW(DDCRC_ARG, "Invalid display handle");
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

   API_EPILOG_BEFORE_RETURN(debug, NORESPECT_QUIESCE, rc, "");
   return rc;
}


//
// Display Handle
//

const char *
ddci_dh_repr(DDCA_Display_Handle ddca_dh) {
   char * repr = NULL;
   Display_Handle * dh = (Display_Handle *) ddca_dh;
   if (valid_display_handle(dh))
      repr = dh_repr(dh);
   return repr;
}


const char *
ddca_dh_repr(DDCA_Display_Handle ddca_dh) {
	return ddci_dh_repr(ddca_dh);
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
   bool debug = false;
   API_PROLOGX(debug, true, "");
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
   API_EPILOG_BEFORE_RETURN(debug, true, rc, "");
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

STATIC
 void ddci_init_display_info(Display_Ref * dref, DDCA_Display_Info * curinfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "dref=%s, curinfo=%p", dref_reprx_t(dref),curinfo);
   memcpy(curinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4);
   curinfo->dispno        = dref->dispno;

   curinfo->path = dref->io_path;
   if (dref->io_path.io_mode == DDCA_IO_USB) {
      curinfo->usb_bus    = dref->usb_bus;
      curinfo->usb_device = dref->usb_device;
   }

   DDCA_MCCS_Version_Spec vspec = DDCA_VSPEC_UNKNOWN;
   if (dref->dispno > 0 && (dref->flags&DREF_DDC_COMMUNICATION_WORKING)) {
      vspec = get_vcp_version_by_dref(dref);
   }
   memcpy(   curinfo->edid_bytes, dref->pedid->bytes, 128);
   G_STRLCPY(curinfo->mfg_id,     dref->pedid->mfg_id,       EDID_MFG_ID_FIELD_SIZE);
   G_STRLCPY(curinfo->model_name, dref->pedid->model_name,   EDID_MODEL_NAME_FIELD_SIZE);
   G_STRLCPY(curinfo->sn,         dref->pedid->serial_ascii, DDCA_EDID_SN_ASCII_FIELD_SIZE);
   curinfo->product_code   = dref->pedid->product_code;
   curinfo->vcp_version    = vspec;
   curinfo->dref           = dref_to_ddca_dref(dref);

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

   DBGTRC_DONE(debug, DDCA_TRC_API, "dref=%s", dref_reprx_t(dref));
}


STATIC
DDCA_Drm_Connector_Found_By drm_to_ddca_connector_found_by(Drm_Connector_Found_By drm_value) {
   DDCA_Drm_Connector_Found_By ddca_value = DDCA_DRM_CONNECTOR_NOT_FOUND;  // pointless initialization to avoid compiler warning
   assert(drm_value != DRM_CONNECTOR_NOT_CHECKED);
   switch (drm_value) {
   case DRM_CONNECTOR_NOT_FOUND:      ddca_value = DDCA_DRM_CONNECTOR_NOT_FOUND;      break;
   case DRM_CONNECTOR_FOUND_BY_BUSNO: ddca_value = DDCA_DRM_CONNECTOR_FOUND_BY_BUSNO; break;
   case DRM_CONNECTOR_FOUND_BY_EDID:  ddca_value = DDCA_DRM_CONNECTOR_FOUND_BY_EDID;  break;
   case DRM_CONNECTOR_NOT_CHECKED:    assert(false);
   }
   return ddca_value;
}


STATIC
 void ddci_init_display_info2(Display_Ref * dref, DDCA_Display_Info2 * curinfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "dref=%s, curinfo=%p", dref_reprx_t(dref),curinfo);
   memcpy(curinfo->marker, DDCA_DISPLAY_INFO_MARKER, 4);
   curinfo->dispno        = dref->dispno;

   curinfo->path = dref->io_path;
   if (dref->io_path.io_mode == DDCA_IO_USB) {
      curinfo->usb_bus    = dref->usb_bus;
      curinfo->usb_device = dref->usb_device;
   }

   DDCA_MCCS_Version_Spec vspec = DDCA_VSPEC_UNKNOWN;
   if (dref->dispno > 0 && (dref->flags&DREF_DDC_COMMUNICATION_WORKING)) {
      vspec = get_vcp_version_by_dref(dref);
   }
   memcpy(curinfo->edid_bytes,    dref->pedid->bytes, 128);
   G_STRLCPY(curinfo->mfg_id,     dref->pedid->mfg_id,       EDID_MFG_ID_FIELD_SIZE);
   G_STRLCPY(curinfo->model_name, dref->pedid->model_name,   EDID_MODEL_NAME_FIELD_SIZE);
   G_STRLCPY(curinfo->sn,         dref->pedid->serial_ascii, DDCA_EDID_SN_ASCII_FIELD_SIZE);
   curinfo->product_code  = dref->pedid->product_code;
   curinfo->vcp_version   = vspec;
   curinfo->dref          = dref_to_ddca_dref(dref);

   // Additional fields in DDCA_Display_Info2 but not DDCA_Display_Info

   if (dref->io_path.io_mode == DDCA_IO_I2C) {
      I2C_Bus_Info * businfo = dref->detail;
      if (businfo->drm_connector_name) {
         G_STRLCPY(curinfo->drm_card_connector, businfo->drm_connector_name, DDCA_DRM_CONNECTOR_FIELD_SIZE);
         curinfo->drm_card_connector_found_by =  drm_to_ddca_connector_found_by(businfo->drm_connector_found_by);
         curinfo->drm_connector_id = businfo->drm_connector_id;
      }
      else {
         curinfo->drm_card_connector[0] = '\0';
         curinfo->drm_card_connector_found_by = DDCA_DRM_CONNECTOR_NOT_FOUND;
         curinfo->drm_connector_id = -1;
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_API, "dref=%s", dref_reprx_t(dref));
}


DDCA_Status
ddca_get_display_info(
      DDCA_Display_Ref  ddca_dref,
      DDCA_Display_Info ** dinfo_loc)
{
   bool debug = false;

   Display_Ref * dref0 = dref_from_published_ddca_dref(ddca_dref);

   // causes return DDCRC_UNINTIALIZED: called after explicit ddca_init()/init2() call failed
   API_PROLOGX(debug, RESPECT_QUIESCE, "ddca_dref=%p, dref0=%s", ddca_dref, dref_reprx_t(dref0));
   // causes return DDCRC_ARG if dinfo_loc == NULL
   API_PRECOND_W_EPILOG(dinfo_loc);
   DDCA_Status ddcrc = 0;

   // if ddc_validate_display_ref() fails, returns its status code
   WITH_VALIDATED_DR4(
         ddca_dref, ddcrc, DREF_VALIDATE_EDID | DREF_VALIDATE_DDC_COMMUNICATION_FAILURE_OK,
         {
            DDCA_Display_Info * info = calloc(1, sizeof(DDCA_Display_Info));
            ddci_init_display_info(dref, info);
            *dinfo_loc = info;
         }
   )

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc, "ddca_dref=%p, dref=%s", ddca_dref, dref_reprx_t(dref0));
   return ddcrc;
}



DDCA_Status
ddca_get_display_info2(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Info2 ** dinfo_loc)
{
   bool debug = false;

   Display_Ref * dref0 = dref_from_published_ddca_dref(ddca_dref);

   // causes return DDCRC_UNINTIALIZED: called after explicit ddca_init()/init2() call failed
   API_PROLOGX(debug, RESPECT_QUIESCE, "ddca_dref=%p, dref0=%s", ddca_dref, dref_reprx_t(dref0));
   // causes return DDCRC_ARG if dinfo_loc == NULL
   API_PRECOND_W_EPILOG(dinfo_loc);
   DDCA_Status ddcrc = 0;

   // if ddc_validate_display_ref() fails, returns its status code
   WITH_VALIDATED_DR4(
         ddca_dref, ddcrc, DREF_VALIDATE_EDID | DREF_VALIDATE_DDC_COMMUNICATION_FAILURE_OK,
         {
            DDCA_Display_Info2 * info = calloc(1, sizeof(DDCA_Display_Info2));
            ddci_init_display_info2(dref, info);
            *dinfo_loc = info;
         }
   )

   API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, ddcrc, "ddca_dref=%p, dref=%s", ddca_dref, dref_reprx_t(dref0));
   return ddcrc;
}


STATIC void
set_ddca_error_detail_from_open_errors() {
   bool debug = false;
   GPtrArray * errs = ddc_get_bus_open_errors();
   // DDCA_Status master_rc = 0;
   if (errs && errs->len > 0) {
      Error_Info * master_error = ERRINFO_NEW(DDCRC_OTHER, "Error(s) opening ddc devices");
      MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Error(s) opening ddc devices");
      for (int ndx = 0; ndx < errs->len; ndx++) {
         Bus_Open_Error * cur = g_ptr_array_index(errs, ndx);
         Error_Info * errinfo = NULL;
         if (cur->io_mode == DDCA_IO_I2C) {
            errinfo = ERRINFO_NEW(cur->error, "Error %s opening /dev/i2c-%d",
                                             psc_desc(cur->error), cur->devno);
            MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Error %s opening /dev/i2c-%d",
                                             psc_desc(cur->error), cur->devno);
         }
         else {
            errinfo = ERRINFO_NEW(cur->error, "Error %s opening /dev/usb/hiddev%d %s",
                                             psc_desc(cur->error), cur->devno, (cur->detail) ? cur->detail : "");
            MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Error %s opening /dev/usb/hiddev%d %s",
                  psc_desc(cur->error), cur->devno, (cur->detail) ? cur->detail : "");
         }
         errinfo_add_cause(master_error, errinfo);
      }
      // master_rc = master_error->status_code;
      DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(master_error);
      errinfo_free_with_report(master_error, debug, __func__);
      save_thread_error_detail(public_error_detail);
   }
   // return master_rc;
}


DDCA_Status
ddca_get_display_refs(
      bool                include_invalid_displays,
      DDCA_Display_Ref**  drefs_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "include_invalid_displays=%s", SBOOL(include_invalid_displays));
   API_PRECOND_W_EPILOG(drefs_loc);

   int dref_ct = 0;
   DDCA_Status ddcrc = 0;
   ddc_ensure_displays_detected();
   GPtrArray * filtered_displays = ddc_get_filtered_display_refs(
                                      include_invalid_displays,
                                      false);  // include_removed_drefs
   DDCA_Display_Ref * result_list = calloc(filtered_displays->len + 1,sizeof(DDCA_Display_Ref));
   DDCA_Display_Ref * cur_ddca_dref = result_list;
   for (int ndx = 0; ndx < filtered_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(filtered_displays, ndx);
         *cur_ddca_dref = dref_to_ddca_dref(dref);
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%p -> %p", cur_ddca_dref, *cur_ddca_dref);
         add_published_dref_id_by_dref(dref);
         cur_ddca_dref++;
   }
   *cur_ddca_dref = NULL; // terminating NULL ptr, redundant since calloc()
   dref_ct = filtered_displays->len;
   g_ptr_array_free(filtered_displays, true);


   if (IS_DBGTRC(debug, DDCA_TRC_API|DDCA_TRC_DDC )) {
      DBGMSG("          *drefs_loc=%p", drefs_loc);
      DDCA_Display_Ref * cur_ddca_dref = result_list;
      while (*cur_ddca_dref) {
         Display_Ref * dref = dref_from_published_ddca_dref(*cur_ddca_dref);
         DBGMSG("          DDCA_Display_Ref %p -> display %d", *cur_ddca_dref, dref->dispno);
         cur_ddca_dref++;
      }
      dbgrpt_published_dref_hash(__func__, 1);
   }

   *drefs_loc = result_list;
   assert(*drefs_loc);

   set_ddca_error_detail_from_open_errors();
   ddcrc = 0;

   API_EPILOG_RET_DDCRC(debug, RESPECT_QUIESCE, ddcrc, "*drefs_loc=%p, returned list has %d displays",
         *drefs_loc, dref_ct);
}


DDCA_Status
ddca_get_display_info_list2(
      bool                      include_invalid_displays,
      DDCA_Display_Info_List**  dlist_loc)
{
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "");

   int filtered_ct = 0;
   API_PRECOND_W_EPILOG(dlist_loc);

   DDCA_Status ddcrc = 0;
   ddc_ensure_displays_detected();
   GPtrArray * filtered_displays = ddc_get_filtered_display_refs(
                                      include_invalid_displays,
                                      false);  // include_removed_drefs
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
      ddci_init_display_info(dref, curinfo);
      add_published_dref_id_by_dref(dref);
      curinfo++;
   }
   g_ptr_array_free(filtered_displays, true);

   if (IS_DBGTRC(debug, DDCA_TRC_API|DDCA_TRC_DDC )) {
      DBGMSG("Final result list %p", result_list);
      dbgrpt_display_info_list(result_list, 2);
      dbgrpt_published_dref_hash(__func__, 1);
   }

   set_ddca_error_detail_from_open_errors();
   ddcrc = 0;
   *dlist_loc = result_list;
   assert(*dlist_loc);

   API_EPILOG_RET_DDCRC(debug, RESPECT_QUIESCE, ddcrc, "Returned list has %d displays", filtered_ct);
}


void
ddca_free_display_info(DDCA_Display_Info * info_rec) {
   bool debug = false;
   API_PROLOG_NO_DISPLAY_IO(debug, "info_rec=%p", info_rec);
   if (info_rec && memcmp(info_rec->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0) {
      // DDCA_IO_Path path = info_rec->path;
      // DDCA_Display_Info contains no pointers, can simply be free'd.
      info_rec->marker[3] = 'x';
      free(info_rec);
   }
   API_EPILOG_NO_RETURN(debug, false, "");
   DISABLE_API_CALL_TRACING();
}


void
ddca_free_display_info2(DDCA_Display_Info2 * info_rec) {
   bool debug = false;
   API_PROLOG_NO_DISPLAY_IO(debug, "info_rec=%p", info_rec);
   if (info_rec && memcmp(info_rec->marker, DDCA_DISPLAY_INFO_MARKER, 4) == 0) {
      // DDCA_Display_Info contains no pointers, can simply be free'd
      info_rec->marker[3] = 'x';
      free(info_rec);
   }
   API_EPILOG_NO_RETURN(debug, false, "");
   DISABLE_API_CALL_TRACING();
}


void
ddca_free_display_info_list(DDCA_Display_Info_List * dlist) {
   bool debug = false;
   API_PROLOG_NO_DISPLAY_IO(debug, "dlist=%p", dlist);
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
   API_EPILOG_NO_RETURN(debug, false, "");
   // DBGTRC_DONE(debug, DDCA_TRC_API, "");
   DISABLE_API_CALL_TRACING();
}


static DDCA_Status
ddci_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   bool debug = false;
   API_PROLOGX(debug, NORESPECT_QUIESCE, "dinfo=%p, dinfo->dispno=%d, depth=%d",
                                         dinfo, dinfo->dispno, depth);
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
      int tw = 22;   // title width
      switch(dinfo->path.io_mode) {
      case (DDCA_IO_I2C):
            rpt_vstring(d1, "%-*s /dev/i2c-%d", tw, "I2C bus:", dinfo->path.path.i2c_busno);
            break;
      case (DDCA_IO_USB):
            rpt_vstring(d1, "%-*s %d.%d", tw, "USB bus device:",
                            dinfo->usb_bus, dinfo->usb_device);
            rpt_vstring(d1, "%-*s /dev/usb/hiddev%d", tw, "USB hiddev device:", dinfo->path.path.hiddev_devno);
            break;
      }

      // workaround, including drm_connector in DDCA_Display_Info would break API
      Display_Ref * dref = dref_from_published_ddca_dref(dinfo->dref);
      if (dref) {   // should never fail, but just in case
         if (dref->drm_connector_id > 0)
            // rpt_vstring(d1, "%-*s %d", tw, "DRM connector id:", dref->drm_connector_id);
            rpt_vstring(d1, "%-*s %s (id: %d)",  tw, "DRM connector:", dref->drm_connector, dref->drm_connector_id);
         else
            rpt_vstring(d1, "%-*s %s",  tw, "DRM connector:", dref->drm_connector);
         // TMI:
         // rpt_vstring(d1, "%-*s %s",  tw, "DRM connector found by:",  drm_connector_found_by_public_name(dref->drm_connector_found_by));
      }

      rpt_vstring(d1, "%-*s %s", tw, "Mfg id:",dinfo->mfg_id);
      rpt_vstring(d1, "%-*s %s", tw, "Model:", dinfo->model_name);
      rpt_vstring(d1, "%-*s %u", tw, "Product code:", dinfo->product_code);
      rpt_vstring(d1, "%-*s %s", tw, "Serial number:",  dinfo->sn);

      // binary SN is not part of DDCA_Display_Info
      Parsed_Edid * edid = create_parsed_edid(dinfo->edid_bytes);
      if (edid) {     // should never fail, but being ultra-cautious
         // Binary serial number is typically 0x00000000 or 0x01010101, but occasionally
         // useful for differentiating displays that share a generic ASCII "serial number"
         rpt_vstring(d1,"%-*s %"PRIu32" (0x%08x)", tw, "Binary serial number:",
                        edid->serial_binary, edid->serial_binary);
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
      GPtrArray * edid_lines = g_ptr_array_new_with_free_func(g_free);
      hex_dump_indented_collect(edid_lines, dinfo->edid_bytes, 128, 0);
      for (int ndx = 0; ndx < edid_lines->len; ndx++) {
         rpt_vstring(d2, "%s", (char *) g_ptr_array_index(edid_lines, ndx));
      }
      g_ptr_array_free(edid_lines, true);

      // OLD: rpt_hex_dump(dinfo->edid_bytes, 128, d2);

      // rpt_vstring(d1, "dref:                %p", dinfo->dref);
      rpt_vstring(d1, "%-*s %s", tw, "VCP Version:", format_vspec(dinfo->vcp_version));
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
   API_EPILOG_RET_DDCRC(debug, NORESPECT_QUIESCE, rc, "");
}


DDCA_Status
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
	return ddci_report_display_info(dinfo, depth);
}


DDCA_Status
ddca_report_display_info2(
      DDCA_Display_Info2 * dinfo,
      int                  depth)
{
   return ddci_report_display_info((DDCA_Display_Info*) dinfo, depth);
}


STATIC void
dbgrpt_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dinfo=%p");
   ddci_report_display_info(dinfo, depth);
   int d1 = depth+1;

   rpt_vstring(d1, "dref:                %s", dref_repr_t(dinfo->dref));
   if (dinfo->dref) {  // paranoid, should never be NULL
      rpt_vstring(d1, "VCP Version (dref xdf): %s", format_vspec_verbose(((Display_Ref*)dinfo->dref)->vcp_version_xdf));
   }
   DBGMSF(debug, "Done.");
}

#ifdef UNUSED
STATIC void
dbgrpt_display_info2(
      DDCA_Display_Info2 * dinfo,
      int                  depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dinfo=%p");
   ddci_report_display_info((DDCA_Display_Info*)dinfo, depth);
   int d1 = depth+1;

   rpt_vstring(d1, "dref:                %s", dref_repr_t(dinfo->dref));
   if (dinfo->dref) {  // paranoid, should never be NULL
      rpt_vstring(d1, "VCP Version (dref xdf): %s", format_vspec_verbose(((Display_Ref*)dinfo->dref)->vcp_version_xdf));
   }
   DBGMSF(debug, "Done.");
}
#endif


void
ddca_report_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth)
{
   bool debug = false;
   API_PROLOG_NO_DISPLAY_IO(debug, "");
   DBGMSF(debug, "Starting.  dlist=%p, depth=%d", dlist, depth);

   int d1 = depth+1;
   rpt_vstring(depth, "Found %d displays", dlist->ct);
   for (int ndx=0; ndx<dlist->ct; ndx++) {
      ddci_report_display_info(&dlist->info[ndx], d1);
   }
   API_EPILOG_NO_RETURN(debug, false, "");
}


STATIC void
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
// Reports
//

// TODO: deprecate, does not respect quiesced
int
ddca_report_displays(bool include_invalid_displays, int depth) {
   bool debug = false;
   API_PROLOG(debug, "");
   int display_ct = 0;
   if (!library_initialization_failed) {
      display_ct = ddc_report_displays(include_invalid_displays, depth);
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_API, "Returning: %d", display_ct);
   DISABLE_API_CALL_TRACING();
   API_EPILOG_NO_RETURN(debug, false, ""); // hack
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
   API_PROLOGX(debug, RESPECT_QUIESCE, "func=%p", func);

#ifdef WATCH_DISPLAYS
   DDCA_Status result = DDCRC_INVALID_OPERATION;
 #ifdef ENABLE_UDEV
    if (check_all_video_adapters_implement_drm())
       result = dw_register_display_status_callback(func);
 #endif
#else
    DDCA_Status result = DDCRC_UNIMPLEMENTED;
#endif

   API_EPILOG_RET_DDCRC(debug, RESPECT_QUIESCE, result, "func=%p", func);
   return result;
}


DDCA_Status
ddca_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func) {
   bool debug = false;
   free_thread_error_detail();
   API_PROLOGX(debug, RESPECT_QUIESCE, "func=%p", func);

#ifdef WATCH_DISPLAYS
   DDCA_Status result = dw_unregister_display_status_callback(func);
#else
   DDCA_Status result = DDCRC_UNIMPLEMENTED;
#endif

   API_EPILOG_RET_DDCRC(debug, RESPECT_QUIESCE, result, "func=%p", func);
   return result;
}


const char *
   ddca_display_event_type_name(DDCA_Display_Event_Type event_type) {
#ifdef WATCH_DISPLAYS
      return dw_display_event_type_name(event_type);
#else
      return NULL;
#endif
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
    API_PROLOGX(debug, RESPECT_QUIESCE, "ddca_dref=%p", ddca_dref);

    assert(library_initialized);
    Display_Ref * dref = NULL;
     //DDCA_Status rc = ddci_validate_ddca_display_ref(ddca_dref, /* basic_only*/ true, /*require_not_asleep*/false, &dref);
    DDCA_Status rc = ddci_validate_ddca_display_ref2(ddca_dref,  DREF_VALIDATE_EDID,  &dref);
    if (rc == 0)  {
       Per_Display_Data * pdd = dref->pdd;
       if (multiplier >= 0.0 && multiplier <= 10.0) {
          pdd_reset_multiplier(pdd, multiplier);
       }
       else
          rc = DDCRC_ARG;
    }
    API_EPILOG_BEFORE_RETURN(debug, RESPECT_QUIESCE, rc, "");
    return rc;
}


DDCA_Status
ddca_get_current_display_sleep_multiplier(
      DDCA_Display_Ref        ddca_dref,
      DDCA_Sleep_Multiplier*  multiplier_loc)
{
   bool debug = false;
   free_thread_error_detail();
    API_PROLOGX(debug, NORESPECT_QUIESCE, "ddca_dref=%p", ddca_dref);

    assert(library_initialized);
    Display_Ref * dref = NULL;
    // DDCA_Status rc = ddci_validate_ddca_display_ref(ddca_dref, true, false, &dref);
    DDCA_Status rc = ddci_validate_ddca_display_ref2(ddca_dref,  DREF_VALIDATE_EDID,  &dref);
    if (rc == 0) {
       Per_Display_Data * pdd = dref->pdd;
       *multiplier_loc        = pdd->final_successful_adjusted_sleep_multiplier;
    }
    API_EPILOG_BEFORE_RETURN(debug, NORESPECT_QUIESCE, rc, "");
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

   API_EPILOG_NO_RETURN(debug, false, "Returning %s", sbool(old));
   return old;
}


bool ddca_is_dynamic_sleep_enabled()
{
   bool debug = false;
   API_PROLOG(debug, "");
   free_thread_error_detail();

   bool result = pdd_is_dynamic_sleep_enabled();

   API_EPILOG_NO_RETURN(debug, false, "Returning %s", sbool(result));
   return result;
}


//
// Module initialization
//

void init_api_displays() {
   RTTI_ADD_FUNC(ddca_close_display);
   RTTI_ADD_FUNC(ddca_get_display_info_list2);
   RTTI_ADD_FUNC(ddca_get_display_info);
   RTTI_ADD_FUNC(ddca_get_display_info2);
   RTTI_ADD_FUNC(ddci_get_display_ref);
   RTTI_ADD_FUNC(ddca_get_display_ref);
   RTTI_ADD_FUNC(ddca_create_display_ref);
   RTTI_ADD_FUNC(ddca_get_display_refs);
   RTTI_ADD_FUNC(ddca_open_display2);
   RTTI_ADD_FUNC(ddci_open_display3);
   RTTI_ADD_FUNC(ddca_redetect_displays);
   RTTI_ADD_FUNC(ddca_report_display_by_dref);
   RTTI_ADD_FUNC(ddca_register_display_status_callback);
   RTTI_ADD_FUNC(ddca_unregister_display_status_callback);
   RTTI_ADD_FUNC(ddci_init_display_info);
   RTTI_ADD_FUNC(ddci_init_display_info2);
#ifdef OLD
   RTTI_ADD_FUNC(ddci_validate_ddca_display_ref);
#endif
   RTTI_ADD_FUNC(ddci_validate_ddca_display_ref2);
   RTTI_ADD_FUNC(ddca_validate_display_ref);
}

