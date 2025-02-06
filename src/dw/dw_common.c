/** @file dw_common.c */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include "util/common_inlines.h"
#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/drm_common.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/i2c_util.h"
#include "util/linux_util.h"
#include "util/msg_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#include "util/traced_function_stack.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_errno.h"
#include "base/drm_connector_state.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
#include "base/sleep.h"
/** \endcond */

#include "sysfs/sysfs_base.h"
#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_sys_drm_connector.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"

#include "dw_status_events.h"
#include "dw_dref.h"
#include "dw_xevent.h"

#include "dw_common.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

uint16_t  initial_stabilization_millisec = DEFAULT_INITIAL_STABILIZATION_MILLISEC;
uint16_t  stabilization_poll_millisec    = DEFAULT_STABILIZATION_POLL_MILLISEC;
uint16_t  udev_watch_loop_millisec       = DEFAULT_UDEV_WATCH_LOOP_MILLISEC;
uint16_t  poll_watch_loop_millisec       = DEFAULT_POLL_WATCH_LOOP_MILLISEC;
uint16_t  xevent_watch_loop_millisec     = DEFAULT_XEVENT_WATCH_LOOP_MILLISEC;

bool      terminate_using_x11_event      = false;


uint32_t dw_calc_watch_loop_millisec(DDCA_Watch_Mode watch_mode) {
   assert(watch_mode != Watch_Mode_Dynamic);
   int final_answer = 0;

   switch (watch_mode) {
   case Watch_Mode_Udev:   final_answer = udev_watch_loop_millisec;   break;
   case Watch_Mode_Xevent: final_answer = xevent_watch_loop_millisec; break;
   case Watch_Mode_Poll:   final_answer = poll_watch_loop_millisec;   break;
   case Watch_Mode_Dynamic:
        PROGRAM_LOGIC_ERROR("watch_mode == Watch_Mode_Dynamic");
   }

   return final_answer;
}


/** Performs a sleep in short segments so that it can be responsively terminated
 *  when dw_stop_watch_displays() is called. Each segment is no longer than
 *  200 milliseconds.
 *
 *  @param  watch_loop_millisec  intended total milliseconds to sleep
 *  @return actual total milliseconds
 */
uint32_t dw_split_sleep(int watch_loop_millisec) {
   assert(watch_loop_millisec > 0);
   uint64_t max_sleep_microsec = watch_loop_millisec * (uint64_t)1000;
   uint64_t sleep_step_microsec = MIN(200, max_sleep_microsec);     // .2 sec
   uint64_t slept = 0;
   for (; slept < max_sleep_microsec && !terminate_watch_thread; slept += sleep_step_microsec)
      usleep(sleep_step_microsec);
   return slept/1000;
}


void dw_terminate_if_invalid_thread_or_process(pid_t cur_pid, pid_t cur_tid) {
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
    }
    if (!pid_found || !tid_found) {
       free_current_traced_function_stack();
       g_thread_exit(GINT_TO_POINTER(-1));
    }
}


void dw_free_watch_displays_data(Watch_Displays_Data * wdd) {
   if (wdd) {
      assert( memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
      wdd->marker[3] = 'x';
      free(wdd->evdata);
      free(wdd);
   }
}



void dw_free_recheck_displays_data(Recheck_Displays_Data * rdd) {
   if (rdd) {
      assert( memcmp(rdd->marker, RECHECK_DISPLAYS_DATA_MARKER, 4) == 0 );
      rdd->marker[3] = 'x';
      free(rdd);
   }
}


Callback_Displays_Data * dw_new_callback_displays_data() {
   Callback_Displays_Data * cdd = calloc(1, sizeof(Callback_Displays_Data));
   cdd->main_process_id = PID();
   return cdd;
}

void dw_free_callback_displays_data(Callback_Displays_Data * cdd) {
   if (cdd) {
      assert( memcmp(cdd->marker, CALLBACK_DISPLAYS_DATA_MARKER, 4) == 0 );
      cdd->marker[3] = 'x';
      free(cdd);
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


void dw_emit_deferred_events(GArray * deferred_events) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

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
      dw_emit_display_status_record(evt);
   }
   g_array_remove_range(deferred_events,0, deferred_events->len);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


#ifdef WATCH_ASLEEP
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
         bool is_dpms_asleep = dpms_check_drm_asleep_by_businfo(businfo);
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
#endif


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
bool dw_hotplug_change_handler(
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray *       events_queue,
      GPtrArray*     drefs_to_recheck)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "bs_buses_w_edid_removed: %s",
         BS256_REPR(bs_buses_w_edid_removed));
   if (IS_DBGTRC(debug, TRACE_GROUP)) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_buses_w_edid_added: %s",
            BS256_REPR(bs_buses_w_edid_added));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "events_queue=%p",
            events_queue);
   }
   // debug_current_traced_function_stack(false);   // ** TEMP **/

   bool event_emitted = true;

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      //  i2c_dbgrpt_buses(false, false, 1);
      DBGMSG("buses before event processed:");
      i2c_dbgrpt_buses_summary(1);
      DBGMSG("display references before event processed:");
      // ddc_dbgrpt_display_refs_summary(true,     // include_invalid_displays
      //                                 false,    // report_businfo
      //                                 1);       // depth
      ddc_dbgrpt_display_refs_terse(true, 1);
      rpt_nl();
   }

   Bit_Set_256_Iterator iter = bs256_iter_new(bs_buses_w_edid_removed);
   while(true) {
      int busno = bs256_iter_next(iter);
      if (busno < 0)
         break;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Removing bus %d", busno);
      I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
      Display_Ref* dref = dw_remove_display_by_businfo(businfo);
      if (dref) {
         dw_emit_or_queue_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
               dref->drm_connector, dref, dref->io_path, events_queue);
         event_emitted = true;
      }
      if (i2c_device_exists(busno)) {
         // i2c_reset_bus_info(businfo);  // already done in ddc_remove_display_by_businfo2()
      }
      else {
         // is this possible?
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Device /dev/i2c-%d no longer exists.", busno);
         i2c_remove_bus_by_busno(busno);
      }
   }
   bs256_iter_free(iter);

   iter = bs256_iter_new(bs_buses_w_edid_added);
   while (true) {
      int busno = bs256_iter_next(iter);
      if (busno < 0)
         break;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding display ref for bus: %d", busno);
       // need to protect ?
      I2C_Bus_Info * businfo = i2c_get_and_check_bus_info(busno);

       char buf[100];
       g_snprintf(buf, 100, "Adding connected display with bus %d", busno);
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
       SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf);
       DDCA_IO_Path path;
       path.io_mode = DDCA_IO_I2C;
       path.path.i2c_busno = busno;
       Display_Ref* dref = dw_add_display_by_businfo(businfo);
       if (dref && !(dref->flags& DREF_TRANSIENT)) {
          add_published_dref_id_by_dref(dref);
          if (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING) && drefs_to_recheck) {
             DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding %s to drefs_to_recheck", dref_reprx_t(dref));
             g_ptr_array_add(drefs_to_recheck, dref);
          }
          dw_emit_or_queue_display_status_event(
             DDCA_EVENT_DISPLAY_CONNECTED, businfo->drm_connector_name, dref, path, events_queue);
          event_emitted = true;
       }
       else {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Newly detected display has disappeared!!!");
          event_emitted = false;
      }
   }
   bs256_iter_free(iter);

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      rpt_nl();
      rpt_label(0,"After buses added or removed:");
      // i2c_dbgrpt_buses(false, false, 1);
      i2c_dbgrpt_buses_summary(1);
      rpt_label(0,"After display refs added or marked disconnected:");
      // ddc_dbgrpt_display_refs_summary(true,     // include_invalid_displays
      //                                 false,    // report_businfo
      //                                 1);       // depth
      ddc_dbgrpt_display_refs_terse(true, 1);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP,event_emitted, "");
   // debug_current_traced_function_stack(false);   // ** TEMP **/
   return event_emitted;
}


#ifdef OLD
/** Repeatedly calls i2c_detect_buses0() until the value read equals the prior value.
 *
 *  @oaram prior                       initial array of I2C_Bus_Info for connected buses
 *  @param some_displays_disconnected  if true, add delay to avoid bogus disconnect/connect sequence
 *  @return stabilized array of Bus_Info for connected buses
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
      if (initial_stabilization_millisec > 0) {
         char * s = g_strdup_printf(
               "Delaying %d milliseconds to avoid a false disconnect/connect sequence...", initial_stabilization_millisec);
         DBGTRC(debug, TRACE_GROUP, "%s", s);
         SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);
         free(s);
         usleep(initial_stabilization_millisec * 1000);
      }
   }

   int stablect = 0;
   bool stable = false;
   while (!stable) {
      // DBGMSG("SLEEPING");
      usleep(1000*stabilization_poll_millisec);
      GPtrArray* latest = i2c_detect_buses0();
      Bit_Set_256 bs_latest =  buses_bitset_from_businfo_array(latest, /* only_connected */ true);
      if (bs256_eq(bs_latest, bs_prior))
            stable = true;
      i2c_discard_buses0(prior);
      prior = latest;
      stablect++;
   }
   if (stablect > 1) {
      DBGTRC(debug || true, TRACE_GROUP,   "Required %d extra calls to i2c_get_buses0()", stablect+1);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s required %d extra calls to i2c_get_buses0()", __func__, stablect-1);
   }

   DBGTRC_RET_STRING(debug, DDCA_TRC_NONE, BS256_REPR(bs_prior),"");
   return prior;
}
#endif


Bit_Set_256
dw_stabilized_buses_bs(Bit_Set_256 bs_prior, bool some_displays_disconnected) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "prior =%s, some_displays_disconnected=%s, extra_stabilization_millisec=%d",
         BS256_REPR(bs_prior), SBOOL(some_displays_disconnected), initial_stabilization_millisec);
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_prior:", BS256_REPR(bs_prior));

   // Special handling for case of apparently disconnected displays.
   // It has been observed that in some cases (Samsung U32H750) a disconnect is followed a
   // few seconds later by a connect. Wait a few seconds to avoid triggering events
   // in this case.
   if (some_displays_disconnected) {
      if (initial_stabilization_millisec > 0) {
         char * s = g_strdup_printf(
               "Delaying %d milliseconds to avoid a false disconnect/connect sequence...",
               initial_stabilization_millisec);
         DBGTRC(debug, TRACE_GROUP, "%s", s);
         SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);
         free(s);
         DW_SLEEP_MILLIS(initial_stabilization_millisec,  "Initial stabilization delay");
      }
   }

   int stablect = 0;
   bool stable = false;
   while (!stable) {
      // DW_SLEEP_MILLIS(stabilization_poll_millisec, "Loop until stable"); // TMI
      sleep_millis(stabilization_poll_millisec);
      BS256 bs_latest = i2c_buses_w_edid_as_bitset();
      if (bs256_eq(bs_latest, bs_prior))
            stable = true;
      bs_prior = bs_latest;
      stablect++;
   }
   if (stablect > 1) {
      char buf[100];
      g_snprintf(buf, 100,
            "Required %d extra %d millisecond calls to i2c_buses_w_edid_as_bitset()",
            stablect+1, stabilization_poll_millisec);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%s", buf);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf);
   }

   DBGTRC_RET_STRING(debug, TRACE_GROUP, BS256_REPR(bs_prior),"");
   return bs_prior;
}


void init_dw_common() {
#ifdef WATCH_ASLEEP
   RTTI_ADD_FUNC(ddc_i2c_check_bus_asleep);
#endif
   RTTI_ADD_FUNC(dw_stabilized_buses_bs);
   RTTI_ADD_FUNC(dw_emit_deferred_events);
   RTTI_ADD_FUNC(dw_hotplug_change_handler);
}
