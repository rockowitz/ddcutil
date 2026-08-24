/** @file dw_udev.c
 *
 *  Watch for monitor addition and removal using UDEV
 */

// Copyright (C) 2021-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "public/ddcutil_types.h"

/** \cond */
#include <errno.h>
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/linux_util.h"
#include "util/string_util.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "sysfs/sysfs_sys_drm_connector.h"

#include "i2c/i2c_bus_core.h"
/** \endcond */

#include "dw_common.h"

#include "dw_udev.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

// globals
bool    report_udev_events = false;

static struct udev* udev = NULL;
static struct udev_monitor *mon = NULL;
static int monitor_fd= -1;


void dw_udev_setup() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   udev = udev_new();
   mon = udev_monitor_new_from_netlink(udev, "udev");
   // Alternative subsystem devtype values that did not detect changes:
   // drm_dp_aux_dev, kernel, i2c, hidraw
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);
   udev_monitor_filter_add_match_subsystem_devtype(mon, "i2c-dev", NULL);
   // udev_monitor_filter_add_match_subsystem_devtype(mon, "i2c", NULL);  // redundant with i2c-dev
   // udev_monitor_filter_add_match_subsystem_devtype(mon, "drm_dp_aux_dev", NULL);
   // udev_monitor_filter_add_match_subsystem_devtype(mon, "wakeup", NULL);
   udev_monitor_enable_receiving(mon);
   monitor_fd = udev_monitor_get_fd(mon);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void dw_udev_teardown() {
   bool debug = false;
   udev_monitor_unref(mon);
   udev_unref(udev);
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "");
}


STATIC bool exclude_event( Udev_Event_Detail * detail) {
   bool exclude = false;

#ifdef NO
   // excludes drm change events
   if (str_starts_with(detail->prop_devname, "/dev/dri"))
      exclude = true;
   if (streq(detail->prop_major, "226"))   // same as above
      exclude = true;
#endif

   return exclude;
}


/** Poll udev to watch for display connection/disconnection
 *
 *  @param  watch_loop_millisec
 *  @retval true   returning because watching terminated
 *  @retval false  display change detected
 */
bool dw_udev_watch(int watch_loop_millisec) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_loop_millisec=%d", watch_loop_millisec);
   int poll_timeout_millisec = watch_loop_millisec;
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      poll_timeout_millisec = 5000;
      DBGTRC(true, DDCA_TRC_NONE, "resetting poll_timeout_millisec to %d for testing", poll_timeout_millisec);
   }

   struct pollfd fds[2];
   fds[0].fd = monitor_fd;
   fds[0].events = POLLIN;
   nfds_t nfds = 1;
   if (use_eventfd && terminate_watch_thread_fd >= 0) {
      // block until a udev event arrives or termination is signaled;
      // the while condition rechecks terminate_watch_thread
      fds[1].fd = terminate_watch_thread_fd;
      fds[1].events = POLLIN;
      nfds = 2;
      poll_timeout_millisec = -1;
   }

   bool found = false;
   int pollctr = 0;
   bool add_event_detected = false;

   while(!found && !terminate_watch_thread) {
      int j = ++pollctr%100;
      if (j == 1)
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling poll()...(%d)", pollctr);
      int rc = poll(fds, nfds, poll_timeout_millisec);   // consider using ppol()
      if (rc == 0) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "poll() timed out");
      }
      else if (rc < 0) {
         // DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "poll() failed, errno=%d", errno);
         // DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,  "poll() failed, errno=%d", errno);
         DUAL_MSGXV(DDCA_SYSLOG_ERROR,  DDCA_TRC_NONE, "poll() failed, errno=%d", errno);
      }
      else {
         if (fds[0].revents&POLLIN) {
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
               // DBGTRC(debug, DDCA_TRC_NONE, "Udev event detected");
               // DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Udev event detected");
               DUAL_MSGX(DDCA_SYSLOG_NOTICE, DDCA_TRC_NONE, "udev event detected");

               Udev_Event_Detail * detail = collect_udev_event_detail(dev);
               if (!exclude_event(detail)) {   // currently never excludes
                  found = true;
                  if (debug || report_udev_events) {
                     dbgrpt_udev_event_basic_detail(detail,1);
                  }
                  char * connector_msg = NULL;
                  char * connector_msg2 = NULL;
                  if (detail->prop_connector) {
                     I2C_Bus_Info * businfo = NULL;
                     Sys_Drm_Connector * conn = NULL;
                     int ival = 0;
                     bool valid_int = str_to_int(detail->prop_connector, &ival, 10);
                     if (valid_int) {
                        // if this is used for more than informational purpose, need to
                        // search sysfs directly, not rely on list that may have been made
                        // invalid by an add or remove
                        conn = find_sys_drm_connector_by_connector_id(ival);
                        if (conn)
                            connector_msg2 = g_strdup_printf("prop_connector = %d -> %s",
                                  ival, conn->connector_name);
                        businfo = i2c_find_businfo_by_drm_connector_id(ival);
                        if (businfo) {
                           connector_msg = g_strdup_printf(
                               "prop_connector = %d -> /dev/i2c-%d",
                                 ival, businfo->busno);
                        }
                     }
                     if (!conn)
                        connector_msg2 = g_strdup_printf(
                              "Could not find DRM connector for connector id: %s",
                              detail->prop_connector);
                     if (!businfo)
                        connector_msg = g_strdup_printf(
                              "Could not find I2C_Bus_Info for connector id: %s",
                              detail->prop_connector);
                  }
                  GPtrArray* collector = udev_event_detail_to_collector(detail, NULL);  // allocates collector
                  if (connector_msg) {
                     g_ptr_array_add(collector, strdup(connector_msg));
                     free(connector_msg);
                  }
                  if (connector_msg2) {
                     g_ptr_array_add(collector, strdup(connector_msg2));
                     free(connector_msg2);
                  }
                  g_ptr_array_to_syslog(LOG_DEBUG, collector, /*ornament*/ true, /*tag*/ NULL);
                  g_ptr_array_free(collector, true);

                  // TODO: refine the test
                  // if (streq(detail->sysname, "i2c-dev") || streq(detail->sysnamm, "drm"))
                  if (streq(detail->prop_action, "add") &&
                      !str_starts_with(detail->prop_devname, "/dev/dri"))
                  {
                     add_event_detected = true;

                  }
               }  // !exclude_event(detail)
               free_udev_event_detail(detail);
               udev_device_unref(dev);
            }
            else {
               // DBGTRC(true, DDCA_TRC_NONE, "udev_monitor_receive_device() failed");
               // DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,  "udev_monitor_receive_device() failed");
               DUAL_MSGX(DDCA_SYSLOG_ERROR, DDCA_TRC_NONE,  "udev_monitor_receive_device() failed");
            }
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Not for us. fds[0].revents=0x%04x", fds[0].revents);
         }
      }
   }     // while()

   if (!terminate_watch_thread) {
      int already_paused_ms = 0;

      if (add_event_detected) {
         int pause_after_add_ms = pause_after_resume_ms;   // ??

         // Run the resume detection before pausing, so that its clocktime
         // reference point precedes this sleep and
         // dw_pause_if_recently_resumed_from_sleep() counts this sleep toward
         // pause_after_resume_ms instead of pausing again in full.  That
         // credit is taken on a later iteration of the watch loop, not by the
         // call below: an add event sets already_paused_ms to the full
         // interval, so the guard below is false whenever this branch ran.
         // No-op if a resume did not recently occur.
         recently_resumed_from_sleep(pause_after_resume_ms, NULL, NULL);
         LOGGABLE_SLEEP(pause_after_add_ms, SLEEP_OPT_TRACEABLE,DDCA_SYSLOG_NOTICE,
               "Pausing %d millisec after UDEV add event", pause_after_add_ms);
         already_paused_ms = pause_after_add_ms;
      }

      if (already_paused_ms < pause_after_resume_ms) {
         // n. returns the milliseconds actually slept, usually 0 if no recent resume;
         // do not credit the full interval, o.w. the coalesce pause below never runs
         already_paused_ms += dw_pause_if_recently_resumed_from_sleep(pause_after_resume_ms);
      }

      int drain_pause_ms = 200;    // ??

      int remaining_pause_ms = drain_pause_ms - already_paused_ms;
      if (remaining_pause_ms > 0) {
         LOGGABLE_SLEEP(remaining_pause_ms, SLEEP_OPT_TRACEABLE, DDCA_SYSLOG_NOTICE,
                        "Allowing time for events to coalesce");
      }

      dw_udev_drain();
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, terminate_watch_thread, "");
   return terminate_watch_thread;
}


/** Drains udev events already queued on the monitor socket, without
 *  processing them.
 *
 *  A resume from sleep causes a burst of udev events in quick succession, as
 *  every GPU/connector/i2c-dev node re-registers. Without draining them,
 *  the watch loop runs one full bus/EDID rescan per queued event instead of
 *  one rescan for the whole burst, which is expensive and shows up as a CPU
 *  spike right after resume. Coalesce: after dw_udev_watch() detects an event
 *  and its post-event pauses have completed, drain any additional events
 *  already queued (non-blocking) before rescanning, since the rescan reads
 *  current system state directly and does not depend on having seen every
 *  individual event. Called from the epilogue of dw_udev_watch(), after the
 *  pauses, so that events arriving during the pauses themselves are also
 *  coalesced into the single rescan.
 *
 *  @return number of events drained
 */
int dw_udev_drain() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   int drained_ct = 0;
   if (monitor_fd < 0)
      return drained_ct;

   struct pollfd fds;
   fds.fd = monitor_fd;
   fds.events = POLLIN;

   const int max_drain_ct = 500;   // safety bound, avoid delaying thread termination
   while (drained_ct < max_drain_ct && !terminate_watch_thread) {
      int rc = poll(&fds, 1, 0);   // non-blocking: only drain what's already queued
      if (rc <= 0 || !(fds.revents & POLLIN))
         break;
      struct udev_device * dev2 = udev_monitor_receive_device(mon);
      if (!dev2)
         break;

      bool debug_drain_detail = IS_DBGTRC(debug, DDCA_TRC_NONE) || report_udev_events;
      if (debug_drain_detail) {
         Udev_Event_Detail * detail = collect_udev_event_detail(dev2);
         if (exclude_event(detail))
            DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Draining event that would be excluded anyway");
         else
            DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Draining event that would not otherwise be excluded");
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Detail for drained event");
         dbgrpt_udev_event_basic_detail(detail,1);   // apparently does only to terminal

         GPtrArray* collector = udev_event_detail_to_collector(detail, NULL);  // allocates collector
         g_ptr_array_to_syslog(LOG_DEBUG, collector, /*ornament*/ true, /*tag*/ NULL);
         g_ptr_array_free(collector, true);

         free_udev_event_detail(detail);
      }

      udev_device_unref(dev2);
      drained_ct++;
   }

   DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Drained %d additional queued udev event(s)", drained_ct);
   DBGTRC_DONE(debug, TRACE_GROUP, , "Drained %d additional queued udev event(s)", drained_ct);
   return drained_ct;
}


void init_dw_udev() {
   RTTI_ADD_FUNC(dw_udev_setup);
   RTTI_ADD_FUNC(dw_udev_teardown);
   RTTI_ADD_FUNC(dw_udev_watch);
   RTTI_ADD_FUNC(dw_udev_drain);
}
