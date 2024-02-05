/** @file i2c_dpms.c
 *  DPMS related functions
 */

// Copyright (C) 2023-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdlib.h>
#ifdef USE_X11
#include <X11/extensions/dpmsconst.h>
#endif

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#ifdef USE_X11
#include "util/x11_util.h"
#endif

#include "public/ddcutil_types.h"

#include "config.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/rtti.h"

#include "i2c/i2c_sysfs.h"
#include "i2c/i2c_dpms.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

//
// DPMS Detection
//

Dpms_State dpms_state;    // global

Value_Name_Table dpms_state_flags_table = {
#ifdef USE_X11
      VN(DPMS_STATE_X11_CHECKED),
      VN(DPMS_STATE_X11_ASLEEP),
#endif
      VN(DPMS_SOME_DRM_ASLEEP),
      VN(DPMS_ALL_DRM_ASLEEP),
      VN_END
};

char * interpret_dpms_state_t(Dpms_State state) {
   return VN_INTERPRET_FLAGS_T(state, dpms_state_flags_table, "|");
}


#ifdef UNUSED
void dpms_check_x11_asleep() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   char * xdg_session_type = getenv("XDG_SESSION_TYPE");
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "XDG_SESSION_TYPE = |%s|", xdg_session_type);

#ifdef USE_X11
// This is truly pathological. Symbol DPMSModeOn is defined in dpmsconst.h, which
// is included if USE_X11 is set.  However we get an undefined symbol error.
// So we defined DPMSModeOn locally. Yet eclipse greys out the conditional local
// definition, indicating that DPMSModeOn is defined.  Moreover, if the
// include of dpmsconst.h is not wrapped in a #ifdef USE_X11 blocked, the the
// symbol is defined.
#ifndef DPMSModeOn
#define DPMSModeOn 0
#endif
   if (streq(xdg_session_type, "x11")) {
      // state indicates whether or not DPMS is enabled (TRUE) or disabled (FALSE).
      // power_level indicates the current power level (one of DPMSModeOn,
      // DPMSModeStandby, DPMSModeSuspend, or DPMSModeOff.)
      unsigned short power_level;
      unsigned char state;
      bool ok =get_x11_dpms_info(&power_level, &state);
      if (ok) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "power_level=%d = %s, state=%s",
            power_level, dpms_power_level_name(power_level), sbool(state));
         if (state && (power_level != DPMSModeOn) )
            dpms_state |= DPMS_STATE_X11_ASLEEP;
         dpms_state |= DPMS_STATE_X11_CHECKED;
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "get_x11_dpms_info() failed.");
         SYSLOG2(DDCA_SYSLOG_ERROR, "get_x11_dpms_info() failed");
      }
   }
   // dpms_state |= DPMS_STATE_X11_ASLEEP; // testing
   // dpms_state = 0;    // testing

#endif
   DBGTRC_DONE(debug, TRACE_GROUP, "dpms_state = 0x%02x = %s", dpms_state, interpret_dpms_state_t(dpms_state));
}
#endif


/** Checks if a display, specified by its DRM connector name, is in a DPMS
 *  sleep mode. The check is performed using the connector's dpms attribute.
 *
 *  @param  drm_connector_name
 *  @retval true  if the dpms attribute value is other than "On"
 *  @retval false if the dpms attribute value is "On"
 */

bool dpms_check_drm_asleep_by_connector(const char * drm_connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "drm_connector_name=%s", drm_connector_name);
   assert(drm_connector_name);

   char * dpms = NULL;
   char * enabled = NULL;
   char * status  = NULL;
   int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;
   RPT_ATTR_TEXT(d, &dpms,    "/sys/class/drm", drm_connector_name, "dpms");
   RPT_ATTR_TEXT(d, &enabled, "/sys/class/drm", drm_connector_name, "enabled");
   RPT_ATTR_TEXT(d, &status,  "/sys/class/drm", drm_connector_name, "status");
   // Nvidia driver reports enabled value as "disabled"
   // asleep = !( streq(dpms, "On") && streq(enabled, "enabled") );
   bool asleep = !streq(dpms, "On");
   free(dpms);
   free(enabled);
   free(status);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, asleep, "");
   return asleep;
}


bool dpms_check_drm_asleep_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   assert(businfo);
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = /dev/i2c-%d, flags: %s",
         businfo->busno, i2c_interpret_bus_flags_t(businfo->flags));

   bool asleep = false;
   // ASSERT_IFF( !(businfo->flags&I2C_BUS_DRM_CONNECTOR_CHECKED), !businfo->drm_connector_found_by);

   if (!(businfo->flags&I2C_BUS_DRM_CONNECTOR_CHECKED)) {
      Sys_Drm_Connector * conn =  i2c_check_businfo_connector(businfo);
      if (!conn) {
        DBGTRC_NOPREFIX(debug, TRACE_GROUP, "i2c_check_businfo_connector() failed for bus %d", businfo->busno);
        SYSLOG2(DDCA_SYSLOG_ERROR, "i2c_check_businfo_connector() failed for bus %d", businfo->busno);
      }
      else {
        assert(businfo->drm_connector_name);
      }
   }
   if (businfo->drm_connector_name) {
      asleep = dpms_check_drm_asleep_by_connector(businfo->drm_connector_name);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, asleep, "");
   return asleep;
}


bool dpms_check_drm_asleep_by_dref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref = %s", dref_repr_t(dref));

   bool result = dpms_check_drm_asleep_by_businfo((I2C_Bus_Info*) dref->detail);

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
}


void init_i2c_dpms() {
   RTTI_ADD_FUNC(dpms_check_drm_asleep_by_businfo);
   RTTI_ADD_FUNC(dpms_check_drm_asleep_by_dref);
   RTTI_ADD_FUNC(dpms_check_drm_asleep_by_connector);
   // RTTI_ADD_FUNC(dpms_check_x11_asleep);
}


