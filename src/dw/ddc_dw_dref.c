/** @file ddc_dw_dref.c
 *  Functions that modify persistent Display_Ref related data structures
 *  when display connection and disconnection are detected.
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 
/** \cond */
#include <assert.h>
#include <dw/ddc_dw_dref.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"

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



// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


/** Adds a Display_Ref to the array of all Display_Refs
 *  in a thread safe manner.
 *
 *  @param dref pointer to Display_Ref to add.
 */
void ddc_add_display_ref(Display_Ref * dref) {
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
void ddc_mark_display_ref_removed(Display_Ref* dref) {
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
Display_Ref * ddc_add_display_by_businfo(I2C_Bus_Info * businfo) {
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
         ddc_add_display_ref(dref);
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
Display_Ref * ddc_remove_display_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno=%d", businfo, businfo->busno);

   i2c_reset_bus_info(businfo);
   int busno = businfo->busno;

   Display_Ref * dref = DDC_GET_DREF_BY_BUSNO(businfo->busno, /*ignore_invalid*/ true);
   char buf[100];
   g_snprintf(buf, 100, "Removing connected display, dref %s", dref_repr_t(dref));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
   SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf); // *** TEMP ***
   if (dref) {
      assert(!(dref->flags & DREF_REMOVED));
      ddc_mark_display_ref_removed(dref);
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
ddc_recheck_dref(Display_Ref * dref) {
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


/** Locates the currently live Display_Ref for the specified bus.
 *  Discarded display references, i.e. ones marked removed (flag DREF_REMOVED)
 *  are ignored. There should be at most one non-removed Display_Ref.
 *
 *  @param  busno    I2C_Bus_Number
 *  @param  connector
 *  @param  ignore_invalid
 *  @return  display reference, NULL if no live reference exists
 */
Display_Ref * ddc_get_dref_by_busno_or_connector(
      int          busno,
      const char * connector,
      bool         ignore_invalid)
{
   ASSERT_IFF(busno >= 0, !connector);
   bool debug = false;
   debug = debug || debug_locks;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno = %d, connector = %s, ignore_invalid=%s",
                                       busno, connector, SBOOL(ignore_invalid));
   assert(all_display_refs);

   Display_Ref * result = NULL;
   int non_removed_ct = 0;
   uint64_t highest_non_removed_creation_timestamp = 0;
   // lock entire function on the extremely rare possibility that recovery
   // will mark a display ref removed
   g_mutex_lock(&all_display_refs_mutex);
   for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
      // If a display is repeatedly removed and added on a particular connector,
      // there will be multiple Display_Ref records.  All but one should already
      // be flagged DDCA_DISPLAY_REMOVED,
      // ?? and should not have a pointer to an I2C_Bus_Info struct.

      Display_Ref * cur_dref = g_ptr_array_index(all_display_refs, ndx);
      // DBGMSG("Checking dref %s", dref_repr_t(cur_dref));

      if (ignore_invalid && cur_dref->dispno <= 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_dref=%s@%p dispno < 0, Ignoring",
               dref_repr_t(cur_dref), cur_dref);
         continue;
      }

      // I2C_Bus_Info * businfo = (I2C_Bus_Info*) cur_dref->detail;
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "DREF_REMOVED=%s, dref_detail=%p -> /dev/i2c-%d",
      //       sbool(cur_dref->flags&DREF_REMOVED), cur_dref->detail,  businfo->busno);

      if (ignore_invalid && (cur_dref->flags&DREF_REMOVED)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_dref=%s@%p DREF_REMOVED set, Ignoring",
                dref_repr_t(cur_dref), cur_dref);
         continue;
      }

      if (cur_dref->io_path.io_mode != DDCA_IO_I2C) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_dref=%s@%p io_mode != DDCA_IO_I2C, Ignoring",
                dref_repr_t(cur_dref), cur_dref);
         continue;
      }

      if (connector)   {   // consistency check
         I2C_Bus_Info * businfo = cur_dref->detail;
         if (businfo) {
            assert(streq(businfo->drm_connector_name, cur_dref->drm_connector));
         }
         else {
            SEVEREMSG("active display ref has no bus info");
         }
      }

      if ( (busno >= 0 && cur_dref->io_path.path.i2c_busno == busno) ||
           (connector  && streq(connector, cur_dref->drm_connector) ) )
      {
         // the match should only happen once, but count matches as check
         non_removed_ct++;
         if (cur_dref->creation_timestamp > highest_non_removed_creation_timestamp) {
            highest_non_removed_creation_timestamp = cur_dref->creation_timestamp;
            result = cur_dref;
         }
      }
   }
   // assert(non_removed_ct <= 1);
   if (non_removed_ct > 1) {
      if (!ignore_invalid) {
         // don't try to recover from this very very very rare case
         assert(non_removed_ct <= 1);
      }
      SEVEREMSG("Multiple non-removed displays on device %s detected. "
                "All but the most recent are being marked DDC_REMOVED",
                dpath_repr_t(&result->io_path));
      for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
         Display_Ref * cur_dref = g_ptr_array_index(all_display_refs, ndx);
         if (ignore_invalid && cur_dref->dispno <= 0)
            continue;
         if (ignore_invalid && (cur_dref->flags&DREF_REMOVED))
            continue;
         if (cur_dref->io_path.io_mode != DDCA_IO_I2C)
            continue;
         if ( (busno >= 0 && cur_dref->io_path.path.i2c_busno == busno) ||
              (connector  && streq(connector, cur_dref->drm_connector) ) )
         {
            if (cur_dref->creation_timestamp < highest_non_removed_creation_timestamp) {
               SEVEREMSG("Marking dref %s removed", dref_reprx_t(cur_dref));
               //ddc_mark_display_ref_removed(cur_dref);
               cur_dref->flags |= DREF_REMOVED;
            }
         }
      }
   }
   g_mutex_unlock(&all_display_refs_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p= %s", result, dref_repr_t(result));
   return result;
}


#ifdef UNUSED
Display_Ref *
ddc_get_display_ref_by_drm_connector(
      const char * connector_name,
      bool         ignore_invalid)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "connector_name=%s, ignore_invalid=%s", connector_name, sbool(ignore_invalid));
   Display_Ref * result = NULL;
   TRACED_ASSERT(all_display_refs);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "all_displays->len=%d", all_display_refs->len);
   for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
      Display_Ref * cur = g_ptr_array_index(all_display_refs, ndx);
      // ddc_dbgrpt_display_ref(cur, 4);
      bool pass_filter = true;
      if (ignore_invalid) {
         pass_filter = (cur->dispno > 0 || !(cur->flags&DREF_REMOVED));
      }
      if (pass_filter) {
         if (cur->io_path.io_mode == DDCA_IO_I2C) {
            I2C_Bus_Info * businfo = cur->detail;
            if (!businfo) {
               SEVEREMSG("active display ref has no bus info");
               continue;
            }
            // TODO: handle drm_connector_name not yet checked
            if (businfo->drm_connector_name && streq(businfo->drm_connector_name,connector_name)) {
               result = cur;
               break;
            }
         }
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s = %p", dref_repr_t(result), result);
   return result;
}
#endif


void init_ddc_watch_displays_dref()  {
   RTTI_ADD_FUNC(ddc_add_display_by_businfo);
   RTTI_ADD_FUNC(ddc_add_display_ref);
   RTTI_ADD_FUNC(ddc_get_dref_by_busno_or_connector);
   RTTI_ADD_FUNC(ddc_mark_display_ref_removed);
   RTTI_ADD_FUNC(ddc_recheck_dref);
   RTTI_ADD_FUNC(ddc_remove_display_by_businfo);
}
