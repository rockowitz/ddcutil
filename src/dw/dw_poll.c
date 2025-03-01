/** @file dw_poll.c
 *
 *  Watch for display changes without using UDEV
 */

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
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/libdrm_aux_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
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

#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_sys_drm_connector.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"

#include "dw_common.h"
#include "dw_dref.h"
#include "dw_recheck.h"
#include "dw_status_events.h"
#ifdef USE_X11
#include "dw_xevent.h"
#endif

#include "dw_poll.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

int  nonudev_poll_loop_millisec = DEFAULT_UDEV_WATCH_LOOP_MILLISEC;   // 2000;
int  retry_thread_sleep_factor_millisec = WATCH_RETRY_THREAD_SLEEP_FACTOR_MILLISEC;
bool stabilize_added_buses_w_edid;  // if set, stabilize when displays added as well as removed
bool recheck_thread_active = false;
GMutex process_event_mutex;


STATIC void process_screen_change_event(
      BS256*      p_bs_attached_buses,
      BS256*      p_bs_buses_w_edid,
      GArray *    deferred_events,
      GPtrArray * displays_to_recheck
      )
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "*p_bs_old_attached_buses -> %s",
                         bs256_to_string_decimal_t(*p_bs_attached_buses, "", ","));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_CONN, "*p_bs_buses_w_edid   -> %s",
                         bs256_to_string_decimal_t(*p_bs_buses_w_edid,   "", ",")) ;

   BS256 bs_old_attached_buses = *p_bs_attached_buses;
   BS256 bs_old_buses_w_edid   = *p_bs_buses_w_edid;

   Bit_Set_256 bs_new_attached_buses = i2c_detect_attached_buses_as_bitset();
   Bit_Set_256 bs_new_buses_w_edid   = i2c_filter_buses_w_edid_as_bitset(bs_new_attached_buses);

   Bit_Set_256 bs_added_buses_w_edid     = bs256_and_not(bs_new_buses_w_edid, bs_old_buses_w_edid);
   Bit_Set_256 bs_removed_buses_w_edid   = bs256_and_not(bs_old_buses_w_edid, bs_new_buses_w_edid);
   Bit_Set_256 bs_added_attached_buses   = bs256_and_not(bs_new_attached_buses, bs_old_attached_buses);
   Bit_Set_256 bs_removed_attached_buses = bs256_and_not(bs_old_attached_buses, bs_new_attached_buses);
#ifdef TMI
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_old_buses_w_edid(0): %s",
                             bs256_to_string_decimal_t(bs_old_buses_w_edid, "", ","));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_new_buses_edid(0): %s",
                             bs256_to_string_decimal_t(bs_new_buses_w_edid, "", ","));
#endif

   if ( bs256_count(bs_removed_buses_w_edid) > 0 ||
        (stabilize_added_buses_w_edid &&  bs256_count(bs_added_buses_w_edid) > 0)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_old_attached_buses: %s", BS256_REPR(bs_old_attached_buses));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_new_attached_buses: %s", BS256_REPR(bs_new_attached_buses));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_old_buses_w_edid: %s",   BS256_REPR(bs_old_buses_w_edid));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_new_buses_edid: %s",   BS256_REPR(bs_new_buses_w_edid));

      bs_new_buses_w_edid = dw_stabilized_buses_bs(bs_new_buses_w_edid, bs256_count(bs_removed_buses_w_edid));

      BS256 bs_added_buses_w_edid     = bs256_and_not(bs_new_buses_w_edid, bs_old_buses_w_edid);
      bs_removed_buses_w_edid   = bs256_and_not(bs_old_buses_w_edid, bs_new_buses_w_edid);
      bs_added_attached_buses   = bs256_and_not(bs_new_attached_buses, bs_old_attached_buses);
      bs_removed_attached_buses = bs256_and_not(bs_old_attached_buses, bs_new_attached_buses);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "After stabilization:");
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_old_attached_buses: %s", BS256_REPR(bs_old_attached_buses));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_new_attached_buses: %s", BS256_REPR(bs_new_attached_buses));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_old_buses_w_edid:   %s", BS256_REPR(bs_old_buses_w_edid));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_new_buses_edid:     %s", BS256_REPR(bs_new_buses_w_edid));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_added_attached_buses:   %s", BS256_REPR(bs_added_attached_buses));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_removed_attached_buses:   %s", BS256_REPR(bs_removed_attached_buses));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_added_buses_w_edid: %s", BS256_REPR(bs_added_buses_w_edid));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bs_removed_buses_w_edid: %s", BS256_REPR(bs_removed_buses_w_edid));
   }
   bs_old_buses_w_edid   = bs_new_buses_w_edid;
   bs_old_attached_buses = bs_new_attached_buses;

   bool hotplug_change_handler_emitted = false;
   bool connected_buses_w_edid_changed = bs256_count(bs_removed_buses_w_edid) > 0 ||
                                       bs256_count(bs_added_buses_w_edid) > 0;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "connected_buses_changed = %s", SBOOL(connected_buses_w_edid_changed));
   if (connected_buses_w_edid_changed) {
      hotplug_change_handler_emitted = dw_hotplug_change_handler(
                                           bs_removed_buses_w_edid,
                                           bs_added_buses_w_edid,
                                           deferred_events,
                                           displays_to_recheck);
   }

   if (hotplug_change_handler_emitted)
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "hotplug_change_handler_emitted = %s",
            sbool (hotplug_change_handler_emitted));


#ifdef WATCH_ASLEEP
   if (watch_dpms) {
      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Before ddc_check_bus_asleep(), bs_sleepy_buses: %s",
      //      BS256_REPR(bs_sleepy_buses));
      // emits dpms events directly or places them on deferred_events queue
      bs_sleepy_buses = ddc_i2c_check_bus_asleep(
            bs_old_buses_w_edid, bs_sleepy_buses, deferred_events);
      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "After ddc_check_bus_asleep(), bs_sleepy_buses: %s",
      //       BS256_REPR(bs_sleepy_buses));
   }
#endif

#ifdef ALT
   if (watch_dpms) {
      for (int ndx = 0; ndx < all_i2c_buses->len; ndx++) {
          I2C_Bus_Info * businfo = g_ptr_array_index(all_i2c_buses, ndx);
          // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "ndx=%d, businfo=%p", ndx, businfo);
          // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bus=%d", businfo->busno);
          if (businfo->flags & I2C_BUS_ADDR_0X50) {
              // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bus=%d, I2C_BUS_ADDR_0X50 set", businfo->busno);
              bool is_dpms_asleep = dpms_check_drm_asleep_by_businfo(businfo);
              DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, is_dpms_asleep=%s, last_checked_dpms_asleep=%s",
                 businfo->busno, sbool(is_dpms_asleep), sbool(businfo->last_checked_dpms_asleep));
              if (is_dpms_asleep != businfo->last_checked_dpms_asleep) {
                 Display_Ref * dref = ddc_get_dref_by_busno_or_connector(businfo->busno, NULL, /*ignore_invalid*/ true);
                 assert(dref);
                 DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sleep change event for dref=%p->%s", dref, dref_repr_t(dref));
                 DDCA_Display_Event_Type event_type = (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE;
                 dw_emit_or_queue_display_status_event(event_type, dref->drm_connector, dref, dref->io_path, NULL);
                 businfo->last_checked_dpms_asleep = is_dpms_asleep;
              }
           }
       }
   }
#endif

   *p_bs_attached_buses  = bs_old_attached_buses;
   *p_bs_buses_w_edid    = bs_old_buses_w_edid;;

   DBGTRC_DONE(debug, DDCA_TRC_CONN, "*p_bs_old_attached_buses -> %s",
         bs256_to_string_decimal_t(*p_bs_attached_buses, "", ","));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_CONN, "*p_bs_buses_w_edid -> %s",
         bs256_to_string_decimal_t(*p_bs_buses_w_edid,   "", ","));
}


/** Function that executes in the display watch thread.
 *
 *  @param   data  pointer to a #Watch_Display_Data struct
 */
gpointer dw_watch_display_connections(gpointer data) {
   bool debug = false;
   bool use_deferred_event_queue = false;
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0);
   assert(wdd->watch_mode == Watch_Mode_Xevent  || wdd->watch_mode == Watch_Mode_Poll);
#ifdef USE_X11
   if (wdd->watch_mode == Watch_Mode_Xevent)
      assert(wdd->evdata);
#endif
   GPtrArray * displays_to_recheck = g_ptr_array_new();

   DBGTRC_STARTING(debug, TRACE_GROUP,
         "Caller process id: %d, caller thread id: %d, our thread id: %d, event_classes=0x%02x, terminate_using_x11_event=%s",
         wdd->main_process_id, wdd->main_thread_id, tid(), wdd->event_classes, sbool(terminate_using_x11_event));
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Watching for display connection events: %s",
         sbool(wdd->event_classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION));
#ifdef WATCH_DPMS
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Watching for dpms events: %s",
          sbool(wdd->event_classes & DDCA_EVENT_CLASS_DPMS));
#endif

   // may need to wait on startup
   while (!all_i2c_buses) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Waiting 1 sec for all_i2c_buses");
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Waiting 1 sec for all_i2c_buses");
      sleep_millis(500);
   }

   pid_t cur_pid = getpid();
   pid_t cur_tid = get_thread_id();
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Our process id: %d, our thread id: %d", cur_pid, cur_tid);

#ifdef WATCH_DPMS
   bool watch_dpms = wdd->event_classes & DDCA_EVENT_CLASS_DPMS;
   BS256 bs_sleepy_buses   = EMPTY_BIT_SET_256;
#endif
   BS256 bs_old_attached_buses = buses_bitset_from_businfo_array(all_i2c_buses, false);
   BS256 bs_old_buses_w_edid   = buses_bitset_from_businfo_array(all_i2c_buses, true);

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Initial i2c buses with edids: %s",
          BS256_REPR(bs_old_buses_w_edid));
   if (IS_DBGTRC(debug, DDCA_TRC_NONE) && false) {
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
   if (use_deferred_event_queue) {
      deferred_events = g_array_new(false,      // zero_terminated
                                    false,      // clear
                                    sizeof(DDCA_Display_Status_Event));
   }
   bool skip_next_sleep = false;
   int slept = 0;   // will contain length of final sleep

   while (!terminate_watch_thread) {
      if (deferred_events && deferred_events->len > 0) {
         dw_emit_deferred_events(deferred_events);
      }
      else {     // skip polling loop sleep if deferred events were output
         if (!skip_next_sleep && wdd->watch_mode == Watch_Mode_Poll) {
            slept = dw_split_sleep(wdd->watch_loop_millisec);
         }
      }
      skip_next_sleep = false;
      if (terminate_watch_thread)
         continue;
      dw_terminate_if_invalid_thread_or_process(cur_pid, cur_tid);

#ifdef USE_X11
      if (wdd->watch_mode == Watch_Mode_Xevent) {
         if (terminate_using_x11_event) {
            bool event_found = dw_next_X11_event_of_interest(wdd->evdata);
            // either display changed or terminate signaled
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "event_found=%s", sbool(event_found));
            if (!event_found) {
               terminate_watch_thread = true;
               continue;
            }
         }

         else {
            bool event_found = dw_detect_xevent_screen_change(wdd->evdata,  wdd->watch_loop_millisec);
            if (event_found)
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Screen change event occurred");
            else
               continue;
         }
      }
#endif

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "locking process_event_mutex");
      g_mutex_lock(&process_event_mutex);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Processing screen change event");
      process_screen_change_event(&bs_old_attached_buses, &bs_old_buses_w_edid,
                                  deferred_events, displays_to_recheck);
      if (displays_to_recheck->len > 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "handling displays_to_recheck");
         int len = displays_to_recheck->len;
         for (int ndx = len-1; ndx >= 0; ndx--) {
            dw_put_recheck_queue(g_ptr_array_index(displays_to_recheck, ndx));
            g_ptr_array_remove_index(displays_to_recheck, ndx);
         }
 #ifdef OLD
         Recheck_Displays_Data * rdd = calloc(1, sizeof(Recheck_Displays_Data));
         rdd->displays_to_recheck = displays_to_recheck;
         rdd->deferred_event_queue = deferred_events;
         g_thread_new("display_recheck_thread",             // optional thread name
                      ddc_recheck_displays_func,
                      rdd);
         displays_to_recheck = g_ptr_array_new();
#endif
      }
      g_mutex_unlock(&process_event_mutex);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "unlocked process_event_mutex");
   } // while()

   // n. slept == 0 if no sleep was performed
   DBGTRC_DONE(debug, TRACE_GROUP,
         "Terminating thread.  Final polling sleep was %d millisec.", slept/1000);
   g_ptr_array_free(displays_to_recheck, true);
   dw_free_watch_displays_data(wdd);
   if (deferred_events)
      g_array_free(deferred_events, true);
#ifdef WATCH_DPMS
   if (watch_dpms)
      g_ptr_array_free(sleepy_connectors, true);
#endif
   free_current_traced_function_stack();
   g_thread_exit(0);
   assert(false);    // avoid clang warning re wdd use after free
   return NULL;    // satisfy compiler check that value returned
}


void init_dw_poll() {
      RTTI_ADD_FUNC(dw_watch_display_connections);
      RTTI_ADD_FUNC(process_screen_change_event);
}
