/** @file ddc_watch_displays.c
 *
 *  Watch for monitor addition and removal
 */

// Copyright (C) 2021-2024 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "util/drm_common.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_errno.h"
#include "base/drm_connector_state.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_dpms.h"
#include "i2c/i2c_sysfs.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_status_events.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_watch_displays.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

static bool      terminate_watch_thread = false;
static GThread * watch_thread = NULL;
static DDCA_Display_Event_Class active_classes = DDCA_EVENT_CLASS_NONE;
static GMutex    watch_thread_mutex;

DDC_Watch_Mode   ddc_watch_mode = Watch_Mode_Udev_I2C;
bool             ddc_slow_watch = false;
int              extra_stabilize_seconds = DEFAULT_EXTRA_STABILIZE_SECS;


const char * ddc_watch_mode_name(DDC_Watch_Mode mode) {
   char * result = NULL;
   switch (mode) {
   case Watch_Mode_Full_Poll:   result = "Watch_Mode_Full_Poll";   break;
   case Watch_Mode_Udev_Sysfs:  result = "Watch_Mode_Udev_Sysfs";  break;
   case Watch_Mode_Udev_I2C:    result = "Watch_Mode_Udev_I2C";    break;
   }
   return result;
}


STATIC
void free_watch_displays_data(Watch_Displays_Data * wdd) {
   if (wdd) {
      assert( memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
      wdd->marker[3] = 'x';
      free(wdd);
   }
}


/** Checks that a thread or process id is valid.
 *
 *  @param  id  thread or process id
 *  @return true if valid, false if not
 */
static bool
is_valid_thread_or_process(pid_t id) {
   bool debug = false;
   struct stat buf;
   char procfn[20];
   snprintf(procfn, 20, "/proc/%d", id);
   int rc = stat(procfn, &buf);
   bool result = (rc == 0);
   DBGMSF(debug, "File: %s, returning %s", procfn, sbool(result));
   if (!result)
      DBGMSG("!!! Returning: %s", sbool(result));
   return result;
}


#ifdef ENABLE_UDEV
#ifdef UNUSED
void set_fd_blocking(int fd) {
   int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
   assert (flags != -1);
   flags &= ~O_NONBLOCK;
   (void) fcntl(fd, F_SETFL, flags);
   assert(rc != -1);
}
#endif


//
// Variant using udev but not relying on /sys
//

#define BS256 Bit_Set_256
#define BS256_REPR(_bs) bs256_to_string_decimal_t(_bs, "", " ")

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


/** Repeatedly calls i2c_detect_buses0() until the value read
 *  equals the prior value.
 *
 *  @oaram prior                       initial array of I2C_Bus_Info for connected buses
 *  @param some_displays_disconnected  if true, add delay to avoid bogus disconnect/connect sequence
 *  @return stabilized array of of Bus_Info for connected buses
 */
GPtrArray *
ddc_i2c_stabilized_buses(GPtrArray* prior, bool some_displays_disconnected) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "prior =%p, some_displays_disconnected=%s",
         prior, SBOOL(some_displays_disconnected));
   Bit_Set_256 bs_prior =  buses_bitset_from_businfo_array(prior, /* only_connected */ true);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_prior:", BS256_REPR(bs_prior));

   // Special handling for case of apparently disconnected displays.
   // It has been observed that in some cases (Samsung U32H750) a disconnect is followed a
   // few seconds later by a connect. Wait a few seconds to avoid triggering events
   // in this case.
   if (some_displays_disconnected) {
      if (extra_stabilize_seconds > 0) {
         char * s = g_strdup_printf(
               "Delaying %d seconds to avoid a false disconnect/connect sequence...", extra_stabilize_seconds);
         DBGTRC(debug, TRACE_GROUP, "%s", s);
         SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);
         free(s);
         usleep(extra_stabilize_seconds * 1000000);
      }
   }

   int stablect = 0;
   bool stable = false;
   while (!stable) {
      usleep(1000*1000);  // 1 second
      GPtrArray* latest = i2c_detect_buses0();
      Bit_Set_256 bs_latest =  buses_bitset_from_businfo_array(latest, /* only_connected */ true);
      if (bs256_eq(bs_latest, bs_prior))
            stable = true;
      i2c_discard_buses0(prior);
      prior = latest;
      stablect++;
   }
   if (stablect > 1) {
      DBGTRC(debug, TRACE_GROUP,   "Required %d extra calls to i2c_get_buses0()", stablect+1);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s required %d extra calls to i2c_get_buses0()", __func__, stablect-1);
   }

   DBGTRC_RETURNING(debug, DDCA_TRC_NONE, BS256_REPR(bs_prior),"");
   return prior;
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
      BS256    bs_buses_w_edid_removed,
      BS256    bs_buses_w_edid_added,
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
      Display_Ref * dref = DDC_GET_DREF_BY_BUSNO(busno, /*ignore_invalid*/ true);
      I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
      i2c_reset_bus_info(businfo);
      if (dref) {
         assert(!(dref->flags & DREF_REMOVED));
         dref->flags |= DREF_REMOVED;
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
            int  busNdx = i2c_find_bus_info_index_in_gptrarray_by_busno(all_i2c_buses, busno);
            assert (busNdx >= 0);
            I2C_Bus_Info * businfo = g_ptr_array_remove_index(all_i2c_buses, busNdx);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Device /dev/i2c-%d no longer exists. Removing businfo record %p.",
                  busno, businfo);
         }

         ddc_emit_or_queue_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
               dref->drm_connector, dref, dref->io_path, events_queue);
         event_emitted = true;
      }
      else {
         char s[80];
         g_snprintf(s, 80, "Display_Ref not found for removed i2c bus: %d", busno);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", s);
         SYSLOG2(DDCA_SYSLOG_ERROR, "(%s) %s", __func__, s);
      }
      busno = bs256_iter_next(iter);
   }
   bs256_iter_free(iter);

   iter = bs256_iter_new(bs_buses_w_edid_added);
   busno = bs256_iter_next(iter);
   while (busno >= 0) {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Added bus: %d", busno);
       I2C_Bus_Info * businfo =  i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
       if (!businfo) {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding /dev/"I2C"-%d to list of buses", busno);
          if (IS_DBGTRC(debug, DDCA_TRC_NONE))
             i2c_dbgrpt_buses(true /*report_all*/, true /*include_sysinfo*/, 2);
          get_sys_drm_connectors(/*rescan*/ true);  // so that drm connector name picked up
          businfo = i2c_new_bus_info(busno);
          businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
          g_ptr_array_add(all_i2c_buses, businfo);
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


/** Compares the set of buses currently asleep with the previous list.
 *  If differences exist, either emit events directly or place them on
 *  the deferred events queue.
 *
 *  @param bs_active_bueses  bit set of all buses having edid
 *  @param bs_sleepy_buses   bit set of buses currently asleep
 *  @param events_queue      if null, emit events directly
 *                           if non-null, put events on the queue
 *  @return updated set of buses currently asleep
 */
Bit_Set_256 ddc_i2c_check_bus_asleep(
      Bit_Set_256  bs_active_buses,
      Bit_Set_256  bs_sleepy_buses,
      GArray*      events_queue) // array of DDCA_Display_Status_Event
{
   bool debug = false;
   // two lines so bs256_to_descimal_t() calls don't clobber private thread specific buffer
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bs_active_buses: %s", BS256_REPR(bs_active_buses));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_sleepy_buses: %s", BS256_REPR(bs_sleepy_buses));

// #ifdef TEMP
   // remove from the sleepy_connectors array any connector that is not currently active
   // so that it will not be marked asleep when it becomes active
   // i.e. turn off is asleep if connector no longer has a monitor
   bs_sleepy_buses = bs256_and(bs_sleepy_buses, bs_active_buses);

   if (bs256_count(bs_sleepy_buses) > 0)
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "bs_sleepy_buses after removing inactive buses: %s", BS256_REPR(bs_sleepy_buses));

   Bit_Set_256_Iterator iter = bs256_iter_new(bs_active_buses);
   int busno = bs256_iter_next(iter);
   while (busno >= 0) {
      I2C_Bus_Info * businfo = i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
      if (!businfo->drm_connector_name) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Unable to find connector for bus /dev/i2c-%d", busno);
         SEVEREMSG("Unable to find connector for bus /dev/i2c-%d", busno);
      }
      else {
         bool is_dpms_asleep = dpms_check_drm_asleep_by_connector(businfo->drm_connector_name);
         bool last_checked_dpms_asleep = bs256_contains(bs_sleepy_buses, busno);
         if (is_dpms_asleep != last_checked_dpms_asleep) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno = %d, last_checked_dpms_asleep=%s, is_dpms_asleep=%s",
               busno, sbool (last_checked_dpms_asleep), sbool(is_dpms_asleep));
            Display_Ref * dref = DDC_GET_DREF_BY_BUSNO(busno, /* ignore_invalid */ true);
            DDCA_IO_Path iopath;
            iopath.io_mode = DDCA_IO_I2C;
            iopath.path.i2c_busno = busno;
            DDCA_Display_Status_Event evt =
                  ddc_create_display_status_event(
                           (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE,
                           businfo->drm_connector_name,
                           dref,
                           iopath);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Queueing %s", display_status_event_repr_t(evt));
            g_array_append_val(events_queue,evt);

            if (is_dpms_asleep) {
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding bus %d to sleepy_connectors", busno);
               bs_sleepy_buses = bs256_insert(bs_sleepy_buses, busno);
            }
            else {
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Removing bus %d from sleepy_connectors", busno);
               bs_sleepy_buses = bs256_remove(bs_sleepy_buses, busno);
            }
         }
      }
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bottom of loop 2, active_connectors->len = %d, sleepy_connectors->len=%d",
      //      bs256_count(bs_active_buses), bs256_count(*p_bs_sleepy_buses));
      busno = bs256_iter_next(iter);
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: bs_sleepy_buses: %s",  BS256_REPR(bs_sleepy_buses));
   return bs_sleepy_buses;
}


/** Identifies the current list of buses having an edid and compares the
 *  current list with the previous one.  If differences exist, either emit
 *  events directly or place them on the deferred events queue.
 *
 *  @param bs_prev_buses_w_edid  previous set of buses have edid
 *  @param events_queue          if null, emit events directly
 *                               if non-null, put events on the queue
 *  @return updated set of buses having edid
 */
Bit_Set_256 ddc_i2c_check_bus_changes(
      Bit_Set_256 bs_prev_buses_w_edid,
      GArray *    events_queue)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bs_prev_buses_w_edid: %s", BS256_REPR(bs_prev_buses_w_edid));

   GPtrArray * new_buses = i2c_detect_buses0();
   Bit_Set_256 bs_new_buses_w_edid =  buses_bitset_from_businfo_array(new_buses, /* only_connected */ true);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_new_buses_w_edid: %s", BS256_REPR(bs_new_buses_w_edid));

   if (!bs256_eq(bs_prev_buses_w_edid, bs_new_buses_w_edid)) {
      // Detect need for special handling for case of display disconnected.
      Bit_Set_256 bs_removed = bs256_and_not(bs_prev_buses_w_edid,bs_new_buses_w_edid);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_removed: %s", BS256_REPR(bs_removed));
      bool detected_displays_removed_flag = bs256_count(bs_removed);

      GPtrArray * stabilized_buses = ddc_i2c_stabilized_buses(new_buses, detected_displays_removed_flag);
      BS256 bs_stabilized_buses_w_edid = buses_bitset_from_businfo_array(stabilized_buses, /*only_connected*/ true);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_stabilized_buses_w_edid: %s", BS256_REPR(bs_stabilized_buses_w_edid));
      new_buses = stabilized_buses;
   }

   bool hotplug_change_handler_emitted = false;
   bool connected_buses_changed = !bs256_eq( bs_prev_buses_w_edid, bs_new_buses_w_edid);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "connected_buses_changed = %s", SBOOL(connected_buses_changed));

   if (connected_buses_changed) {
      BS256 bs_buses_w_edid_removed = bs256_and_not(bs_prev_buses_w_edid, bs_new_buses_w_edid);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_buses_w_edid_removed: %s", BS256_REPR(bs_buses_w_edid_removed));

      BS256 bs_buses_w_edid_added = bs256_and_not(bs_new_buses_w_edid, bs_prev_buses_w_edid);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_buses_w_edid_added: %s", BS256_REPR(bs_buses_w_edid_added));

      hotplug_change_handler_emitted = ddc_i2c_hotplug_change_handler(
                                           bs_buses_w_edid_removed,
                                           bs_buses_w_edid_added,
                                           events_queue);
   }

   if (hotplug_change_handler_emitted)
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "hotplug_change_handler_emitted = %s",
            sbool (hotplug_change_handler_emitted));

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning Bit_Set_256: %s", BS256_REPR(bs_new_buses_w_edid));
   return bs_new_buses_w_edid;
}

/** Main loop watching for display changes. Runs as thread.
 *
 *  @param data   #Watch_Displays_Data passed from creator thread
 */
gpointer ddc_watch_displays_udev_i2c(gpointer data) {
   bool debug = false;
   bool debug_sysfs_state = false;
   bool use_deferred_event_queue = false;

   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
         "Caller process id: %d, caller thread id: %d, event_classes=0x%02x",
         wdd->main_process_id, wdd->main_thread_id, wdd->event_classes);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Watching for display connection events: %s",
         sbool(wdd->event_classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Watching for dpms events: %s",
          sbool(wdd->event_classes & DDCA_EVENT_CLASS_DPMS));

   bool watch_dpms = wdd->event_classes & DDCA_EVENT_CLASS_DPMS;

   pid_t cur_pid = getpid();
   pid_t cur_tid = get_thread_id();
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Our process id: %d, our thread id: %d", cur_pid, cur_tid);

   GPtrArray * sleepy_connectors = NULL;
   if (watch_dpms)
      sleepy_connectors = g_ptr_array_new_with_free_func(g_free);
   BS256 bs_sleepy_buses = EMPTY_BIT_SET_256;

   struct udev* udev;
   udev = udev_new();
   assert(udev);
   struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
   // Alternative subsystem devtype values that did not detect changes:
   // drm_dp_aux_dev, kernel, i2c-dev, i2c, hidraw
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // detects
   udev_monitor_enable_receiving(mon);

   // make udev_monitor_receive_device() blocking
   // int fd = udev_monitor_get_fd(mon);
   // set_fd_blocking(fd);

   // Sysfs_Connector_Names current_connector_names = get_sysfs_drm_connector_names();
   GPtrArray * cur_active_buses = all_i2c_buses;
   Bit_Set_256 bs_cur_buses_w_edid =
         buses_bitset_from_businfo_array(cur_active_buses, /*only_connected=*/ true);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Initial i2c buses with edids: %s",
         BS256_REPR(bs_cur_buses_w_edid));
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      rpt_vstring(0, "Initial I2C buses:");
      i2c_dbgrpt_buses_summary(1);
      rpt_vstring(0, "Initial Display Refs:");
      ddc_dbgrpt_display_refs_summary(true,     // include_invalid_displays
                                      false,    // report_businfo
                                      1);       // depth
      if (use_drm_connector_states) {
         rpt_vstring(0, "Initial DRM connector states");
         report_drm_connector_states_basic(/*refresh*/ true, 1);
      }
   }

   GArray * deferred_events = NULL;
   if (use_deferred_event_queue)
      deferred_events = g_array_new( false,      // zero_terminated
                                     false,       // clear
                                     sizeof(DDCA_Display_Status_Event));
   // if (IS_DBGTRC(debug_sysfs_state, DDCA_TRC_NONE)) {
   if (debug_sysfs_state) {
      rpt_label(0, "Initial sysfs state:");
      dbgrpt_sysfs_basic_connector_attributes(1);
   }

   struct udev_device * dev = NULL;
   while (true) {
      if (wdd->event_classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION) {
         dev = udev_monitor_receive_device(mon);
      }
      while ( !dev ) {
         int slept = 0;
         if (!deferred_events || deferred_events->len == 0) {
            int sleep_secs = 2;   // default sleep time on each loop
            if (ddc_slow_watch)   // for testing
               sleep_secs *= 3;
            const int max_sleep_microsec = sleep_secs * 1000000;
            const int sleep_step_microsec = MIN(200000, max_sleep_microsec);     // .2 sec
            int slept = 0;
            for (; slept < max_sleep_microsec && !terminate_watch_thread; slept += sleep_step_microsec)
               usleep(sleep_step_microsec);
         }

         if (deferred_events && deferred_events->len > 0) {
            ddc_i2c_emit_deferred_events(deferred_events);
         }

         if (terminate_watch_thread) {
            DBGTRC_DONE(debug, TRACE_GROUP,
                  "Terminating thread.  Final polling sleep was %d millisec.", slept/1000);
            free_watch_displays_data(wdd);
            g_thread_exit(0);
            assert(false);    // avoid clang warning re wdd use after free
         }

         if (watch_dpms) {
            // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Before ddc_check_bus_asleep(), bs_sleepy_buses: %s",
            //      BS256_REPR(bs_sleepy_buses));
            // emits dpms events directly or places them on deferred_events queue
            bs_sleepy_buses = ddc_i2c_check_bus_asleep(
                  bs_cur_buses_w_edid, bs_sleepy_buses, deferred_events);
            // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "After ddc_check_bus_asleep(), bs_sleepy_buses: %s",
            //       BS256_REPR(bs_sleepy_buses));
         }

         // Doesn't work to detect client crash, main thread and process remains for some time.
         // 11/2020: is this even needed since terminate_watch_thread check added?
         // #ifdef DOESNT_WORK
         bool pid_found = is_valid_thread_or_process(cur_pid);
         if (!pid_found) {
            DBGMSG("Process %d not found", cur_pid);
         }
         bool tid_found = is_valid_thread_or_process(cur_tid);
         if (!pid_found || !tid_found) {
            DBGMSG("Thread %d not found", cur_tid);
            g_thread_exit(GINT_TO_POINTER(-1));
            break;
         }
         // #endif

         if (wdd->event_classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION)
            dev = udev_monitor_receive_device(mon);
      }

      // if (debug) {
      //    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Got Device\n");
      //    // dbgrpt_udev_device(dev, /*verbose=*/false, 2);
      // }

      const char * prop_action    = udev_device_get_property_value(dev, "ACTION");     // always "changed"
      const char * prop_connector = udev_device_get_property_value(dev, "CONNECTOR");  // drm connector number
      const char * prop_devname   = udev_device_get_property_value(dev, "DEVNAME");    // e.g. /dev/dri/card0
      const char * prop_hotplug   = udev_device_get_property_value(dev, "HOTPLUG");    // always 1
      const char * attr_sysname   = udev_device_get_sysname(dev);                      // e.g. card0

      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "udev event received: ACTION: %s, CONNECTOR: %s, DEVNAME: %s, HOTPLUG: %s, sysname: %s",
            prop_action,
            prop_connector,
            prop_devname,
            prop_hotplug,     // "1"
            attr_sysname);

      // if (IS_DBGTRC(debug_sysfs_state, DDCA_TRC_NONE)) {
      if (debug_sysfs_state) {
         rpt_label(0, "/sys/class/drm state after hotplug event:");
         dbgrpt_sysfs_basic_connector_attributes(1);
         if (use_drm_connector_states) {
            rpt_vstring(0, "DRM connector states after hotplug event:");
            report_drm_connector_states_basic(/*refresh*/ true, 1);
         }
      }

      // emits display change events or queues them
      bs_cur_buses_w_edid = ddc_i2c_check_bus_changes(bs_cur_buses_w_edid, deferred_events);

      if (watch_dpms) {
         // remove buses marked asleep if they no longer have a monitor so they will
         // not be considered asleep when reconnected
         bs_sleepy_buses = bs256_and(bs_sleepy_buses, bs_cur_buses_w_edid);
      }

      udev_device_unref(dev);

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "udev event processed");
   }  // while

   udev_monitor_unref(mon);
   if (watch_dpms)
      g_ptr_array_free(sleepy_connectors, true);
   return NULL;
}

#undef BS256
#undef BS256_REPR

#endif // ENABLE_UDEV

//
// Common to all variants
//

/** Starts thread that watches for changes in display connection status.
 *
 *  \return  Error_Info struct if error:
 *           -  DDCRC_INVALID_OPERATION  watch thread already started
 *           -  DDCRC_ARG                event_classes == DDCA_EVENT_CLASS_NONE
 */
Error_Info *
ddc_start_watch_displays(DDCA_Display_Event_Class event_classes) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_mode = %s, watch_thread=%p, event_clases=0x%02x, drm_enabled=%s",
                                       ddc_watch_mode_name(ddc_watch_mode),
                                       watch_thread, event_classes, SBOOL(drm_enabled) );
   Error_Info * err = NULL;

#ifdef ENABLE_UDEV
   if (!drm_enabled) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Requires DRM video drivers");
      goto bye;
   }

   g_mutex_lock(&watch_thread_mutex);
   if (!(event_classes & (DDCA_EVENT_CLASS_DPMS|DDCA_EVENT_CLASS_DISPLAY_CONNECTION))) {
      err = ERRINFO_NEW(DDCRC_ARG, "Invalid event classes");
   }
   else if (watch_thread) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watch thread already running");
   }
   else {
      terminate_watch_thread = false;
      Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
      memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
      data->main_process_id = getpid();
      data->main_thread_id = get_thread_id();  // alt = syscall(SYS_gettid);
      // event_classes &= ~DDCA_EVENT_CLASS_DPMS;     // *** TEMP ***
      data->event_classes = event_classes;

      GThreadFunc watch_thread_func = ddc_watch_displays_udev_i2c;

      watch_thread = g_thread_new(
                       "watch_displays",             // optional thread name
                       watch_thread_func,
                       data);
      active_classes = event_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread started");
   }
   g_mutex_unlock(&watch_thread_mutex);
#endif

bye:
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "watch_thread=%p", watch_thread);
   return err;
}


/** Halts thread that watches for changes in display connection status.
 *
 *  @param   wait                if true, does not return until the watch thread exits,
 *                               if false, returns immediately
 *  @param   enabled_clases_loc  location at which to return watch classes that were active
 *  @retval  DDCRC_OK
 *  @retval  DDCRC_INVALID_OPERATION  watch thread not executing
 */
DDCA_Status
ddc_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "wait=%s, watch_thread=%p", SBOOL(wait), watch_thread );

   DDCA_Status ddcrc = DDCRC_OK;
#ifdef ENABLE_UDEV
   if (enabled_classes_loc)
      *enabled_classes_loc = DDCA_EVENT_CLASS_NONE;
   g_mutex_lock(&watch_thread_mutex);

   if (watch_thread) {
      terminate_watch_thread = true;  // signal watch thread to terminate
      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
      // usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
      if (wait)
         g_thread_join(watch_thread);

      //  g_thread_unref(watch_thread);
      watch_thread = NULL;
      if (enabled_classes_loc)
         *enabled_classes_loc = active_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread terminated.");
   }
   else {
      ddcrc = DDCRC_INVALID_OPERATION;
   }

   g_mutex_unlock(&watch_thread_mutex);
#endif

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


/** If the watch thread is currently executing returns, reports the
 *  currently active display event classes as a bit flag.
 *
 *  @param  classes_loc  where to return bit flag
 *  @retval DDCRC_OK
 *  @retval DDCRC_INVALID_OPERATION watch thread not executing
 */
DDCA_Status
ddc_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc) {
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "classes_loc = %p", classes_loc);
   DDCA_Status ddcrc = DDCRC_INVALID_OPERATION;
   *classes_loc = DDCA_EVENT_CLASS_NONE;
   g_mutex_lock(&watch_thread_mutex);
   if (watch_thread) {
      *classes_loc = active_classes;
      ddcrc = DDCRC_OK;
   }
   g_mutex_unlock(&watch_thread_mutex);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "*classes_loc=0x%02x", *classes_loc);
   return ddcrc;
}


void init_ddc_watch_displays() {
   RTTI_ADD_FUNC(ddc_start_watch_displays);
   RTTI_ADD_FUNC(ddc_stop_watch_displays);
   RTTI_ADD_FUNC(ddc_get_active_watch_classes);

#ifdef ENABLE_UDEV
#ifdef UNUSED
   RTTI_ADD_FUNC(ddc_i2c_filter_sleep_events);
#endif

   RTTI_ADD_FUNC(ddc_i2c_check_bus_changes);
   RTTI_ADD_FUNC(ddc_i2c_stabilized_buses);
   RTTI_ADD_FUNC(ddc_i2c_check_bus_asleep);
   RTTI_ADD_FUNC(ddc_i2c_emit_deferred_events);
   RTTI_ADD_FUNC(ddc_i2c_hotplug_change_handler);
   RTTI_ADD_FUNC(ddc_watch_displays_udev_i2c);
#endif
}
