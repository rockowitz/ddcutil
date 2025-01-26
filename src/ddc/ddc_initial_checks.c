/** @file ddc_initial_checks.c */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 
/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#ifdef USE_X11
#include <X11/extensions/dpmsconst.h>
#endif

#include "config.h"

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#ifdef USE_X11
#include "util/x11_util.h"
#endif
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_packets.h"
#include "base/dsa2.h"
#include "base/i2c_bus_base.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_base.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_initial_checks.h"


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;

//
// Globals
//

bool skip_ddc_checks = false;
bool monitor_state_tests = false;


//
// Utility Functions
//

static inline bool
value_bytes_zero_for_any_value(DDCA_Any_Vcp_Value * pvalrec) {
   assert(pvalrec);
   bool result = pvalrec && pvalrec->value_type ==  DDCA_NON_TABLE_VCP_VALUE &&
                 pvalrec->val.c_nc.mh == 0 &&
                 pvalrec->val.c_nc.ml == 0 &&
                 pvalrec->val.c_nc.sh == 0 &&
                 pvalrec->val.c_nc.sl == 0;
   return result;
}


static inline bool
value_bytes_zero_for_nontable_value(Parsed_Nontable_Vcp_Response* valrec) {
   assert(valrec);
   bool result =  valrec->mh == 0 && valrec->ml == 0 && valrec->sh == 0 && valrec->sl == 0;
   return result;
}


//
// Monitor Checks
//

/** Attempt to read a non-table feature code that should never be valid.
 *  Check that it is in fact reported as unsupported.
 *
 *  @param  dh            Display Handle
 *  @param  feature code  VCP feature code
 *  @return Error_Info    if unsupported, NULL if supported
 *
 *  @remark
 *  Possible return settings
 *  Error_Info.status = DDCRC_DETERMINED_UNSUPPORTED, set DREF_DDC_USES_MH_ML_SHL_SL_ZERO_FOR_UNSUPPORTED
 *  Error_Info.status = DDCRC_ALL_RESPONSES_NULL, set DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED
 *  Error_Info.status = DDCRC_RETRIES
 *
 */
STATIC Error_Info *
read_unsupported_feature(
      Display_Handle * dh,
      DDCA_Vcp_Feature_Code feature_code)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s. feature_code=0x%02x", dh_repr(dh), feature_code);
   I2C_Bus_Info * businfo = (I2C_Bus_Info *) dh->dref->detail;
   Per_Display_Data * pdd = dh->dref->pdd;
   Parsed_Nontable_Vcp_Response * parsed_response_loc = NULL;
   // turns off possible abbreviated NULL msg handling in ddc_write_read_with_retry()
   dh->testing_unsupported_feature_active = true;
   bool dynamic_sleep_was_active = false;

   Error_Info * ddc_excp = ddc_get_nontable_vcp_value(dh, feature_code, &parsed_response_loc);

   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "busno=%d,  sleep-multiplier=%5.2f, ddc_get_nontable_vcp_value() for feature 0x%02x returned: %s",
            businfo->busno, pdd_get_adjusted_sleep_multiplier(pdd),
            feature_code, errinfo_summary(ddc_excp));
retry:
   if (!ddc_excp) {
      if (value_bytes_zero_for_nontable_value(parsed_response_loc)) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Setting DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED");
         dh->dref->flags |= DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED;
         ddc_excp = ERRINFO_NEW(DDCRC_DETERMINED_UNSUPPORTED, "Set DREF_DDC_USES_MH_ML_SH_SL for unsupported");
      }
      else {
         if (get_output_level() >= DDCA_OL_VERBOSE)
            rpt_vstring(0, "/dev/i2c-%d, Feature 0x%02x should not exist, but the monitor reports it as valid",
                  businfo->busno, feature_code);
         SYSLOG2(DDCA_SYSLOG_WARNING,
            "busno=%d, Feature 0x%02x should not exist but ddc_get_nontable_vcp_value() succeeds,"
            " returning mh=0x%02x ml=0x%02x sh=0x%02x sl=0x%02x",
            businfo->busno, feature_code,
            parsed_response_loc->mh, parsed_response_loc->ml,
            parsed_response_loc->sh, parsed_response_loc->ml);
      }
   }
   else if ( ERRINFO_STATUS(ddc_excp) == DDCRC_RETRIES ) {
      if (errinfo_all_causes_same_status(ddc_excp, DDCRC_NULL_RESPONSE)) {
         errinfo_free(ddc_excp);
         ddc_excp = ERRINFO_NEW(DDCRC_ALL_RESPONSES_NULL, "");
         dh->dref->flags |= DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED;
      }
      else {
         if (!dynamic_sleep_was_active) {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                  "busno=%d, sleep-multiplier=%d, Testing for unsupported feature 0x%02x returned %s",
                  businfo->busno,  pdd_get_adjusted_sleep_multiplier(pdd),
                  feature_code, errinfo_summary(ddc_excp));
            SYSLOG2(DDCA_SYSLOG_ERROR,
                  "busno=%d, sleep-multiplier=%5.2f, Testing for unsupported feature 0x%02x returned %s",
                  businfo->busno,  pdd_get_adjusted_sleep_multiplier(pdd),
                  feature_code, errinfo_summary(ddc_excp));
         }
         if (pdd_is_dynamic_sleep_active(pdd) ) {
            dynamic_sleep_was_active = true;
            ERRINFO_FREE(ddc_excp);
            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Turning off dynamic sleep and retrying");
            SYSLOG2(DDCA_SYSLOG_ERROR, "Turning off dynamic sleep and retrying");
            pdd_set_dynamic_sleep_active(pdd, false);
            ddc_excp = ddc_get_nontable_vcp_value(dh, feature_code, &parsed_response_loc);
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                  "busno=%d, sleep-multiplier=%5.2f, Retesting for unsupported feature 0x%02x returned %s",
                  businfo->busno,   pdd_get_adjusted_sleep_multiplier(pdd),
                  feature_code, errinfo_summary(ddc_excp));
            SYSLOG2(DDCA_SYSLOG_ERROR,
                  "busno=%d, sleep-multiplier =%5.2f, Retesting for unsupported feature 0x%02x returned %s",
                  businfo->busno,
                  pdd_get_adjusted_sleep_multiplier(pdd),
                  feature_code, errinfo_summary(ddc_excp));
            goto retry;
         }
      }
   }
   if (dynamic_sleep_was_active)
      pdd_set_dynamic_sleep_active(pdd, true);
   dh->testing_unsupported_feature_active = false;
   free(parsed_response_loc);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "");
   return ddc_excp;
}


/** Determines how an unsupported non-table feature is reported.
 *
 *  @param  dh   Display Handle
 *
 *  Sets relevant DREF_DDC_* flags in the associated Display Reference to
 *  indicate how unsupported features are reported.
 *  Possible values:
 *     DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED
 *     DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED
 *     DREF_DDC_USES_MH_ML_SHL_SL_ZERO_FOR_UNSUPPORTED
 *     DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED
 */
STATIC void
check_how_unsupported_reported(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr(dh));
   Display_Ref* dref = dh->dref;
   I2C_Bus_Info * businfo = (I2C_Bus_Info *) dref->detail;
   assert(dref->io_path.io_mode == DDCA_IO_I2C);

   // Try features that should never exist
   Error_Info * erec = read_unsupported_feature(dh, 0xdd);  // not defined in MCCS
   if ((!erec || ERRINFO_STATUS(erec) == DDCRC_RETRIES) &&
       is_input_digital(dh->dref->pedid))
   {
      ERRINFO_FREE(erec);
      erec = read_unsupported_feature(dh, 0x41);    // CRT only feature
   }
   if (!erec || ERRINFO_STATUS(erec) == DDCRC_RETRIES) {
      ERRINFO_FREE(erec);
      erec = read_unsupported_feature(dh, 0x00);
   }

   Public_Status_Code psc = ERRINFO_STATUS(erec);

   if (psc == 0) {
      dh->dref->flags |= DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED;
      SYSLOG2(DDCA_SYSLOG_ERROR,
            "busno=%d, All features that should not exist detected. "
            "Monitor does not indicate unsupported", businfo->busno);
   }
   else {
      if (psc == DDCRC_RETRIES) {
            dref->flags |= DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED;   // our best guess
            SYSLOG2(DDCA_SYSLOG_ERROR,
                  "busno=%d, DDCRC_RETRIES failure reading all unsupported features. "
                  "Setting DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED",
                  businfo->busno);
      }

      else if (psc == DDCRC_DETERMINED_UNSUPPORTED) {
         // already handled in read_unsupported_feature()
      }

      else if (psc == DDCRC_REPORTED_UNSUPPORTED) {   // the monitor is well-behaved
         dref->flags |= DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED;
      }

      else if ( (psc == DDCRC_NULL_RESPONSE || psc == DDCRC_ALL_RESPONSES_NULL) &&
            !ddc_never_uses_null_response_for_unsupported) {      // for testing
         // Null Msg really means unsupported
         dref->flags |= DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED;
      }

      // What if returns -EIO?  Dell AW3418D returns -EIO for unsupported features
      // EXCEPT that it returns mh=ml=sh=sl=0 for feature 0x00  (2/2019)
      // Too dangerous to always treat -EIO as unsupported
      else if (psc == -EIO) {
         MSG_W_SYSLOG(DDCA_SYSLOG_WARNING,
               "busno=%d. Monitor apparently returns -EIO for unsupported features. This cannot be relied on.",
               businfo->busno);
      }
   }
   errinfo_free(erec);
   dh->dref->flags |= DREF_UNSUPPORTED_CHECKED;
#ifdef OUT   // EIO case fails this assertion
   assert(dh->dref->flags & (DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED         |
                             DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED    |
                             DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED |
                             DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED ) );
#endif
   DBGTRC_DONE(debug, TRACE_GROUP, "dref->flags=%s", interpret_dref_flags_t(dref->flags));
}


STATIC Error_Info *
check_supported_feature(Display_Handle * dh, bool newly_added, DDCA_Vcp_Feature_Code feature_code, uint16_t * p_shsl) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, newly_added=%s feature=0x%02x, p_shsl=%p",
         dh_repr(dh), SBOOL(newly_added), feature_code,p_shsl);

   Error_Info * ddc_excp = NULL;
   *p_shsl = 0;

   Per_Display_Data * pdd = dh->dref->pdd;
   Display_Ref * dref = dh->dref;
   I2C_Bus_Info * businfo = dh->dref->detail;

   DDCA_Sleep_Multiplier initial_multiplier = pdd_get_adjusted_sleep_multiplier(pdd);
   Parsed_Nontable_Vcp_Response* parsed_response_loc = NULL;
   // feature that always exists
   // Byte feature_code = 0x10;
   ddc_excp = ddc_get_nontable_vcp_value(dh, feature_code, &parsed_response_loc);
   // may return DDCRC_DISCONNECTED from i2c_check_open_bus_alive()
   if (!ddc_excp) {
      *p_shsl = HI_LO_BYTES_TO_SHORT(parsed_response_loc->sh, parsed_response_loc->sl);
   }

#ifdef TESTING
   if (businfo->busno == 6) {
      ddc_excp = ERRINFO_NEW(DDCRC_BAD_DATA, "Dummy error");
      DBGMSG("Setting dummy ddc_excp(DDCRC_BAD_DATA)");
   }
#endif

   if (ddc_excp) {
      char * msg = g_strdup_printf(
            "busno=%d, sleep-multiplier = %5.2f. Testing for supported feature 0x%02x returned %s",
            businfo->busno,
            pdd_get_adjusted_sleep_multiplier(pdd),
            feature_code,
            errinfo_summary(ddc_excp));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "!!!! %s", msg);
      SYSLOG2(DDCA_SYSLOG_WARNING, "(%s) %s", __func__, msg);
      free(msg);

      dref->communication_error_summary = g_strdup(errinfo_summary(ddc_excp));
      if (ddc_excp->status_code != DDCRC_DISCONNECTED) {
         bool dynamic_sleep_active = pdd_is_dynamic_sleep_active(pdd);
         if (newly_added || (ERRINFO_STATUS(ddc_excp) == DDCRC_RETRIES &&
                                         dynamic_sleep_active &&
                                         initial_multiplier < 1.0f))
         {
            if (newly_added) {
               DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Additional 1 second sleep for newly added display A");
               DW_SLEEP_MILLIS(1000, "Additional 1 second sleep for newly added display C");
            }
            // turn off optimization in case it's on
            if (dynamic_sleep_active ) {
               FREE(dref->communication_error_summary);
               DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Turning off dynamic sleep");
               pdd_set_dynamic_sleep_active(dref->pdd, false);
               ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, TRACE_GROUP));
               ddc_excp = ddc_get_nontable_vcp_value(dh, 0x10, &parsed_response_loc);
               if (!ddc_excp) {
                  *p_shsl = HI_LO_BYTES_TO_SHORT(parsed_response_loc->sh, parsed_response_loc->sl);
               }

               DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                     "busno=%d, sleep-multiplier=%5.2f. "
                     "Retesting for supported feature 0x%02x returned %s",
                     businfo->busno,
                     pdd_get_adjusted_sleep_multiplier(pdd),
                     feature_code,
                     errinfo_summary(ddc_excp));
               dref->communication_error_summary = g_strdup(errinfo_summary(ddc_excp));
               SYSLOG2((ddc_excp) ? DDCA_SYSLOG_ERROR : DDCA_SYSLOG_INFO,
                     "busno=%d, sleep-multiplier=%5.2f."
                     "Retesting for supported feature 0x%02x returned %s",
                     businfo->busno,
                     pdd_get_adjusted_sleep_multiplier(pdd),
                     feature_code,
                     errinfo_summary(ddc_excp));
            }
         }
      }
   }

   // if (businfo->busno == 6) {
   //    ddc_excp = ERRINFO_NEW(DDCRC_BAD_DATA, "Dummy error");
   //    DBGMSG("Setting dummy ddc_excp(DDCRC_BAD_DATA)");
   // }

   free(parsed_response_loc);

   Public_Status_Code psc = ERRINFO_STATUS(ddc_excp);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "ddc_get_nontable_vcp_value() for feature 0x%02x returned: %s, status: %s",
         feature_code, errinfo_summary(ddc_excp), psc_desc(psc));

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "*p_shsl=0x%04x", *p_shsl);
   return ddc_excp;
}


/** Collects initial monitor checks to perform them on a single open of the
 *  monitor device, and to avoid repeating them.
 *
 *  Performs the following tests:
 *  - Checks that DDC communication is working.
 *  - Checks if the monitor uses DDC Null Response to indicate invalid VCP code
 *  - Checks if the monitor uses mh=ml=sh=sl=0 to indicate invalid VCP code
 *
 *  @param dh           pointer to #Display_Handle for open monitor device
 *  @param newly_added  called by display watch when adding a display
 *  @return #Error_Info struct if error, caller responsible for freeing
 *
 *  @remark
 *  Sets bits in dh->dref->flags
 *   *  @remark
 *  It has been observed that DDC communication can fail even if slave address x37
 *  is valid on the I2C bus.
 *  @remark
 *  ADL does not notice that a reported display, e.g. Dell 1905FP, does not support
 *  DDC.
 *  @remark
 *  Monitors are supposed to set the unsupported feature bit in a valid DDC
 *  response, but a few monitors (mis)use the Null Response instead to indicate
 *  an unsupported feature. Others return with the unsupported feature bit not
 *  set, but all bytes (mh, ml, sh, sl) zero.
 *  @remark
 *  Note that the test here is not perfect, as a Null Response might
 *  in fact indicate a transient error, but that is rare.
 *  @remark
 *  Output level should have been set <= DDCA_OL_NORMAL prior to this call since
 *  verbose output is distracting.
 */
STATIC Error_Info *
ddc_initial_checks_by_dh(Display_Handle * dh, bool newly_added) {
   bool debug = false;
   TRACED_ASSERT(dh && dh->dref);
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, newly_added=%s", dh_repr(dh), sbool(newly_added));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Initial flags: %s",interpret_dref_flags_t(dh->dref->flags));

   Display_Ref * dref = dh->dref;
   I2C_Bus_Info * businfo = dref->detail;
   Per_Display_Data * pdd = dref->pdd;
   // bool iomode_is_i2c = dh->dref->io_path.io_mode == DDCA_IO_I2C;

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "adjusted sleep-multiplier = %5.2f",
                                       pdd_get_adjusted_sleep_multiplier(pdd));
   Error_Info * ddc_excp = NULL;

   bool saved_dynamic_sleep_active = pdd_is_dynamic_sleep_active(pdd);

   if (debug)
      show_backtrace(0);

   if (!(dref->flags & DREF_DDC_COMMUNICATION_CHECKED)) {
      // assert(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED);
      assert(businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_CHECKED);
      // if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED)) {
      //    i2c_check_businfo_connector(businfo);
      // }

      int depth = IS_DBGTRC(debug, DDCA_TRC_NONE) ? 1 : -1;
      if (businfo->drm_connector_name) {
         possibly_write_detect_to_status_by_connector_name(businfo->drm_connector_name);
         if (depth > 0)
            rpt_label(0, "Current sysfs attributes:");
         RPT_ATTR_TEXT(depth, NULL, "/sys/class/drm",businfo->drm_connector_name, "dpms");
         RPT_ATTR_TEXT(depth, NULL, "/sys/class/drm",businfo->drm_connector_name, "status");
         RPT_ATTR_TEXT(depth, NULL, "/sys/class/drm",businfo->drm_connector_name, "enabled");
         RPT_ATTR_INT (depth, NULL, "/sys/class/drm",businfo->drm_connector_name, "drm_connector_id");
         bool edid_found = GET_ATTR_EDID(NULL, "/sys/class/drm",businfo->drm_connector_name, "edid");
         rpt_vstring(depth, "/sys/class/drm/%s/edid:                                     %s",
               businfo->drm_connector_name, (edid_found) ? "Found" : "Not found");
         // RPT_ATTR_EDID(depth, NULL, "/sys/class/drm",businfo->drm_connector_name, "edid");
      }

      // DBGMSG("monitor_state_tests = %s", SBOOL(monitor_state_tests));
      if (monitor_state_tests)
         explore_monitor_state(dh);

      if (businfo->flags & I2C_BUS_LVDS_OR_EDP) {
         DBGTRC(debug, TRACE_GROUP, "Laptop display definitely detected, not checking feature x10");
         dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
      }
      else if (!(businfo->flags & I2C_BUS_ADDR_X37)) {
         DBGTRC(debug, TRACE_GROUP, "Slave address x37 not responsive, not checking feature x10");
         dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
      }
      else {
         uint16_t shsl;
         // DDCA_Vcp_Feature_Code fc = (iomode_is_i2c) ? 0xdf : 0x10;
         DDCA_Vcp_Feature_Code fc = 0x10;
         ddc_excp = check_supported_feature(dh, newly_added, fc, &shsl);

         Public_Status_Code psc = ERRINFO_STATUS(ddc_excp);

         if (psc == 0 ||
             psc == DDCRC_REPORTED_UNSUPPORTED ||
             psc == DDCRC_DETERMINED_UNSUPPORTED)
         {
            dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
         }
         else if (psc == DDCRC_DISCONNECTED)
         {
            dref->flags = DREF_REMOVED;
         }
         else if (psc == -EBUSY) {
             // communication failed, do not set DDCRC_COMMUNICATION_WORKING
             dref->flags |= DREF_DDC_BUSY;
          }

         if (psc != -EBUSY) {
            dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
         }

         if ( (dref->flags&DREF_DDC_COMMUNICATION_WORKING) &&
               dref->io_path.io_mode == DDCA_IO_I2C)
         {
            check_how_unsupported_reported(dh);

            if ( i2c_force_bus /* && psc == DDCRC_RETRIES */) {  // used only when testing
               DBGTRC_NOPREFIX(debug || true , TRACE_GROUP,
                     "dh=%s, Forcing DDC communication success.", dh_repr(dh) );
               dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
               dref->flags |= DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED;   // good_enuf_for_test
            }
         }    // end, io_mode == DDC_IO_I2C
      }
   }  // end, !DREF_DDC_COMMUNICATION_CHECKED

   if ( dref->flags & DREF_DDC_COMMUNICATION_WORKING ) {
      // Would prefer to defer checking version until actually needed to avoid
      // additional DDC io during monitor detection.  Unfortunately, this would
      // introduce ddc_open_display(), with its possible error states,
      // into other functions, e.g. ddca_get_feature_list_by_dref()
      if ( vcp_version_eq(dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED)) {
         // may have been forced by option --mccs
         set_vcp_version_xdf_by_dh(dh);
      }
   }

   pdd_set_dynamic_sleep_active(dref->pdd, saved_dynamic_sleep_active);   // in case it was set false

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "Final flags: %s", interpret_dref_flags_t(dref->flags));
   return ddc_excp;
}


/** Given a #Display_Ref, opens the monitor device and calls #ddc_initial_checks_by_dh()
 *  to perform initial monitor checks.
 *
 *  @param dref pointer to #Display_Ref for monitor
 *  @return **true** if DDC communication with the display succeeded, **false** otherwise.
 *
 *  @remark
 *  If global flag **skip_ddc_checks** is set, checking is not performed.
 *  DDC communication is assumed to work, and monitor uses the unsupported feature
 *  flag in reply packets to indicate an unsupported feature.
 */
Error_Info *
ddc_initial_checks_by_dref(Display_Ref * dref, bool newly_added) {
   assert(dref);
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s, newly_added=%s", dref_repr_t(dref), sbool(newly_added));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Initial dref->flags: %s", interpret_dref_flags_t(dref->flags));

   bool result = false;
   Error_Info * err = NULL;
   I2C_Bus_Info * businfo = NULL;

   bool disabled_mmk = is_disabled_mmk(*dref->mmid); // is this monitor model disabled?
   if (disabled_mmk) {
      dref->flags |= DREF_DDC_DISABLED;
      dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;
      goto bye;
   }

   bool skip_ddc_checks0 = skip_ddc_checks;
   if (dref->io_path.io_mode == DDCA_IO_I2C) {
      businfo = dref->detail;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "I2C_BUS_DDC_CHECKS_IGNORABLE is set: %s",
           SBOOL(businfo->flags & I2C_BUS_DDC_CHECKS_IGNORABLE) );
      if (businfo->flags & I2C_BUS_DDC_CHECKS_IGNORABLE)
         skip_ddc_checks0 = true;
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "skip_ddc_checks0 = %s", SBOOL(skip_ddc_checks0));
   if (skip_ddc_checks0) {
      dref->flags |= (DREF_DDC_COMMUNICATION_CHECKED |
                      DREF_DDC_COMMUNICATION_WORKING |
                      DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED);
      dref->vcp_version_xdf = DDCA_VSPEC_UNKNOWN;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "dref=%s, skipping initial ddc checks", dref_repr_t(dref));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Skipping initial ddc checks");
      result = true;
   }
   else {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Performing initial ddc checks");
      // if (!(dref->flags & DREF_DPMS_SUSPEND_STANDBY_OFF)) {
      Display_Handle * dh = NULL;

      err = ddc_open_display(dref, CALLOPT_ERR_MSG, &dh);
      if (err)  {
         char * msg = g_strdup_printf("Unable to open %s: %s",
         dpath_repr_t(&dref->io_path), psc_desc(err->status_code));
         SYSLOG2(DDCA_SYSLOG_WARNING, "%s", msg);
         free(msg);
      }
      else {
         err = ddc_initial_checks_by_dh(dh, newly_added);
         if (err) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "ddc_initial_checks_by_dh() returned %s",
                                                  psc_desc(err->status_code));
         }
         ddc_close_display_wo_return(dh);
      }
      if (!(dref->flags & DREF_REMOVED))
         dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;

      if (err && err->status_code == -EBUSY)
         dref->flags |= DREF_DDC_BUSY;

      // return err;
   // }
   }

   if (businfo) {
      bool last_ddc_check_ok = result &&
            // take the no-skip branch on a reconnection call so that
            // DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED is not automatically set:
            (dref->flags & DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED);

      SETCLR_BIT(businfo->flags, I2C_BUS_DDC_CHECKS_IGNORABLE, last_ddc_check_ok);

      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "I2C_BUS_DDC_CHECKS_IGNORABLE is set: %s",
      //       SBOOL(businfo->flags & I2C_BUS_DDC_CHECKS_IGNORABLE));
   }

bye:
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref=%s, Final flags: %s", dref_repr_t(dref), interpret_dref_flags_t(dref->flags));

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "dref=%s", dref_repr_t(dref));
   return err;
}


//
// Exploratory programming, DPMS detection
//

static void explore_monitor_one_feature(Display_Handle * dh, Byte feature_code) {
   Parsed_Nontable_Vcp_Response * parsed_response_loc = NULL;
   rpt_vstring(1, "Getting value of feature 0x%02x", feature_code);;
   Error_Info * ddc_excp = ddc_get_nontable_vcp_value(dh, feature_code, &parsed_response_loc);
   ASSERT_IFF(!ddc_excp, parsed_response_loc);
   if (ddc_excp) {
      rpt_vstring(2, "ddc_get_nontable_vcp_value() for feature 0x%02x returned: %s",
            feature_code, errinfo_summary(ddc_excp));
      free(ddc_excp);
   }
   else {
      if (!parsed_response_loc->valid_response)
         rpt_vstring(2, "Invalid Response");
      else if (!parsed_response_loc->supported_opcode)
         rpt_vstring(2, "Unsupported feature code");
      else {
         rpt_vstring(2, "getvcp 0x%02x succeeded", feature_code);
         rpt_vstring(2, "mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x",
               parsed_response_loc->mh, parsed_response_loc->ml,
               parsed_response_loc->sh, parsed_response_loc->sl);
      }
      free(parsed_response_loc);
   }
}


void explore_monitor_state(Display_Handle* dh) {
   rpt_nl();
   rpt_label(0, "-----------------------");

#ifdef SYS_DRM_CONNECTOR_DEPENDENCY
   I2C_Bus_Info * businfo = (I2C_Bus_Info*) dh->dref->detail;

   char * connector_name = NULL;
   Sys_Drm_Connector * conn =  i2c_check_businfo_connector(businfo);
   if (!conn)
      rpt_vstring(0, "i2c_check_businfo_connector() failed for bus %d", businfo->busno);
   else {
      connector_name = conn->connector_name;
      rpt_vstring(0, "Examining monitor state for model: %s, bus /dev/i2c-%d:, connector: %s",
            dh->dref->pedid->model_name, businfo->busno, connector_name);
   }
   rpt_nl();
#endif

   rpt_vstring(0, "Environment Variables");
   char * xdg_session_desktop = getenv("XDG_SESSION_DESKTOP");
   rpt_vstring(1, "XDG_SESSION_DESKTOP:  %s", xdg_session_desktop);
   char * xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
   rpt_vstring(1, "XDG_CURRENT_DESKTOP:  %s", xdg_current_desktop);
   char * xdg_vtnr = getenv("XDG_VTNR");
   rpt_vstring(1, "XDG_VTNR:  %s", xdg_vtnr);
   char * xdg_session_type = getenv("XDG_SESSION_TYPE");
   rpt_vstring(1, "XDG_SESSION_TYPE = |%s|", xdg_session_type);
   rpt_nl();

   rpt_vstring(0, "Getvcp tests");
   pdd_set_dynamic_sleep_active(dh->dref->pdd, false);
   explore_monitor_one_feature(dh, 0x00);
   explore_monitor_one_feature(dh, 0x10);
   explore_monitor_one_feature(dh, 0x41);
   explore_monitor_one_feature(dh, 0xd6);
   rpt_nl();

   if (streq(xdg_session_type, "x11")) {
      rpt_vstring(0, "X11 dpms information");
   //  query X11
   // execute_shell_cmd(  "xset q | grep DPMS -A 3");
#ifdef USE_X11
   unsigned short power_level;
   unsigned char state;
   bool ok =get_x11_dpms_info(&power_level, &state);
   if (ok) {
      rpt_vstring(1, "power_level=%d = %s, state=%s",
            power_level, dpms_power_level_name(power_level), sbool(state));
   }
   else
      DBGMSG("get_x11_dpms_info() failed");
#endif
   rpt_nl();
   }

#ifdef SYS_DRM_CONNECTOR_DEPENDENCY
   rpt_vstring(0, "Probing sysfs");
   if (connector_name) {
      RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", connector_name, "dpms");
      RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", connector_name, "enabled");
      RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", connector_name, "status");
#ifdef NO_USEFUL_INFO
      RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", connector_name, "power/runtime_enabled");
      RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", connector_name, "power/runtime_status");
      RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", connector_name, "power/runtime_suspended_time");
#endif
   }
#endif

   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0", "name");
#ifdef NO_USEFUL_INFO
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "async");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "autosuspend_delay_ms");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "control");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "runtime_active_kids");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "runtime_active_time");
#endif
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "runtime_enabled");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "runtime_status");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "runtime_syspended_time");
   RPT_ATTR_TEXT(1, NULL, "/sys/class/graphics/fb0/power", "runtime_usage");
   rpt_nl();
}


void init_ddc_initial_checks() {
   RTTI_ADD_FUNC(check_how_unsupported_reported);

   RTTI_ADD_FUNC(ddc_initial_checks_by_dh);
   RTTI_ADD_FUNC(ddc_initial_checks_by_dref);

   RTTI_ADD_FUNC(read_unsupported_feature);
   RTTI_ADD_FUNC(check_supported_feature);
}

