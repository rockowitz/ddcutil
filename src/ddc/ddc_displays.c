/** @file ddc_displays.c
 *
 *  Access displays, whether DDC or USB
 *
 *  This file and ddc_display_ref_reports.c cross-reference each other.
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef USE_X11
#include <X11/extensions/dpmsconst.h>
#endif

#include "config.h"

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/drm_common.h"
#include <base/drm_connector_state.h>
#include "util/edid.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#ifdef ENABLE_UDEV
#include "util/udev_usb_util.h"
#include "util/udev_util.h"
#endif
#ifdef USE_X11
#include "util/x11_util.h"
#endif

/** \endcond */

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/dsa2.h"
#include "base/feature_metadata.h"
#include "base/linux_errno.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/rtti.h"

#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_dpms.h"
#include "i2c/i2c_strategy_dispatcher.h"
#include "i2c/i2c_sys_drm_connector.h"
#include "i2c/i2c_sysfs_base.h"  // for is_sysfs_unreliable()

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

#include "dynvcp/dyn_feature_files.h"

#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_phantom_displays.h"
#include "ddc/ddc_serialize.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_watch_displays_main.h"

#include "ddc/ddc_displays.h"

// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;

static GPtrArray * display_open_errors = NULL;  // array of Bus_Open_Error
static int ddc_detect_async_threshold = DEFAULT_DDC_CHECK_ASYNC_THRESHOLD;
#ifdef ENABLE_USB
static bool detect_usb_displays = true;
#else
static bool detect_usb_displays = false;
#endif

#ifdef DONT_CHECK_EDID
static bool allow_asleep = true;
#endif

// Externally visible globals:
int  dispno_max = 0;                      // highest assigned display number
bool monitor_state_tests = false;
bool skip_ddc_checks = false;


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
check_supported_feature(Display_Handle * dh, bool newly_added) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, newly_added=%s", dh_repr(dh), SBOOL(newly_added));

   Error_Info * ddc_excp = NULL;

   Per_Display_Data * pdd = dh->dref->pdd;
   Display_Ref * dref = dh->dref;
   I2C_Bus_Info * businfo = dh->dref->detail;
:
   DDCA_Sleep_Multiplier initial_multiplier = pdd_get_adjusted_sleep_multiplier(pdd);
   Parsed_Nontable_Vcp_Response* parsed_response_loc = NULL;
   // feature that always exists
   Byte feature_code = 0x10;
   ddc_excp = ddc_get_nontable_vcp_value(dh, feature_code, &parsed_response_loc);
   // may return DDCRC_DISCONNECTED from i2c_check_open_bus_alive()


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
      SYSLOG2(DDCA_SYSLOG_ERROR, "!!! %s", msg);
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
         "ddc_get_nontable_vcp_value() for feature 0x10 returned: %s, status: %s",
         errinfo_summary(ddc_excp), psc_desc(psc));


   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "");
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
   I2C_Bus_Info * businfo = dh->dref->detail;
   Per_Display_Data * pdd = dh->dref->pdd;
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "adjusted sleep-multiplier = %5.2f",
                                       pdd_get_adjusted_sleep_multiplier(pdd));
   Error_Info * ddc_excp = NULL;

   if (debug)
      show_backtrace(1);

   Display_Ref * dref = dh->dref;
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
         ddc_excp = check_supported_feature(dh, newly_added);

         Public_Status_Code psc = ERRINFO_STATUS(ddc_excp);
         if (psc == DDCRC_DISCONNECTED) {
            dh->dref->flags = DREF_REMOVED;
         }

         else if (psc != -EBUSY)
            dh->dref->flags |= DREF_DDC_COMMUNICATION_CHECKED;

         if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
            if (psc == 0 ||
                psc == DDCRC_REPORTED_UNSUPPORTED ||
                psc == DDCRC_DETERMINED_UNSUPPORTED)
            {
               dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
            }
         }

         else {   // DDCA_IO_I2C
            if (psc == 0 ||
                psc == DDCRC_REPORTED_UNSUPPORTED ||
                psc == DDCRC_DETERMINED_UNSUPPORTED)
            {
               dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
               check_how_unsupported_reported(dh);
            }  // end, communication working
            else {
               if (psc == -EBUSY) {
                  // communication failed, do not set DDCRC_COMMUNICATION_WORKING
                  dh->dref->flags |= DREF_DDC_BUSY;
               }
               else {
                  // do not set DDCRC_COMMUNICATION_WORKING
               }
            }

            if ( i2c_force_bus /* && psc == DDCRC_RETRIES */) {  // used only when testing
               DBGTRC_NOPREFIX(debug || true , TRACE_GROUP,
                     "dh=%s, Forcing DDC communication success.", dh_repr(dh) );
               dh->dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
               dh->dref->flags |= DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED;   // good_enuf_for_test
            }
         }    // end, io_mode == DDC_IO_I2C

         // DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Before ERRINFO_FREE_WITH_REPORT(): ddc_excp=%p", ddc_excp);
         // if (ddc_excp)
         //    ERRINFO_FREE_WITH_REPORT(ddc_excp, IS_DBGTRC(debug, TRACE_GROUP));

         if ( dh->dref->flags & DREF_DDC_COMMUNICATION_WORKING ) {
            // Would prefer to defer checking version until actually needed to avoid
            // additional DDC io during monitor detection.  Unfortunately, this would
            // introduce ddc_open_display(), with its possible error states,
            // into other functions, e.g. ddca_get_feature_list_by_dref()
            if ( vcp_version_eq(dh->dref->vcp_version_xdf, DDCA_VSPEC_UNQUERIED)) {
               // may have been forced by option --mccs
               set_vcp_version_xdf_by_dh(dh);
            }
         }

         pdd_set_dynamic_sleep_active(dref->pdd, true);   // in case it was set false
         // free(parsed_response_loc);
      }
   }  // end, !DREF_DDC_COMMUNICATION_CHECKED


   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "Final flags: %s", interpret_dref_flags_t(dh->dref->flags));
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
   if (dref) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref=%s, Final flags: %s", dref_repr_t(dref), interpret_dref_flags_t(dref->flags));
   }
   // DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "dref = %s", dref_repr_t(dref) );
   // if (err)
   //    errinfo_free(err);
   // return result;
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "");
   return err;
}


/** Performs initial checks in a thread
 *
 *  @param data display reference
 */
STATIC void *
threaded_initial_checks_by_dref(gpointer data) {
   bool debug = false;

   Display_Ref * dref = data;
   TRACED_ASSERT(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref) );

   Error_Info * erec = ddc_initial_checks_by_dref(dref, false);
   // g_thread_exit(NULL);
   ERRINFO_FREE_WITH_REPORT(erec, debug);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning NULL. dref = %s,", dref_repr_t(dref) );
   free_current_traced_function_stack();
   return NULL;
}


/** Spawns threads to perform initial checks and waits for them all to complete.
 *
 *  @param all_displays #GPtrArray of pointers to #Display_Ref
 */
STATIC void
ddc_async_scan(GPtrArray * all_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays=%p, display_count=%d",
                                       all_displays, all_displays->len);

   GPtrArray * threads = g_ptr_array_new();
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );

      GThread * th =
      g_thread_new(
            dref_repr_t(dref),                // thread name
            threaded_initial_checks_by_dref,
            dref);                            // pass pointer to display ref as data
      g_ptr_array_add(threads, th);
   }
   DBGMSF(debug, "Started %d threads", threads->len);
   for (int ndx = 0; ndx < threads->len; ndx++) {
      GThread * thread = g_ptr_array_index(threads, ndx);
      g_thread_join(thread);  // implicitly unrefs the GThread
   }
   DBGMSF(debug, "Threads joined");
   g_ptr_array_free(threads, true);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Loops through a list of display refs, performing initial checks on each.
 *
 *  @param all_displays #GPtrArray of pointers to #Display_Ref
 */
STATIC void
ddc_non_async_scan(GPtrArray * all_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "checking %d displays", all_displays->len);

   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      Error_Info * err = ddc_initial_checks_by_dref(dref, false);
      free(err);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Functions to get display information
//

/** Gets a list of all detected displays, whether they support DDC or not.
 *
 *  Detection must already have occurred.
 *
 *  @return **GPtrArray of #Display_Ref instances
 */
GPtrArray *
ddc_get_all_display_refs() {
   // ddc_ensure_displays_detected();
   TRACED_ASSERT(all_display_refs);
   return all_display_refs;
}


/** Gets a list of all detected displays, optionally excluding those
 *  that are invalid.
 *
 *  @param  include_invalid_displays  i.e. displays having EDID but not DDC
 *  @param  include_removed_drefs
 *  @return **GPtrArray of #Display_Ref instances
 */
GPtrArray *
ddc_get_filtered_display_refs(bool include_invalid_displays, bool include_removed_drefs) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "include_invalid_displays=%s, include_removed_drefs=%s",
         sbool(include_invalid_displays), sbool(include_removed_drefs));
   TRACED_ASSERT(all_display_refs);

   GPtrArray * result = g_ptr_array_sized_new(all_display_refs->len);
   for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
      Display_Ref * cur = g_ptr_array_index(all_display_refs, ndx);
      if ((include_invalid_displays || cur->dispno > 0) &&
          (!(cur->flags&DREF_REMOVED) || include_removed_drefs) )
      {
         g_ptr_array_add(result, cur);
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning array of size %d", result->len);
   if (debug || IS_TRACING()) {
      ddc_dbgrpt_drefs("Display_Refs:", result, 2);
   }
   return result;
}


#ifdef UNUSED
void ddc_dbgrpt_display_refs(bool include_invalid_displays, bool report_businfo, int depth) {
   GPtrArray * drefs = ddc_get_filtered_display_refs(include_invalid_displays, true);
   rpt_vstring(depth, "Reporting %d display refs", drefs->len);
   for (int ndx = 0; ndx < drefs->len; ndx++) {
      dbgrpt_display_ref(g_ptr_array_index(drefs, ndx), false, depth+1);
   }
   g_ptr_array_free(drefs,true);
}
#endif


void ddc_dbgrpt_display_refs_summary(bool include_invalid_displays, bool report_businfo, int depth) {
   GPtrArray * drefs = ddc_get_filtered_display_refs(include_invalid_displays, /* include removed */ true);
   // rpt_vstring(depth, "Reporting %d display refs", drefs->len);
   for (int ndx = 0; ndx < drefs->len; ndx++) {
      dbgrpt_display_ref_summary(g_ptr_array_index(drefs, ndx), report_businfo, depth);
   }
   g_ptr_array_free(drefs,true);
}


void ddc_dbgrpt_display_refs_terse(bool include_invalid_displays, int depth) {
   GPtrArray * drefs = ddc_get_filtered_display_refs(include_invalid_displays, /* include removed */ true);
   // rpt_vstring(depth, "Reporting %d display refs", drefs->len);
   for (int ndx = 0; ndx < drefs->len; ndx++) {
      rpt_vstring(depth, "%s", dref_reprx_t(g_ptr_array_index(drefs, ndx)));
   }
   g_ptr_array_free(drefs,true);
}

/** Returns the number of detected displays.
 *
 *  @param  include_invalid_displays
 *  @return number of displays, 0 if display detection has not yet occurred.
 */
int
ddc_get_display_count(bool include_invalid_displays) {
   int display_ct = -1;
   if (all_display_refs) {
      display_ct = 0;
      for (int ndx=0; ndx<all_display_refs->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_display_refs, ndx);
         TRACED_ASSERT(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
         if (dref->dispno > 0 || include_invalid_displays) {
            display_ct++;
         }
      }
   }
   return display_ct;
}


/** Returns list of all open() errors encountered during display detection.
 *
 *  @return **GPtrArray of #Bus_Open_Error.
 */
GPtrArray *
ddc_get_bus_open_errors() {
   return display_open_errors;
}



/** Emits a debug report of a list of #Bus_Open_Error.
 *
 *  @param open_errors  array of #Bus_Open_Error
 *  @param depth        logical indentation depth
 */
void dbgrpt_bus_open_errors(GPtrArray * open_errors, int depth) {
   int d1 = depth+1;
   if (!open_errors || open_errors->len == 0) {
      rpt_vstring(depth, "Bus open errors:  None");
   }
   else {
      rpt_vstring(depth, "Bus open errors:");
      for (int ndx = 0; ndx < open_errors->len; ndx++) {
         Bus_Open_Error * cur = g_ptr_array_index(open_errors, ndx);
         rpt_vstring(d1, "%s bus:  %-2d, error: %d, detail: %s",
               (cur->io_mode == DDCA_IO_I2C) ? "I2C" : "hiddev",
               cur->devno, cur->error, cur->detail);
      }
   }
}


//
// Display Detection
//

/** Sets the threshold for async display examination.
 *  If the number of /dev/i2c devices for which DDC communication is to be
 *  checked is greater than or equal to the threshold value, examine each
 *  device in a separate thread.
 *
 *  @param threshold  threshold value
 */
void ddc_set_async_threshold(int threshold) {
   // DBGMSG("threshold = %d", threshold);
   ddc_detect_async_threshold = threshold;
}

static bool all_displays_asleep = true;

#ifdef UNUSED
Display_Ref * get_display_ref_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno = %d", businfo->busno);
   Display_Ref * dref = NULL;

   DBGTRC_DONE(debug, TRACE_GROUP, "dref=%s", dref_repr_t(dref));
   return dref;
}

Display_Ref * detect_display_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno = %d", businfo->busno);
   Display_Ref * dref = NULL;
   if ( (businfo->flags & I2C_BUS_ADDR_0X50)  && businfo->edid ) {
      dref = create_bus_display_ref(businfo->busno);
      dref->dispno = DISPNO_INVALID;   // -1, guilty until proven innocent
      if (businfo->drm_connector_name) {
         dref->drm_connector = g_strdup(businfo->drm_connector_name);
      }
      dref->pedid = copy_parsed_edid(businfo->edid);    // needed?
      dref->mmid  = mmk_new(dref->pedid->mfg_id,
                                          dref->pedid->model_name,
                                          dref->pedid->product_code);
      dref->detail = businfo;
      dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
      dref->flags |= DREF_DDC_IS_MONITOR;

      bool asleep = dpms_check_drm_asleep(businfo);
      if (asleep) {
         dref->flags |= DREF_DPMS_SUSPEND_STANDBY_OFF;
      }
      else {
         all_displays_asleep = false;
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "dref=%s", dref_repr_t(dref));
   return dref;
}
#endif


/** Detects all connected displays by querying the I2C and USB subsystems.
 *
 *  @param  open_errors_loc where to return address of #GPtrArray of #Bus_Open_Error
 *  @return array of #Display_Ref
 */
STATIC GPtrArray *
ddc_detect_all_displays(GPtrArray ** i2c_open_errors_loc) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "display_caching_enabled=%s, detect_usb_displays=%s",
         sbool(display_caching_enabled), sbool(detect_usb_displays));

   dispno_max = 0;
   GPtrArray * bus_open_errors = g_ptr_array_new();
   g_ptr_array_set_free_func(bus_open_errors, (GDestroyNotify) free_bus_open_error);
   GPtrArray * display_list = g_ptr_array_new();
   g_ptr_array_set_free_func(display_list, (GDestroyNotify) free_display_ref);

   int busct = i2c_detect_buses();
   DBGTRC(debug, DDCA_TRC_NONE, "i2c_detect_buses() returned: %d", busct);
   guint busndx = 0;
   for (busndx=0; busndx < busct; busndx++) {
      DBGMSF(debug, "busndx = %d", busndx);
      I2C_Bus_Info * businfo = i2c_get_bus_info_by_index(busndx);
      // if (IS_DBGTRC(debug, DDCA_TRC_NONE))
      //    i2c_dbgrpt_bus_info(businfo, 2);
      if ( businfo->edid ) {
         Display_Ref * dref = NULL;
         // Do not restore serialized display ref if slave address x37 inactive
         // Prevents creating a display ref with stale contents
         if (display_caching_enabled && (businfo->flags&I2C_BUS_ADDR_X37) ) {
            dref = copy_display_ref(
                       ddc_find_deserialized_display(businfo->busno, businfo->edid->bytes));
            if (dref)
               dref->detail = businfo;
         }
         if (!dref) {
            dref = create_bus_display_ref(businfo->busno);
            dref->dispno = DISPNO_INVALID;   // -1, guilty until proven innocent
            if (businfo->drm_connector_name) {
               dref->drm_connector = g_strdup(businfo->drm_connector_name);
            }
            dref->drm_connector_id = businfo->drm_connector_id;
            dref->pedid = copy_parsed_edid(businfo->edid);
            dref->mmid  = mmk_new(dref->pedid->mfg_id,
                                                dref->pedid->model_name,
                                                dref->pedid->product_code);
            dref->detail = businfo;
            dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
            dref->flags |= DREF_DDC_IS_MONITOR;
         }

#ifdef USE_X11
         bool asleep = dpms_state&DPMS_STATE_X11_ASLEEP;
         if (!asleep & !(dpms_state&DPMS_STATE_X11_CHECKED)) {
             if (dpms_check_drm_asleep_by_dref(dref)) {
                dpms_state |= DPMS_SOME_DRM_ASLEEP;
                asleep = true;
             }
             else {
                all_displays_asleep = false;
             }
         }
#else
         if (dpms_check_drm_asleep_by_dref(dref)) {
            dpms_state |= DPMS_SOME_DRM_ASLEEP;
            dref->flags |= DREF_DPMS_SUSPEND_STANDBY_OFF;
          }
          else {
             all_displays_asleep = false;
          }
#endif

         // dbgrpt_display_ref(dref,5);
         g_ptr_array_add(display_list, dref);
      }
      else if ( !(businfo->flags & I2C_BUS_ACCESSIBLE) ) {
         Bus_Open_Error * boe = calloc(1, sizeof(Bus_Open_Error));
         boe->io_mode = DDCA_IO_I2C;
         boe->devno = businfo->busno;
         boe->error = businfo->open_errno;
         g_ptr_array_add(bus_open_errors, boe);
      }
   }

#ifdef ENABLE_USB
   if (detect_usb_displays) {
      GPtrArray * usb_monitors = get_usb_monitor_list();  // array of USB_Monitor_Info
      // DBGMSF(debug, "Found %d USB displays", usb_monitors->len);
      for (int ndx=0; ndx<usb_monitors->len; ndx++) {
         Usb_Monitor_Info  * curmon = g_ptr_array_index(usb_monitors,ndx);
         TRACED_ASSERT(memcmp(curmon->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
         Display_Ref * dref = create_usb_display_ref(
                                   curmon->hiddev_devinfo->busnum,
                                   curmon->hiddev_devinfo->devnum,
                                   curmon->hiddev_device_name);
         dref->dispno = DISPNO_INVALID;   // -1
         dref->pedid = copy_parsed_edid(curmon->edid);
         if (dref->pedid)
            dref->mmid  = mmk_new(
                             dref->pedid->mfg_id,
                             dref->pedid->model_name,
                             dref->pedid->product_code);
         else
            dref->mmid = mmk_new("UNK", "UNK", 0);
         // drec->detail.usb_detail = curmon;
         dref->detail = curmon;
         dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
         dref->flags |= DREF_DDC_IS_MONITOR;

#ifdef OUT
         bool asleep = dpms_state&DPMS_STATE_X11_ASLEEP;
         if (!asleep & !(dpms_state&DPMS_STATE_X11_CHECKED)) {
             if (dpms_check_drm_asleep_by_dref(dref)) {
                dpms_state |= DPMS_SOME_DRM_ASLEEP;
                asleep = true;
             }
             else {
                all_displays_asleep = false;
             }
         }
         if (asleep) {
            dref->flags |= DREF_DPMS_SUSPEND_STANDBY_OFF;
         }
#endif
#ifdef USE_X11
         bool asleep = dpms_state&DPMS_STATE_X11_ASLEEP;
         if (!asleep & !(dpms_state&DPMS_STATE_X11_CHECKED)) {
             if (dpms_check_drm_asleep_by_dref(dref)) {
                dpms_state |= DPMS_SOME_DRM_ASLEEP;
                asleep = true;
             }
             else {
                all_displays_asleep = false;
             }
         }
#else
         if (dpms_check_drm_asleep_by_dref(dref)) {
            dpms_state |= DPMS_SOME_DRM_ASLEEP;
            dref->flags |= DREF_DPMS_SUSPEND_STANDBY_OFF;
          }
          else {
             all_displays_asleep = false;
          }
#endif
         g_ptr_array_add(display_list, dref);
      }


      GPtrArray * usb_open_errors = get_usb_open_errors();
      if (usb_open_errors && usb_open_errors->len > 0) {
         for (int ndx = 0; ndx < usb_open_errors->len; ndx++) {
            Bus_Open_Error * usb_boe = (Bus_Open_Error *) g_ptr_array_index(usb_open_errors, ndx);
            Bus_Open_Error * boe_copy = calloc(1, sizeof(Bus_Open_Error));
            boe_copy->io_mode = DDCA_IO_USB;
            boe_copy->devno   = usb_boe->devno;
            boe_copy->error   = usb_boe->error;
            boe_copy->detail  = usb_boe->detail;
            g_ptr_array_add(bus_open_errors, boe_copy);
         }
      }
   }
#endif
    if (all_displays_asleep)
       dpms_state |= DPMS_ALL_DRM_ASLEEP;
    else
       dpms_state &= ~DPMS_ALL_DRM_ASLEEP;

   // verbose output is distracting within scans
   // saved and reset here so that async threads are not adjusting output level
   DDCA_Output_Level olev = get_output_level();
   if (olev == DDCA_OL_VERBOSE)
      set_output_level(DDCA_OL_NORMAL);

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "display_list->len=%d, ddc_detect_async_threshold=%d",
                 display_list->len, ddc_detect_async_threshold);
   if (display_list->len >= ddc_detect_async_threshold)
      ddc_async_scan(display_list);
   else
      ddc_non_async_scan(display_list);

   if (olev == DDCA_OL_VERBOSE)
      set_output_level(olev);

   // assign display numbers
   for (int ndx = 0; ndx < display_list->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(display_list, ndx);
      TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
      // DBGMSG("dref->flags: %s", interpret_dref_flags_t(dref->flags));
      if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING))
         DBGTRC(debug, DDCA_TRC_NONE,"dref=%s, DREF_DDC_COMMUNICATON_WORKING not set", dref_repr_t(dref));
      // if (dref->flags & DREF_DPMS_SUSPEND_STANDBY_OFF)
      //    dref->dispno = DISPNO_INVALID;  // does this need to be different?
      if (dref->flags & DREF_DDC_DISABLED)
         dref->dispno = DISPNO_DDC_DISABLED;
      else if (dref->flags & DREF_DDC_BUSY)
         dref->dispno = DISPNO_BUSY;
      else if (dref->flags & DREF_DDC_COMMUNICATION_WORKING)
         dref->dispno = ++dispno_max;
      else {
         dref->dispno = DISPNO_INVALID;   // -1;
      }
   }

   bool phantom_displays_found = filter_phantom_displays(display_list);
   if (phantom_displays_found) {
      // in case a display other than the last was marked phantom
      int next_valid_display = 1;
      for (int ndx = 0; ndx < display_list->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(display_list, ndx);
         if (dref->dispno > 0)
            dref->dispno = next_valid_display++;
      }
   }

   if (bus_open_errors->len > 0) {
      *i2c_open_errors_loc = bus_open_errors;
   }
   else {
      g_ptr_array_free(bus_open_errors, false);
      *i2c_open_errors_loc = NULL;
   }

   if (debug) {
      DBGMSG("Displays detected:");
      ddc_dbgrpt_drefs("display_list:", display_list, 1);
      dbgrpt_bus_open_errors(*i2c_open_errors_loc, 1);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p, Detected %d valid displays",
                display_list, dispno_max);
   return display_list;
}


/** Initializes the master display list in global variable #all_displays and
 *  records open errors in global variable #display_open_errors.
 *
 *  Does nothing if the list has already been initialized.
 */
void
ddc_ensure_displays_detected() {
   bool debug = false || debug_locks;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   g_mutex_lock(&all_display_refs_mutex);
   if (!all_display_refs) {
      // i2c_detect_buses();  // called in ddc_detect_all_displays()
      all_display_refs = ddc_detect_all_displays(&display_open_errors);
   }
   g_mutex_unlock(&all_display_refs_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP,
               "all_displays=%p, all_displays has %d displays",
               all_display_refs, all_display_refs->len);
}


/** Discards all detected displays.
 *
 *  - All open displays are closed
 *  - The list of open displays in #all_displays is discarded:1427
 *  - The list of errors in #display_open_errors is discarded
 *  - The list of detected I2C buses is discarded
 *  - The USB monitor list is discarded
 */
void
ddc_discard_detected_displays() {
   bool debug = false || debug_locks;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   // grab locks to prevent any opens?
   ddc_close_all_displays();
#ifdef ENABLE_USB
   discard_usb_monitor_list();
#endif
   if (all_display_refs) {
      for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_display_refs, ndx);
         dref->flags |= DREF_TRANSIENT;  // hack to allow all Display References to be freed
#ifdef OUT
#ifndef NDEBUG
         DDCA_Status ddcrc = free_display_ref(dref);
         TRACED_ASSERT(ddcrc==0);
#else
         free_display_ref(dref);
#endif
#endif
      }
      g_mutex_lock(&all_display_refs_mutex);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "calling g_ptr_array_free(all_display_refs, true)...");
      g_ptr_array_free(all_display_refs, true);
      g_mutex_unlock(&all_display_refs_mutex);
      all_display_refs = NULL;
      if (display_open_errors) {
         g_ptr_array_free(display_open_errors, true);
         display_open_errors = NULL;
      }
   }
   // free_sys_drm_connectors();
   i2c_discard_buses();
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void
ddc_redetect_displays() {
   bool debug = false || debug_locks;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays=%p", all_display_refs);
   SYSLOG2(DDCA_SYSLOG_NOTICE, "Display redetection starting.");
   DDCA_Display_Event_Class enabled_classes = DDCA_EVENT_CLASS_NONE;
   DDCA_Status active_rc = ddc_get_active_watch_classes(&enabled_classes);
   if (active_rc == DDCRC_OK) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling ddc_stop_watch_displays()");
      DDCA_Status rc = ddc_stop_watch_displays(/*wait*/ true, &enabled_classes);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Called ddc_stop_watch_displays()");
      assert(rc == DDCRC_OK);
   }
   ddc_discard_detected_displays();
   if (dsa2_is_enabled())
      dsa2_save_persistent_stats();
   // free_sysfs_drm_connector_names();

   if (use_drm_connector_states)
      redetect_drm_connector_states();

   // init_sysfs_drm_connector_names();
   // get_sys_drm_connectors(/*rescan=*/true);
   if (dsa2_is_enabled()) {
      Error_Info * erec = dsa2_restore_persistent_stats();
      if (erec) {
         MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Unexpected error from dsa2_restore_persistent_stats(): %s",
               errinfo_summary(erec));
         free(erec);
      }
   }
   i2c_detect_buses();
   g_mutex_lock(&all_display_refs_mutex);
   all_display_refs = ddc_detect_all_displays(&display_open_errors);
   g_mutex_unlock(&all_display_refs_mutex);
   if (debug) {
      ddc_dbgrpt_drefs("all_displays:", all_display_refs, 1);
      // dbgrpt_valid_display_refs(1);
   }
   if (active_rc == DDCRC_OK) {
      Error_Info * err = ddc_start_watch_displays(enabled_classes);
      assert(!err);    // should never fail since restarting with same enabled classes
   }

   SYSLOG2(DDCA_SYSLOG_NOTICE, "Display redetection finished.");
   DBGTRC_DONE(debug, TRACE_GROUP, "all_displays=%p, all_displays->len = %d",
                                   all_display_refs, all_display_refs->len);
}


#ifdef UNUSED
/** Checks that a #Display_Ref is in array **all_displays**
 *  of all valid #Display_Ref values.
 *
 *  @param  dref  #Display_Ref
 *  @return true/false
 */
bool
ddc_is_known_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%p -> %s", dref, dref_repr_t(dref));
   bool result = false;
   if (all_display_refs) {
      for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
         Display_Ref* cur = g_ptr_array_index(all_display_refs, ndx);
         DBGMSF(debug, "Checking vs valid dref %p", cur);
         
         if (cur == dref) {
            // if (cur->dispno > 0)  // why?
               result = true;
            break;
         }
      }
   }
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "dref=%p, dispno=%d", dref, dref->dispno);
   return result;
}
#endif


/** Replacement for #ddc_is_valid_display_ref() that returns a status code
 *  indicating why a display ref is invalid.
 *
 *  @param   dref   display reference to validate
 *  #param   validation_options
 *  @retval  DDCRC_OK
 *  @retval  DDCRC_ARG             dref is null or does not point to a Display_Ref
 *  @retval  DDCRC_DISCONNECTED    display has been disconnected
 *  @retval  DDCRC_DPMS_ASLEEP     possible if require_not_asleep == true
 */
DDCA_Status
ddc_validate_display_ref2(Display_Ref * dref, Dref_Validation_Options validation_options) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%p -> %s, validation_options=x%02x",
         dref, dref_reprx_t(dref), validation_options);
   assert(all_display_refs);

   DDCA_Status ddcrc = DDCRC_OK;
   if (!dref || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Invalid marker");
         ddcrc = DDCRC_ARG;
   }
   else {
      if (IS_DBGTRC(debug, DDCA_TRC_NONE))
         dbgrpt_display_ref(dref, /*include_businfo*/ false, 1);
      // int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;
      if (dref->flags & DREF_REMOVED) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Already marked removed");
         ddcrc = DDCRC_DISCONNECTED;
      }
#ifdef DONT_CHECK_EDID
      else if (all_video_adapters_implement_drm) {
         int d = (debug) ? 1 : -1;
         if (!dref->drm_connector) {
            // start_capture(DDCA_CAPTURE_STDERR);
            rpt_vstring(0, "Internal error in %s at line %d in file %s. dref->drm_connector == NULL",
                         __func__, __LINE__, __FILE__);
            SYSLOG2(DDCA_SYSLOG_ERROR,
                  "Internal error in %s at line %d in file %s. dref->drm_connector == NULL",
                  __func__, __LINE__, __FILE__);
            dbgrpt_display_ref(dref, true, 1);
            rpt_nl();
            report_sys_drm_connectors(true, 1);
            // Null_Terminated_String_Array lines = end_capture_as_ntsa();
            // for (int ndx=0; lines[ndx]; ndx++) {
            //    LOGABLE_MSG(DDCA_SYSLOG_ERROR, "%s", lines[ndx]);
            // }
            // ntsa_free(lines, true);
            // ddcrc = DDCRC_INTERNAL_ERROR;
            ddcrc = DDCRC_OK;
         }

         else {
            if (ddcrc == 0 && (validation_options&DREF_VALIDATE_EDID)) {
               if (is_sysfs_reliable_for_busno(DREF_BUSNO(dref))) {
                  int maxtries = 4;
                  int sleep_millis = 500;
                  for (int tryctr = 0; tryctr < maxtries; tryctr++) {
                     if (tryctr > 0) {
                        // usleep(MILLIS2NANOS(sleep_millis));
                        DW_SLEEP_MILLIS(sleep_millis, "Reading edid from sysfs");
                     }
                     possibly_write_detect_to_status_by_dref(dref);
                     if (!RPT_ATTR_EDID(d, NULL, "/sys/class/drm/", dref->drm_connector, "edid") ) {
                        DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "RPT_ATTR_EDID failed. tryctr=%d", tryctr);
                        ddcrc = DDCRC_DISCONNECTED;
                     }
                     else {
                        DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "RPT_ATTR_EDID succeeded. tryctr=%d", tryctr);
                        ddcrc = DDCRC_OK;
                        break;
                     }
                  }
               }
               else {
                  // may be wrong if bug in driver, edid persists after disconnection
                  MSG_W_SYSLOG(DDCA_SYSLOG_WARNING,
                        "is_sysfs_reliable_for_busno(%d) returned false.  Assuming EDID exists",
                        DREF_BUSNO(dref));
               }
            }
         }
         if (ddcrc == 0 && ((validation_options&DREF_VALIDATE_AWAKE) && !allow_asleep)) {
            if (dpms_check_drm_asleep_by_dref(dref))
               ddcrc = DDCRC_DPMS_ASLEEP;
         }
      }
#endif
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "dref=%p=%s", dref, dref_reprx_t(dref));
   return ddcrc;
}


/** Indicates whether displays have already been detected
 *
 *  @return true/false
 */
bool
ddc_displays_already_detected()
{
   bool debug = false;
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "Returning %s", SBOOL(all_display_refs));
   return all_display_refs;
}


/** Controls whether USB displays are to be detected.
 *
 *  Must be called before any function that triggers display detection.
 *
 *  @param  onoff  true if USB displays are to be detected, false if not
 *  @retval DDCRC_OK  normal
 *  @retval DDCRC_INVALID_OPERATION function called after displays have been detected
 *  @retval DDCRC_UNIMPLEMENTED ddcutil was not built with USB support
 *
 *  @remark
 *  If this function is not called, the default (if built with USB support) is on
 */
DDCA_Status
ddc_enable_usb_display_detection(bool onoff) {
   bool debug = false;
   DBGMSF(debug, "Starting. onoff=%s", sbool(onoff));

   DDCA_Status rc = DDCRC_UNIMPLEMENTED;
#ifdef ENABLE_USB
   if (ddc_displays_already_detected()) {
      rc = DDCRC_INVALID_OPERATION;
   }
   else {
      detect_usb_displays = onoff;
      rc = DDCRC_OK;
   }
#endif
   DBGMSF(debug, "Done.     Returning %s", psc_name_code(rc));
   return rc;
}


#ifdef UNUSED
/** Indicates whether USB displays are to be detected
 *
 *  @return true/false
 */
bool
ddc_is_usb_display_detection_enabled() {
   return detect_usb_displays;
}
#endif



#ifdef UNUSED
/** Tests if communication working for a Display_Ref
 *
 *  @param  dref   display reference
 *  @return true/false
 */
bool is_dref_alive(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s", dref_repr_t(dref));
   Display_Handle* dh = NULL;
   Error_Info * erec = NULL;
   bool is_alive = true;
   if (dref->io_path.io_mode == DDCA_IO_I2C) {   // punt on DDCA_IO_USB for now
      bool check_needed = true;
      if (dref->drm_connector) {
         char * status = NULL;
         int depth = ( IS_DBGTRC(debug, TRACE_GROUP) ) ? 2 : -1;
         RPT_ATTR_TEXT(depth,&status, "/sys/class/drm", dref->drm_connector, "status");
         if (!streq(status, "connected"))  // WRONG: Nvidia driver always "disconnected", check edid instead
            check_needed = false;
         free(status);
      }
      if (check_needed) {
         erec = ddc_open_display(dref, CALLOPT_WAIT, &dh);
         assert(!erec);
         Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;  // vs interpreted ..
         erec = ddc_get_nontable_vcp_value(dh, 0x10, &parsed_nontable_response);
         // seen: -ETIMEDOUT, DDCRC_NULL_RESPONSE then -ETIMEDOUT, -EIO, DDCRC_DATA
         // if (erec && errinfo_all_causes_same_status(erec, 0))
         if (erec)
            is_alive = false;
         ERRINFO_FREE_WITH_REPORT(erec, IS_DBGTRC(debug, TRACE_GROUP));
         ddc_close_display_wo_return(dh);
      }
   }
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, is_alive, "");
   return is_alive;
}


/** Check all display references to determine if they are active.
 *  Sets or clears the DREF_ALIVE flag in the display reference.
 */
void check_drefs_alive() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (ddc_displays_already_detected()) {
      GPtrArray * all_displays = ddc_get_all_display_refs();
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         bool alive = is_dref_alive(dref);
         if (alive != (dref->flags & DREF_ALIVE) )
            (void) dref_set_alive(dref, alive);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref=%s, is_alive=%s",
                         dref_repr_t(dref), SBOOL(dref->flags & DREF_ALIVE));
      }
   }
   else {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "displays not yet detected");
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}
#endif



void init_ddc_displays() {
   RTTI_ADD_FUNC(check_how_unsupported_reported);
   RTTI_ADD_FUNC(ddc_async_scan);
   RTTI_ADD_FUNC(ddc_close_all_displays);
   RTTI_ADD_FUNC(ddc_detect_all_displays);
   RTTI_ADD_FUNC(ddc_discard_detected_displays);
   RTTI_ADD_FUNC(ddc_displays_already_detected);
   RTTI_ADD_FUNC(ddc_ensure_displays_detected);
   RTTI_ADD_FUNC(ddc_get_all_display_refs);
   RTTI_ADD_FUNC(ddc_initial_checks_by_dh);
   RTTI_ADD_FUNC(ddc_initial_checks_by_dref);
   RTTI_ADD_FUNC(ddc_non_async_scan);
   RTTI_ADD_FUNC(ddc_redetect_displays);
   RTTI_ADD_FUNC(ddc_validate_display_ref2);
   RTTI_ADD_FUNC(read_unsupported_feature);
   RTTI_ADD_FUNC(check_supported_feature);
   RTTI_ADD_FUNC(threaded_initial_checks_by_dref);
}


void terminate_ddc_displays() {
   ddc_discard_detected_displays();
}

