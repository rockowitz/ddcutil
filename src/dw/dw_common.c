/** @file dw_common.c */

// Copyright (C) 2018-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "public/ddcutil_types.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "util/common_inlines.h"
#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/dbus_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/suspend_resume_util.h"
#include "util/timestamp.h"
#include "util/traced_function_stack.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/dw_base.h"
#include "base/i2c_bus_base.h"
#include "base/rtti.h"
#include "base/sleep.h"
/** \endcond */

#include "sysfs/sysfs_dpms.h"

#include "i2c/i2c_bus_collections.h"
#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"

#include "dw_status_events.h"
#include "dw_dref.h"
#ifdef USE_X11
#include "dw_xevent.h"
#endif

#include "dw_common.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

static const int Dbgtrc_Dbgrpt_Depth = 4;  // most closely matches dbgtrc indentation

// Timing globals are atomic to handle concurrent reads in watch
// thread and writes by ddca_set_display_settings()
_Atomic uint16_t  initial_stabilization_millisec = DEFAULT_INITIAL_STABILIZATION_MILLISEC;
_Atomic uint16_t  pause_after_add_ms              = DEFAULT_PAUSE_AFTER_ADD_MS;
_Atomic uint16_t  stabilization_poll_millisec    = DEFAULT_STABILIZATION_POLL_MILLISEC;
_Atomic uint16_t  udev_watch_loop_millisec       = DEFAULT_UDEV_WATCH_LOOP_MILLISEC;
_Atomic uint16_t  poll_watch_loop_millisec       = DEFAULT_POLL_WATCH_LOOP_MILLISEC;
_Atomic uint16_t  xevent_watch_loop_millisec     = DEFAULT_XEVENT_WATCH_LOOP_MILLISEC;

_Atomic bool  terminate_watch_thread    = false;
_Atomic bool  terminate_using_x11_event = false;
_Atomic bool  use_eventfd               = true;
_Atomic bool  split_sleep_eventfd       = true;
int            terminate_watch_thread_fd = -1;
// True while the watch thread has a screen change event waiting for, or being
// processed under, master_dw_mutex.  Recheck worker threads consult it to
// defer starting a probe, which can hold master_dw_mutex for seconds, so that
// event processing is not stalled behind them.  Set and cleared only by
// invoke_process_screen_change_event().
_Atomic bool  dw_event_processing_pending = false;
GMutex    master_dw_mutex;
GMutex    process_event_mutex;
bool      use_drm_connector_states       = false;
bool      force_recheck                  = false;
// If true, dw_process_screen_change_event() rescans when a bus open failed
// with EACCES (e.g. the post-resume window before udev reapplies device ACLs)
// instead of treating the affected monitors as disconnected.
bool      rescan_on_eacces               = true;


/** Creates the eventfd used to wake blocking waits in the watch thread
 *  when termination is requested.  No-op unless use_eventfd or
 *  split_sleep_eventfd is set, or if the eventfd already exists.  If
 *  eventfd() fails, terminate_watch_thread_fd remains -1 and the timed
 *  polling code paths are used.
 */
void dw_create_terminate_eventfd() {
   if ((use_eventfd || split_sleep_eventfd) && terminate_watch_thread_fd < 0) {
      terminate_watch_thread_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
      if (terminate_watch_thread_fd < 0)
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "eventfd() failed, errno=%d", errno);
   }
}


/** Wakes any watch thread wait blocked on the termination eventfd.
 *  Callers must set terminate_watch_thread (or otherwise arrange loop exit)
 *  first.  No-op if the eventfd does not exist.
 */
void dw_signal_terminate_eventfd() {
   if (terminate_watch_thread_fd >= 0) {
      uint64_t increment = 1;
      ssize_t rc = write(terminate_watch_thread_fd, &increment, sizeof(increment));
      if (rc < 0)
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,
               "write() to terminate_watch_thread_fd failed, errno=%d", errno);
   }
}


/** Closes the termination eventfd if it exists.  Called after the watch
 *  thread has been joined.
 */
void dw_close_terminate_eventfd() {
   if (terminate_watch_thread_fd >= 0) {
      close(terminate_watch_thread_fd);
      terminate_watch_thread_fd = -1;
   }
}


uint32_t dw_calc_watch_loop_millisec(DDC_Watch_Mode watch_mode) {
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
 *  when global #terminate_watch_thread is set (by dw_stop_watch_displays()).
 *  Each segment is no longer than 200 milliseconds.
 *
 *  If #split_sleep_eventfd is set (and the termination eventfd exists), the
 *  sleep is instead a single poll() on the termination eventfd with the
 *  intended sleep time as its timeout: one wakeup per sleep instead of one
 *  every 200 milliseconds, and immediate rather than up-to-200 ms response
 *  to termination.
 *
 *  @param  watch_loop_millisec  intended total milliseconds to sleep
 *  @return actual total milliseconds
 */
uint32_t dw_split_sleep(int watch_loop_millisec) {
   assert(watch_loop_millisec > 0);
   uint64_t max_sleep_microsec = MILLIS2MICROS(watch_loop_millisec);
   uint64_t slept = 0;

   if (split_sleep_eventfd && terminate_watch_thread_fd >= 0) {
      gint64 start_us = g_get_monotonic_time();
      int remaining_millisec = watch_loop_millisec;
      while (remaining_millisec > 0 && !terminate_watch_thread) {
         struct pollfd fds;
         fds.fd = terminate_watch_thread_fd;
         fds.events = POLLIN;
         int rc = poll(&fds, 1, remaining_millisec);
         if (rc > 0)        // termination signaled
            break;
         if (rc == 0)       // full interval slept
            break;
         if (errno != EINTR) {
            DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,
                  "poll() on terminate_watch_thread_fd failed, errno=%d", errno);
            break;
         }
         // interrupted by a signal: sleep the remainder
         uint64_t elapsed_us = g_get_monotonic_time() - start_us;
         remaining_millisec = watch_loop_millisec - MICROS2MILLIS(elapsed_us);
      }
      slept = g_get_monotonic_time() - start_us;
      if (slept > max_sleep_microsec)
         slept = max_sleep_microsec;
   }

   else {
      uint64_t sleep_step_microsec = MILLIS2MICROS(200);   // normal sleep step is .2 seconds
      if (sleep_step_microsec > max_sleep_microsec)        // but can't exceed the max sleep time
         sleep_step_microsec = max_sleep_microsec;
      for (; slept < max_sleep_microsec && !terminate_watch_thread; slept += sleep_step_microsec)
         usleep(sleep_step_microsec);
   }

   return MICROS2MILLIS(slept);
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
    if (!tid_found) {
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
#ifdef USE_X11
      if (wdd->evdata)
         dw_deinit_xevent_screen_change_notification(wdd->evdata);
#endif
      free(wdd);
   }
}


#ifdef UNUSED
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
#endif


#ifdef UNUSED
void ddc_i2c_filter_sleep_events(GArray * events) {
   assert(events);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "Initial events queue length: %d", events->len);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      for (int ndx = 0; ndx < events->len; ndx++) {
         DDCA_Display_Status_Event evt = g_array_index(events, DDCA_Display_Status_Event, ndx);
         DBGMSG("%s", display_status_event_repr_t(evt));
      }
   }
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
             if (evt2.event_type == DDCA_EVENT_DPMS_ASLEEP) {
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
            DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Filtered out sleep event: %s", display_status_event_repr_t(evt_asleep));
            DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Filtered out sleep event: %s", display_status_event_repr_t(evt_awake));
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
 *  @param bs_active_buses   bit set of all buses having edid
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
   // two lines so bs256_to_decimal_t() calls don't clobber private thread specific buffer
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
      if (!businfo || !businfo->drm_connector_name) {
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


/** Updates persistent data structures for bus changes and either
 *  emits change events or queues them for later processing.
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
      Bit_Set_256    bs_attached_buses_removed,
      Bit_Set_256    bs_attached_buses_added,
      Bit_Set_256    bs_buses_w_edid_removed,
      Bit_Set_256    bs_buses_w_edid_added,
      GArray *       events_queue,
      GPtrArray*     drefs_to_recheck)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (IS_DBGTRC(debug, TRACE_GROUP)) {

      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_attached_buses_removed: %s",
            BS256_REPR(bs_attached_buses_removed));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_attached_buses_added: %s",
            BS256_REPR(bs_attached_buses_added));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_buses_w_edid_removed: %s",
            BS256_REPR(bs_buses_w_edid_removed));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "bs_buses_w_edid_added: %s",
            BS256_REPR(bs_buses_w_edid_added));
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "events_queue=%p",
            events_queue);
   }
   // debug_current_traced_function_stack(false);   // ** TEMP **/

   bool emitted = false;
   Error_Info * err = NULL;

#define  MBSZ 100
   char msgbuf[MBSZ];

#ifdef NOT_HERE
   if (bs256_count(bs_attached_buses_removed) > 0 ) {
      Bit_Set_256_Iterator iter = bs256_iter_new(bs_attached_buses_removed);
      while(true) {
         int busno = bs256_iter_next(iter);
         if (busno < 0)
            break;

         i2c_remove_bus_by_busno(busno);
      }
   }

   if (bs256_count(bs_attached_buses_added) > 0 ) {
      Bit_Set_256_Iterator iter = bs256_iter_new(bs_attached_buses_added);
      while(true) {
         int busno = bs256_iter_next(iter);
         if (busno < 0)
            break;

         I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
         assert(businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_CHECKED);
         businfo->flags = I2C_BUS_EXISTS;
         DBGMSF(debug, "Valid bus: /dev/"I2C"-%d", busno);
         Error_Info * errs = i2c_check_bus(businfo);
         if (errs) {
            // dump to syslog
         }
         else {
            // if there's already a businfo rec in all_i2c_buses, it's an error

            g_ptr_array_add(all_i2c_buses, businfo);
         }
      }
   }

   bool event_emitted = false;


#endif

   // Buses with EDID, removed

   Bit_Set_256_Iterator iter = bs256_iter_new(bs_buses_w_edid_removed);
   while(true) {
      int busno = bs256_iter_next(iter);
      if (busno < 0)
         break;

      I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
      if (!businfo) {
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "Failed to find I2C_BUS_INFO for /dev/i2c-%d", busno);
         continue;
      }

      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Removing bus %d from set of buses with EDID", busno);
      Display_Ref* dref = dw_remove_display_by_businfo(businfo);
      if (dref) {
         dw_emit_or_queue_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
               dref->drm_connector, dref, dref->io_path, events_queue);
         emitted = true;
      }

      if (bs256_contains(bs_attached_buses_removed, busno)) {
         char * s = g_strdup_printf("Removing /dev/i2c-%d which no longer exists.", busno);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%s", s);
         DECORATED_SYSLOG(DDCA_SYSLOG_WARNING, "%s", s);
         free(s);
         i2c_remove_businfo_by_busno(busno);
      }
      else {
         i2c_reset_bus_info(businfo);
      }
   }
   bs256_iter_free(iter);

   // Attached buses removed
   // Handle removed buses that were not already handled because they had an edid

   if (bs256_count(bs_attached_buses_removed) > 0 ) {
       Bit_Set_256_Iterator iter2 = bs256_iter_new(bs_attached_buses_removed);
       while(true) {
          int busno = bs256_iter_next(iter2);
          if (busno < 0)
             break;

          // if already handled, nothing to do
          if (!bs256_contains(bs_buses_w_edid_removed, busno)) {
             i2c_remove_businfo_by_busno(busno);
          }
       }
       bs256_iter_free(iter2);
    }

   // Buses with EDID added

   iter = bs256_iter_new(bs_buses_w_edid_added);
   while (true) {
      int busno = bs256_iter_next(iter);
      if (busno < 0)
         break;

      I2C_Bus_Info * businfo =  i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
      if (!bs256_contains(bs_attached_buses_added, busno)) {
         // simple case, monitor connected to existing bus
         if (!businfo) {
            // report error, recover
            g_snprintf(msgbuf, MBSZ,
                "Businfo struct not found for pre-exisiting bus %d. Creating new businfo", busno);
            DUAL_MSGNV(DDCA_SYSLOG_ERROR, "%s", msgbuf);
            businfo = i2c_new_bus_info(busno);
            businfo->flags = I2C_BUS_EXISTS;
            i2c_add_businfo(businfo);
         }
         else {
            i2c_reset_bus_info(businfo);
         }
         err = i2c_check_bus(businfo, EDID_EXISTS);
         if (err) {
            g_snprintf(msgbuf, MBSZ,
                 "Unexpected error checking bus %d: %s", busno, errinfo_summary(err));
            DUAL_MSGNV(DDCA_SYSLOG_ERROR, "%s", msgbuf);
            ERRINFO_FREE_WITH_REPORT(err, false);
            /* coverity[UNUSED_VALUE] defensive null-after-free; err is reassigned before its next read */
            err = NULL;
         }
      }

      else {   //edid on newly attached bus
         // newly attached bus
         if (businfo) {
            g_snprintf(msgbuf, MBSZ,
               "Replacing existing businfo that should not exist for bus %d being added!!!", busno);
            DUAL_MSGNV(DDCA_SYSLOG_ERROR, "%s", msgbuf);
            i2c_remove_businfo(businfo);
         }
         businfo = i2c_new_bus_info(busno);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding /dev/"I2C"-%d to set of buses", busno);
         businfo->flags = I2C_BUS_EXISTS;
         err = i2c_check_bus(businfo, EDID_EXISTS);
         if (err) {
            g_snprintf(msgbuf, MBSZ,
                  "Unexpected error checking bus %d: %s", busno, errinfo_summary(err));
            DUAL_MSGNV(DDCA_SYSLOG_ERROR, "%s", msgbuf);
            ERRINFO_FREE_WITH_REPORT(err, false);
            /* coverity[UNUSED_VALUE] defensive null-after-free; err is reassigned before its next read */
            err = NULL;
         }
         i2c_add_businfo(businfo);
      }

      // Create display ref and emit
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Adding display ref for bus: %d", busno);
      g_snprintf(msgbuf, MBSZ, "Adding connected display with bus %d", busno);
      DUAL_MSGNV(DDCA_SYSLOG_NOTICE, "%s", msgbuf);
      Display_Ref* dref = dw_add_display_by_businfo(businfo);
      if (dref && !(dref->flags & DREF_TRANSIENT)) {   // how could it be transient here???
         add_published_dref_id_by_dref(dref);
         if (   businfo->flags & I2C_BUS_ADDR_X37
             && (!(dref->flags & DREF_DDC_COMMUNICATION_WORKING) || force_recheck)
             && drefs_to_recheck)
         {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding %s to drefs_to_recheck", dref_reprx_t(dref));
            g_ptr_array_add(drefs_to_recheck, dref);
         }
         DDCA_IO_Path path;
         path.io_mode = DDCA_IO_I2C;
         path.path.i2c_busno = busno;
         dw_emit_or_queue_display_status_event(
            DDCA_EVENT_DISPLAY_CONNECTED, businfo->drm_connector_name, dref, path, events_queue);
         emitted = true;
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Newly detected display has disappeared!!!");
      }
   }
   bs256_iter_free(iter);

   // Buses without edid attached
   iter = bs256_iter_new(bs_attached_buses_added);
   while (true) {
      int busno = bs256_iter_next(iter);
      if (busno < 0)
         break;

      if (bs256_contains(bs_buses_w_edid_added, busno)) {
         continue;    // already processed
      }
      I2C_Bus_Info* businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS;
      i2c_add_businfo(businfo);
   }
   bs256_iter_free(iter);

   bool saved_reports_to_syslog = redirect_reports_to_syslog;
   redirect_reports_to_syslog = true;
   rpt_label(Dbgtrc_Dbgrpt_Depth, "After buses added or removed:");
   i2c_dbgrpt_buses_summary(Dbgtrc_Dbgrpt_Depth+1);
   rpt_label(Dbgtrc_Dbgrpt_Depth, "After display refs added or marked disconnected:");
   ddc_dbgrpt_display_refs_terse(true, Dbgtrc_Dbgrpt_Depth+1);
   redirect_reports_to_syslog = saved_reports_to_syslog;


#ifdef HUH
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      //  i2c_dbgrpt_buses(false, false, 1);
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "buses before event processed:");
      i2c_dbgrpt_buses_summary(Dbgtrc_Dbgrpt_Depth);
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "display references before event processed:");
      // ddc_dbgrpt_display_refs_summary(true,     // include_invalid_displays
      //                                 false,    // report_businfo
      //                                 1);       // depth
      ddc_dbgrpt_display_refs_terse(true, Dbgtrc_Dbgrpt_Depth);
      // rpt_nl();
   }
#endif

   DBGTRC_RET_BOOL(debug, TRACE_GROUP,emitted, "");
   // debug_current_traced_function_stack(false);   // ** TEMP **/
   return emitted;
}


#ifdef OLD
/** Repeatedly calls i2c_detect_buses0() until the value read equals the prior value.
 *
 *  @param prior                       initial array of I2C_Bus_Info for connected buses
 *  @param some_displays_disconnected  if true, add delay to avoid bogus disconnect/connect sequence
 *  @return stabilized array of Bus_Info for connected buses
 */
GPtrArray *
ddc_i2c_stabilized_buses(GPtrArray* prior, bool some_displays_disconnected) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "prior =%p, some_displays_disconnected=%s",
         prior, SBOOL(some_displays_disconnected));
   Bit_Set_256 bs_prior =  buses_bitset_from_businfo_array(prior, /* only_connected */ true);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_prior: %s", BS256_REPR(bs_prior));

   // Special handling for case of apparently disconnected displays.
   // It has been observed that in some cases (Samsung U32H750) a disconnect is followed a
   // few seconds later by a connect. Wait a few seconds to avoid triggering events
   // in this case.
   if (some_displays_disconnected) {
      if (initial_stabilization_millisec > 0) {
         char * s = g_strdup_printf(
               "Delaying %d milliseconds to avoid a false disconnect/connect sequence...", initial_stabilization_millisec);
         DBGTRC(debug, TRACE_GROUP, "%s", s);
         DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s", s);
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
      DBGTRC(debug, TRACE_GROUP, "Required %d extra calls to i2c_get_buses0()", stablect+1);
      DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s required %d extra calls to i2c_get_buses0()", __func__, stablect-1);
   }

   DBGTRC_RET_STRING(debug, DDCA_TRC_NONE, BS256_REPR(bs_prior),"");
   return prior;
}
#endif


Bit_Set_256
dw_stabilized_buses_bs(Bit_Set_256 bs_prior, bool some_displays_disconnected) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "prior =%s, some_displays_disconnected=%s, initial_stabilization_millisec=%d",
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
         DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s", s);
         free(s);
         SLEEP_MILLIS_WITH_SYSLOG(initial_stabilization_millisec,  "Initial stabilization delay");
      }
   }

   int stablect = 0;
   bool stable = false;
   while (!stable) {
      // DW_SLEEP_MILLIS(stabilization_poll_millisec, "Loop until stable"); // TMI
      SLEEP_MILLIS_WITH_STATS(stabilization_poll_millisec);
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
            stablect-1, stabilization_poll_millisec);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%s", buf);
      DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s", buf);
   }

   DBGTRC_RET_STRING(debug, TRACE_GROUP, BS256_REPR(bs_prior),"");
   return bs_prior;
}


GMutex       active_callback_threads_mutex;
GHashTable * active_callback_threads;


void record_active_callback_thread(GThread* pthread){
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "pthread=%p", pthread);

   g_mutex_lock(&active_callback_threads_mutex);
   if (!active_callback_threads)
      active_callback_threads = g_hash_table_new(g_direct_hash, g_direct_equal);
   g_hash_table_add(active_callback_threads, pthread);
   g_mutex_unlock(&active_callback_threads_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "pthread=%p", pthread);
}


void remove_active_callback_thread(GThread* pthread){
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "pthread=%p", pthread);

   g_mutex_lock(&active_callback_threads_mutex);
   if (active_callback_threads)
      g_hash_table_remove(active_callback_threads, pthread);
   g_mutex_unlock(&active_callback_threads_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "pthread=%p", pthread);
}


int  active_callback_thread_ct() {
   int ct = 0;
   g_mutex_lock(&active_callback_threads_mutex);
   if (active_callback_threads)
      ct = g_hash_table_size(active_callback_threads);
   g_mutex_unlock(&active_callback_threads_mutex);
   return ct;
}


/** A pause overrunning its requested duration by more than this was not merely
 *  delayed by scheduling: for the difference, this process was not running at
 *  all, i.e. it was frozen for a suspend.  Well above the jitter of a loaded
 *  system, well below the length of any suspend cycle.
 */
#define PAUSE_SUSPENDED_SLACK_MS 1000


/** Sleeps, and reports whether a suspend spent the sleep rather than the sleep
 *  being served.
 *
 *  A settling pause exists to put time between some event and the work that
 *  follows it, most often while udev applies permissions to device nodes.  A
 *  pause begun before the kernel freezes user space does not do that.  The
 *  freeze consumes it: the sleep's deadline expires while the process is not
 *  running, and the call returns the instant the process is thawed, so the
 *  caller believes it has waited when no time has passed on the far side.
 *  Callers that must actually settle retake the pause when this reports true.
 *
 *  Measured on CLOCK_BOOTTIME, which advances across a true suspend as well as
 *  across a freeze that never reaches the point of suspending timekeeping;
 *  CLOCK_MONOTONIC would see only the latter.  dw_split_sleep()'s return value
 *  cannot serve: it is clamped to the requested duration, and its non-eventfd
 *  form is not a measurement at all.
 *
 *  @param  millisec  milliseconds to sleep, > 0
 *  @return true if the sleep overran far enough to have been spent by a suspend
 */
bool dw_sleep_spent_by_suspend(int millisec) {
   uint64_t start_ns = cur_boot_time_nanosec();
   // dw_split_sleep() rather than LOGGABLE_SLEEP(): these pauses run on the
   // watch thread, and a split sleep is interruptible at shutdown.
   dw_split_sleep(millisec);
   uint64_t elapsed_ms = NANOS2MILLIS(cur_boot_time_nanosec() - start_ns);
   return elapsed_ms > (uint64_t) millisec + PAUSE_SUSPENDED_SLACK_MS;
}


/** Pause execution if recently resumed from sleep. Permissions are assigned
 *  by udev after the /dev/i2c devices ara creaetd.
 *
 *  @param min_interval_ms Pause execution if less than
 *                         this number of ms since return from sleep
 *  @return number of milliseconds actually slept
 *
 *  If the number of milliseconds since last resumed from sleep
 *  is less than **min_iterval_ms**, sleep for the difference
 *  between the difference between of milliseconds since resume
 *  and **min_interval_ms**.
 *
 *  The pause measures itself, and is taken again if it was spent by a suspend
 *  rather than served after the resume.  See the body.
 */
int dw_pause_if_recently_resumed_from_sleep(int min_interval_ms) {
     bool debug = false;
     DBGTRC_STARTING(debug, TRACE_GROUP, "min_interval_ms = %d", min_interval_ms);

     int slept_millisec = 0;
     // Syslog only when a pause is actually taken.  This function runs on
     // every pass of the watch loop in poll mode, so an unconditional message
     // would flood the log with one line per poll interval.

     uint64_t since_resume_ms = UINT64_MAX;
     Resume_Detection detection = RESUME_DETECTED_NONE;
     bool suspended_during_pause = false;

     // The pause exists to sit between the resume and the first bus open,
     // while udev reapplies the /dev/i2c ACLs.  A pause begun before the
     // kernel freezes user space does not do that: the freeze consumes it, the
     // sleep's deadline expires while the process is not running, and the call
     // returns the instant the process is thawed.  The caller then believes it
     // has served its settling interval and opens buses immediately, which is
     // exactly the moment the ACLs are still missing.  This is reachable
     // whenever a pause is taken inside an open sleep cycle, i.e. from a
     // PrepareForSleep(true) whose freeze has not happened yet.
     //
     // So measure the pause and take it again if it overran; see
     // dw_sleep_spent_by_suspend().  Re-testing rather than simply repeating
     // lets the second pass use whatever is known by then:
     // the resume signal has usually arrived, so the wait becomes the
     // remainder of the interval measured from the resume itself.
     for (int attempt = 1; attempt <= MAX_SETTLING_PAUSE_ATTEMPTS; attempt++) {
        if (!recently_resumed_from_sleep(min_interval_ms, &since_resume_ms, &detection))
           break;
        // recently_resumed_from_sleep() reports a resume only when the elapsed
        // time is less than the interval it was passed, so the subtraction
        // below cannot wrap.
        assert(since_resume_ms < (uint64_t) min_interval_ms);

        // Sleep only the time remaining until min_interval_ms has elapsed
        // since the resume. The clocktime detector's grace window keeps
        // reporting "recently resumed" for 5 seconds, so sleeping the full
        // min_interval_ms on every call would pause redundantly for each
        // event batch in a post-resume udev burst, and again after the pause
        // already taken in dw_udev_watch() for an add event.
        int delay_ms = (int) ((uint64_t) min_interval_ms - since_resume_ms);
        // Worded from what was detected rather than fixed text.  A pause taken
        // inside an open sleep cycle can precede the suspend itself, so a
        // message asserting a resume would contradict the machine's state in
        // the log; and when a resume did occur, naming the method that saw it
        // is what makes a log of this subsystem readable.
        DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s, pausing for %d millisec",
              resume_detection_description(detection), delay_ms);

        suspended_during_pause = dw_sleep_spent_by_suspend(delay_ms);
        slept_millisec += delay_ms;

        if (!suspended_during_pause)   // the pause followed the resume, as intended
           break;
        if (terminate_watch_thread)    // shutting down, do not pause again
           break;
        DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE,
              "Pause of %d millisec was spent by a suspend, so it preceded the resume "
              "rather than following it.  Pausing again.", delay_ms);
     }
     if (suspended_during_pause && !terminate_watch_thread)
        DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE,
              "Settling pause interrupted by a suspend %d times, continuing without it",
              MAX_SETTLING_PAUSE_ATTEMPTS);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d slept_millisec", slept_millisec);
   return slept_millisec;
}


void init_dw_common() {
   RTTI_ADD_FUNC(dw_stabilized_buses_bs);
   RTTI_ADD_FUNC(dw_emit_deferred_events);
   RTTI_ADD_FUNC(dw_hotplug_change_handler);
   RTTI_ADD_FUNC(dw_pause_if_recently_resumed_from_sleep);
   RTTI_ADD_FUNC(record_active_callback_thread);
   RTTI_ADD_FUNC(remove_active_callback_thread);

   g_mutex_init(&master_dw_mutex);
}
