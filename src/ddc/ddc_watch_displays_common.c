/** @file ddc_watch_displays_common.c */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE    // for usleep()

#include "config.h"
#include "public/ddcutil_types.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#ifdef ENABLE_UDEV
#include <libudev.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/drm_common.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/i2c_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_errno.h"
#include "base/drm_connector_state.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "i2c/i2c_sysfs_base.h"
#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_dpms.h"
#include <i2c/i2c_sys_drm_connector.h>

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_status_events.h"
#include "ddc/ddc_vcp.h"


#include "ddc_watch_displays_common.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

bool      terminate_watch_thread = false;
bool      ddc_slow_watch;

void free_watch_displays_data(Watch_Displays_Data * wdd) {
   if (wdd) {
      assert( memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
      wdd->marker[3] = 'x';
      free(wdd);
   }
}



#ifdef UNUSED
void ddc_i2c_filter_sleep_events(GArray * events) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "Initial events queue length: %d", events->len);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      for (int ndx = 0; ndx < events->len; ndx++) {
         DDCA_Display_Status_Event evt = g_array_index(events, DDCA_Display_Status_Event, ndx);
         DBGMSG("%s", display_status_event_repr_t(evt));
      }
   }
   assert(events);
   int initial_ndx = 0;
   while (initial_ndx < events->len) {
      DDCA_Display_Status_Event evt = g_array_index(events, DDCA_Display_Status_Event, initial_ndx);
      if (evt.event_type == DDCA_EVENT_DPMS_ASLEEP) {
         int asleep_ndx = initial_ndx;
         int awake_ndx = -1;
         for (int successor_ndx = initial_ndx+1; successor_ndx < events->len; successor_ndx++) {
             DDCA_Display_Status_Event evt2 = g_array_index(events, DDCA_Display_Status_Event, successor_ndx);
             DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "evt:  %s", display_status_event_repr_t(evt));
             DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "evt2: %s", display_status_event_repr_t(evt2));

             if (evt2.event_type != DDCA_EVENT_DPMS_ASLEEP && evt2.event_type != DDCA_EVENT_DPMS_AWAKE) {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Intervening non-sleep event");
                // connection events intervened, can't simplify
                break;
             }
             if (!dpath_eq(evt2.io_path, evt.io_path)) {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Different bus number");
                // for a different bus, ignore
                continue;
             }
             if (evt.event_type == DDCA_EVENT_DPMS_ASLEEP) {
                // multiple successive awake events, need to figure out logic
                // ignore for now
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Multiple DDCA_EVENT_DPMS_ASLEEP events");
                break;
             }
             awake_ndx = successor_ndx;
             break;
         }

         if (awake_ndx > 0) {   // impossible for it to be the first
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Removing events %d, %d", asleep_ndx, awake_ndx);
            DDCA_Display_Status_Event evt_asleep = g_array_index(events, DDCA_Display_Status_Event, asleep_ndx);
            DDCA_Display_Status_Event evt_awake  = g_array_index(events, DDCA_Display_Status_Event, awake_ndx);
            SYSLOG2(DDCA_SYSLOG_NOTICE, "Filtered out sleep event: %s", display_status_event_repr_t(evt_asleep));
            SYSLOG2(DDCA_SYSLOG_NOTICE, "Filtered out sleep event: %s", display_status_event_repr_t(evt_awake));
            g_array_remove_index(events, awake_ndx);
            g_array_remove_index(events, asleep_ndx);
            initial_ndx = asleep_ndx - 1;
         }
      }
      initial_ndx++;
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Final events queue length: %d", events->len);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      for (int ndx = 0; ndx < events->len; ndx++) {
         DDCA_Display_Status_Event evt = g_array_index(events, DDCA_Display_Status_Event, ndx);
         DBGMSG("%s", display_status_event_repr_t(evt));
      }
   }
}
#endif


void ddc_i2c_emit_deferred_events(GArray * deferred_events) {
   bool debug = false;

#ifdef TEMPORARY_SIMPLIFICATION
   if (deferred_events->len > 1) {  // FUTURE ENHANCMENT, filter out meaningless events
      // check for cancellation events
      for (int ndx = 0; ndx < deferred_events->len; ndx++) {
         DDCA_Display_Status_Event evt = g_array_index(deferred_events, DDCA_Display_Status_Event, ndx);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Event %d in queue: %s", ndx, display_status_event_repr_t(evt));
      }
      ddc_i2c_filter_sleep_events(deferred_events);
   }
#endif
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Emitting %d deferred events", deferred_events->len);
   for (int ndx = 0; ndx < deferred_events->len; ndx++) {
      DDCA_Display_Status_Event evt = g_array_index(deferred_events, DDCA_Display_Status_Event, ndx);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Emitting deferred event %s", display_status_event_repr_t(evt));
      ddc_emit_display_status_record(evt);
   }
   g_array_remove_range(deferred_events,0, deferred_events->len);
}


Display_Ref * ddc_remove_display_by_businfo2(I2C_Bus_Info * businfo) {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p", businfo);

   i2c_reset_bus_info(businfo);
   int busno = businfo->busno;
   Display_Ref * dref = DDC_GET_DREF_BY_BUSNO(businfo->busno, /*ignore_invalid*/ true);
   if (dref) {
      assert(!(dref->flags & DREF_REMOVED));
      ddc_mark_display_ref_removed(dref);
      // dref->detail = NULL;
      char buf[100];
      g_snprintf(buf, 100, "Removing connected display, dref %s", dref_repr_t(dref));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf); // *** TEMP ***
#ifdef MAYBE
            DDCA_Display_Status_Event evt = ddc_create_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
                                            connector_name,
                                            dref,
                                            dref->io_path);
#endif
      if (!i2c_device_exists(busno)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Device /dev/i2c-%d no longer exists.", busno);
         i2c_remove_bus_info(busno);
      }


   }
   else {
      char s[80];
      g_snprintf(s, 80, "Display_Ref not found for removed i2c bus: %d", busno);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", s);
      SYSLOG2(DDCA_SYSLOG_ERROR, "(%s) %s", __func__, s);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning dref=%p", dref);
   return dref;
}


/** Updates persistent data structures for bus changes and
 *  either emits change events or queues them for later processing.
 *
 *  For buses with edid removed, marks the display ref as removed
 *  For buses with edid added, create a new display ref.
 *
 *  @param  bs_buses_w_edid_removed
 *  @param  bs_buses_w_edid_added
 *  @param  events_queue    if non-null, put events on the queue,
 *                          otherwise, emit them directly
 *  @return true if an event was emitted or placed on the queue,
 *          false if not
 */
bool ddc_i2c_hotplug_change_handler(
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray * events_queue)
{
   bool debug = false;
   if (IS_DBGTRC(debug, TRACE_GROUP)) {
      DBGTRC_STARTING(debug, TRACE_GROUP, "bs_buses_removed: %s",
            BS256_REPR(bs_buses_w_edid_removed));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_buses_w_edid_added: %s",
            BS256_REPR(bs_buses_w_edid_added));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "events_queue=%p",
            events_queue);
   }

   bool event_emitted = true;

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      //  i2c_dbgrpt_buses(false, false, 1);
      i2c_dbgrpt_buses_summary(1);
      rpt_nl();
      ddc_dbgrpt_display_refs_summary(true,     // include_invalid_displays
                                      false,    // report_businfo
                                      1);       // depth
   }

   Bit_Set_256_Iterator iter = bs256_iter_new(bs_buses_w_edid_removed);
   int busno = bs256_iter_next(iter);
   while(busno >= 0) {
      I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
      Display_Ref* dref = ddc_remove_display_by_businfo2(businfo);
      if (dref) {
         ddc_emit_or_queue_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
               dref->drm_connector, dref, dref->io_path, events_queue);
         event_emitted = true;
      }


      busno = bs256_iter_next(iter);
   }
   bs256_iter_free(iter);

   iter = bs256_iter_new(bs_buses_w_edid_added);
   busno = bs256_iter_next(iter);
   while (busno >= 0) {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Added bus: %d", busno);
       // need to protect ?
       I2C_Bus_Info * businfo =  i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
       if (!businfo) {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding /dev/"I2C"-%d to list of buses", busno);
          // if (IS_DBGTRC(debug, DDCA_TRC_NONE))
          //    i2c_dbgrpt_buses(true /*report_all*/, true /*include_sysinfo*/, 2);
          get_sys_drm_connectors(/*rescan*/ true);  // so that drm connector name picked up
          businfo = i2c_new_bus_info(busno);
          businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
          i2c_add_bus_info(businfo);
       }
       char buf[100];
       g_snprintf(buf, 100, "Adding connected display with bus %d", busno);
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
       SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf);
       DDCA_IO_Path path;
       path.io_mode = DDCA_IO_I2C;
       path.path.i2c_busno = busno;
       Display_Ref* dref = ddc_add_display_by_businfo(businfo);
       ddc_emit_or_queue_display_status_event(
             DDCA_EVENT_DISPLAY_CONNECTED, businfo->drm_connector_name, dref, path, events_queue);
       event_emitted = true;
       busno = bs256_iter_next(iter);
   }
   bs256_iter_free(iter);

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      rpt_nl();
      rpt_label(0,"After buses added or removed:");
      // i2c_dbgrpt_buses(false, false, 1);
      i2c_dbgrpt_buses_summary(1);
      rpt_nl();
      rpt_label(0,"After display refs added or marked disconnected:");
      ddc_dbgrpt_display_refs_summary(true,     // include_invalid_displays
                                      false,    // report_businfo
                                      1);       // depth
   }
   DBGTRC_RET_BOOL(debug, TRACE_GROUP,event_emitted, "");
   return event_emitted;
}


void init_ddc_watch_displays_common() {
   RTTI_ADD_FUNC(ddc_i2c_emit_deferred_events);
   RTTI_ADD_FUNC(ddc_i2c_hotplug_change_handler);
}
