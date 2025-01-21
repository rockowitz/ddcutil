/** @file dw_dref.c
 *  Functions that modify persistent Display_Ref related data structures
 *  when display connection and disconnection are detected.
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 
/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"

#include "util/backtrace.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/rtti.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_initial_checks.h"

#include "dw_dref.h"


// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


/** Adds a Display_Ref to the array of all Display_Refs
 *  in a thread safe manner.
 *
 *  @param dref pointer to Display_Ref to add.
 */
void dw_add_display_ref(Display_Ref * dref) {
   bool debug = false ;
   debug = debug || debug_locks;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "dref=%s", dref_repr_t(dref));
   g_mutex_lock(&all_display_refs_mutex);
   g_ptr_array_add(all_display_refs, dref);
   g_mutex_unlock(&all_display_refs_mutex);
   DBGTRC_DONE(debug, DDCA_TRC_CONN, "dref=%s", dref_repr_t(dref));
}


/** Marks a Display_Ref as removed, in a thread safe manner.
 *
 * @param dref pointer to Display_Ref to mark removed.
 */
void dw_mark_display_ref_removed(Display_Ref* dref) {
   bool debug = false;
   debug = debug || debug_locks;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "dref=%s", dref_repr_t(dref));
   g_mutex_lock(&all_display_refs_mutex);
   if (IS_DBGTRC(debug, debug)) {
      show_backtrace(2);
      backtrace_to_syslog(LOG_NOTICE, 2);
   }
   dref->flags |= DREF_REMOVED;
   g_mutex_unlock(&all_display_refs_mutex);
   DBGTRC_DONE(debug, DDCA_TRC_CONN, "dref=%s", dref_repr_t(dref));
}


/** If a display is present on a specified bus, adds a Display_Ref
 *  for that display.
 *
 *  @param businfo  I2C_Bus_Info record for the bus
 *  @return         true Display_Ref added, false if not.
 *
 *  @remark
 *  Called only when ddc_watch_displays_common.c handling display change
 */
Display_Ref * dw_add_display_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   assert(businfo);
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "businfo=%p, busno=%d", businfo, businfo->busno);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      i2c_dbgrpt_bus_info(businfo, /* include sysinfo*/ true, 4);
   //    ddc_dbgrpt_display_refs_summary(/* include_invalid_displays*/ true, /* report_businfo */ false, 2 );
   }

   // bool ok = false;
   Display_Ref * dref = NULL;
   Error_Info * err = NULL;

   // Sys_Drm_Connector * conrec = find_sys_drm_connector(-1, NULL, drm_connector_name);  // unused

   assert(businfo->flags & I2C_BUS_PROBED);
   // businfo->flags &= ~I2C_BUS_PROBED;
   // unnecessary, already done by i2c_get_and_check_bus_info() call in our only caller, ddc_i2c_hotplug_change_handler()
   // i2c_check_bus2(businfo);  // if display on bus was previously removed, info in businfo, particuarly EDID, will be stale
   if (!businfo->edid) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "No display detected on bus %d", businfo->busno);
   }
   else {
      dref = create_bus_display_ref(businfo->busno);
      // dref->dispno = DISPNO_INVALID;   // -1, guilty until proven innocent
      // dref->dispno = ++dispno_max;   // dispno not used in libddcutil except to indicate invalid
      dref->pedid = copy_parsed_edid(businfo->edid);
      dref->mmid  = mmk_new(
                       dref->pedid->mfg_id,
                       dref->pedid->model_name,
                       dref->pedid->product_code);

      // drec->detail.bus_detail = businfo;
      dref->detail = businfo;
      dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
      dref->flags |= DREF_DDC_IS_MONITOR;
      dref->drm_connector = g_strdup(businfo->drm_connector_name);
      dref->drm_connector_id = businfo->drm_connector_id;

      err = ddc_initial_checks_by_dref(dref, true);

      if (err && err->status_code == DDCRC_DISCONNECTED) {
         assert(dref->flags & DREF_REMOVED);
         // pathological case, monitor went away
         dref->flags |= DREF_TRANSIENT;   // allow free_display_ref() to free
         free_display_ref(dref);
         dref = NULL;
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "Display %s found on bus %d", dref_repr_t(dref), businfo->busno);
         if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING))
            dref->dispno = DISPNO_INVALID;
         else
            dref->dispno = ++dispno_max;
         dw_add_display_ref(dref);
      }
      errinfo_free(err);
   }   // edid exists


   // ddc_dbgrpt_display_refs_summary(/* include_invalid_displays*/ true, /* report_businfo */ true, 2 );
   DBGTRC_DONE(debug, DDCA_TRC_CONN, "Returning dref %s", dref_reprx_t(dref), dref);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE) && dref)
      dbgrpt_display_ref_summary(dref, /* include_businfo*/ false, 2);
   return dref;
}


/** Given a #I2C_Bus_Info instance, checks if there is a currently active #Display_Ref
 *  for that bus (i.e. one with the DREF_REMOVED flag not set).
 *  If found, sets the DREF_REMOVED flag.
 *
 *  @param  businfo
 *  @return Display_Ref
 */
Display_Ref * dw_remove_display_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno=%d", businfo, businfo->busno);

   i2c_reset_bus_info(businfo);
   int busno = businfo->busno;

   Display_Ref * dref = GET_DREF_BY_BUSNO(businfo->busno, /*ignore_invalid*/ true);
   char buf[100];
   g_snprintf(buf, 100, "Removing connected display, dref %s", dref_repr_t(dref));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
   SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf); // *** TEMP ***
   if (dref) {
      assert(!(dref->flags & DREF_REMOVED));
      dw_mark_display_ref_removed(dref);
      dref->detail = NULL;
   }
   else {
      char s[80];
      g_snprintf(s, 80, "No Display_Ref found for i2c bus: %d", busno);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,"%s", s);
      SYSLOG2(DDCA_SYSLOG_ERROR, "(%s) %s", __func__, s);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning dref=%p", dref);
   return dref;
}


Error_Info *
dw_recheck_dref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dref=%s", dref_reprx_t(dref));
   Error_Info * err = NULL;

   dref_lock(dref);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Obtained lock on %s:", dref_reprx_t(dref));
   dref->flags = 0;
   err = ddc_initial_checks_by_dref(dref, false);
   dref_unlock(dref);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Released lock on %s:", dref_reprx_t(dref));

   DBGTRC_RET_ERRINFO(debug, DDCA_TRC_NONE, err, "");
   return err;
}



void init_dw_dref()  {
   RTTI_ADD_FUNC(dw_add_display_by_businfo);
   RTTI_ADD_FUNC(dw_add_display_ref);
   RTTI_ADD_FUNC(dw_mark_display_ref_removed);
   RTTI_ADD_FUNC(dw_recheck_dref);
   RTTI_ADD_FUNC(dw_remove_display_by_businfo);
}
