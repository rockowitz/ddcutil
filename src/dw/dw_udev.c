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
#include "util/suspend_resume_util.h"
#include "util/timestamp.h"
#include "util/udev_util.h"

#include "base/core.h"
#ifdef PROFILE_UDEV_WATCH_THREAD
#include "base/per_thread_data.h"
#endif
#include "base/rtti.h"

#include "sysfs/sysfs_sys_drm_connector.h"

#include "i2c/i2c_bus_collections.h"
#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_services.h"
/** \endcond */

#include "dw_common.h"

#include "dw_udev.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

// globals
bool    report_udev_events = false;

/** Minimum seconds between execution statistics reports from #dw_udev_watch(),
 *  0 to report nothing.  Set by --i15.
 *
 *  A floor, not a period.  The check sits at the top of the poll loop, and
 *  with use_eventfd set -- the default, see dw_common.c -- poll() is given a
 *  timeout of -1 and blocks until a udev event arrives or termination is
 *  signaled.  The loop therefore reaches the top once at entry and then only
 *  when a display actually changes, so reports come at whatever rate udev
 *  events do, never oftener than this interval.  On an idle system with no
 *  display activity, the report at entry is the only one.
 *
 *  Accepted rather than fixed: capping the poll timeout at the interval would
 *  make it a true period, but would reintroduce the periodic wakeup that the
 *  eventfd path (--f32) exists to eliminate, and idle power residency matters
 *  more here than report cadence.
 */
int     udev_watch_stats_interval_sec = DEFAULT_UDEV_WATCH_STATS_INTERVAL_SEC;

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


/** Pauses, retaking the pause if a suspend spends it rather than serving it.
 *
 *  A sleep is measured on CLOCK_MONOTONIC, which does not advance while the
 *  system is suspended.  A pause that a suspend begins during is therefore not
 *  merely delayed: it is served in full on the far side and buys nothing,
 *  leaving whatever it was waiting for no more settled than before.
 *  #dw_sleep_spent_by_suspend() times on CLOCK_BOOTTIME and so can tell the
 *  two apart.
 *
 *  Bounded by MAX_SETTLING_PAUSE_ATTEMPTS, and abandoned on shutdown.
 *
 *  The two messages are passed in whole rather than assembled from a name.
 *  That is a clumsier signature than it might be, and deliberate: the wording
 *  of these lines predates the helper and is what log analysis of this
 *  subsystem greps for, so unifying the code must not quietly reword them.
 *  Both formats take a single %d, the interval.  Both call sites pass string
 *  literals -- they are not caller data, despite the shape.
 *
 *  @param  debug       caller's debug flag
 *  @param  millisec    length of each attempt
 *  @param  start_fmt   logged before each attempt, takes the interval
 *  @param  retake_fmt  logged when an attempt was spent by a suspend
 *  @return total milliseconds spent pausing, counting retaken attempts
 */
STATIC int dw_pause_retaking_if_suspended(
      bool debug, int millisec, const char * start_fmt, const char * retake_fmt)
{
   // The messages are formatted here and passed as "%s" rather than handed to
   // the macros as formats.  DBGTRC_NOPREFIX(), which DUAL_MSGNV() expands to,
   // concatenates its prefix onto the format -- "          "format -- so the
   // format must be a string literal.  A runtime format does not compile.
   char msgbuf[200];
   int paused_ms = 0;
   for (int attempt = 1; attempt <= MAX_SETTLING_PAUSE_ATTEMPTS; attempt++) {
      g_snprintf(msgbuf, sizeof(msgbuf), start_fmt, millisec);
      DUAL_MSGNV(debug, DDCA_SYSLOG_NOTICE, "%s", msgbuf);
      paused_ms += millisec;
      if (!dw_sleep_spent_by_suspend(millisec))
         break;
      if (terminate_watch_thread)
         break;
      g_snprintf(msgbuf, sizeof(msgbuf), retake_fmt, millisec);
      DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s", msgbuf);
   }
   return paused_ms;
}


/** CLOCK_BOOTTIME nanoseconds at which #dw_udev_watch() last reported
 *  execution statistics, 0 if it has not yet reported.
 *
 *  File scope rather than local to the function: dw_udev_watch() returns each
 *  time an event is detected and is immediately re-entered by the watch loop
 *  in dw_poll.c, so a local would treat every display change as a first pass
 *  and report then, rather than on the requested interval.
 */
static uint64_t last_stats_report_ns = 0;


/** Poll udev to watch for display connection/disconnection
 *
 *  @param  watch_loop_millisec
 *  @retval true   returning because watching terminated
 *  @retval false  display change detected
 */
bool dw_udev_watch(int watch_loop_millisec) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_loop_millisec=%d", watch_loop_millisec);

#ifdef PROFILE_UDEV_WATCH_THREAD
   // Profiling this function both registers the watch thread in
   // per_thread_data_hash -- ptd_profile_function_start() creates the record --
   // and gives it one set of function stats.  Without it the thread has no
   // Per_Thread_Data at all, since the only other creator is the API entry
   // macro and the watch thread crosses no API boundary, so the report emitted
   // below lists the client's calling thread and omits the one thread it is
   // about.  Gated as the API prologs are, so profiling stays a single switch.
   //
   // Backed out because what it records is not worth reading: the elapsed time
   // is almost entirely time blocked in poll() awaiting a udev event, so the
   // figure says how long the watch waited, not what it cost, and the call
   // count is just the number of events seen -- which the event log already
   // shows.  Retained in case the thread needs to appear in the per-thread
   // section for some other reason.  See also the end of this function.
   if (ptd_api_profiling_enabled)
      ptd_profile_function_start(__func__);
#endif

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
      // Off unless --i15 sets an interval: a full statistics dump per udev
      // event burst is diagnostic output, not something every libddcutil
      // client should find in its journal.  When on, report on the first pass,
      // then no oftener than udev_watch_stats_interval_sec.  Reaching this
      // point again requires a udev event, so the interval is a floor rather
      // than a period; see the comment on that global.  Timed on
      // CLOCK_BOOTTIME so a suspend counts toward the interval: the elapsed
      // wall time is what makes the report readable in the log.
      uint64_t cur_ns = cur_boot_time_nanosec();
      if (udev_watch_stats_interval_sec > 0 &&
          (last_stats_report_ns == 0 ||
           cur_ns - last_stats_report_ns >= SECS2NANOS(udev_watch_stats_interval_sec)))
      {
         last_stats_report_ns = cur_ns;
         ddc_report_stats_main(DDCA_STATS_ALL,
                               /*report_per_display=*/  false,
                               /*include_dsa_stats=*/   false,
                               /*stats_to_syslog_only=*/true,
                               /*depth=*/               0);
      }

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
         DUAL_MSGXV(debug, DDCA_SYSLOG_ERROR,  DDCA_TRC_NONE, "poll() failed, errno=%d", errno);
      }
      else {
         if (fds[0].revents&POLLIN) {
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
               // DBGTRC(debug, DDCA_TRC_NONE, "Udev event detected");
               // DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Udev event detected");
               DUAL_MSGX(debug, DDCA_SYSLOG_NOTICE, DDCA_TRC_NONE, "udev event detected");

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
               DUAL_MSGX(debug, DDCA_SYSLOG_ERROR, DDCA_TRC_NONE,  "udev_monitor_receive_device() failed");
            }
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Not for us. fds[0].revents=0x%04x", fds[0].revents);
         }
      }
   }     // while()

   if (!terminate_watch_thread) {
      int already_paused_ms = 0;

      if (!skip_resume_from_pauses_sleeps ) {
         if (add_event_detected) {
            // Run the resume detection before pausing, so that its clocktime
            // reference point precedes this sleep and
            // dw_pause_if_recently_resumed_from_sleep() counts this sleep toward
            // pause_after_resume_ms instead of pausing again in full.  Whether
            // that credit is enough to skip the guard below now depends on how
            // pause_after_add_ms compares with pause_after_resume_ms: they are
            // separately tunable, and equal only by default.  When the add pause
            // is the shorter, the guard runs and tops up the settling time,
            // measured from the resume itself, which is what it should do.
            // No-op if a resume did not recently occur.
            recently_resumed_from_sleep(pause_after_resume_ms, NULL, NULL);

            // An add event can arrive in the interval between
            // PrepareForSleep(true) and the freeze, so this pause is one a
            // suspend can spend.
            already_paused_ms += dw_pause_retaking_if_suspended(debug, pause_after_add_ms,
                  "Pausing %d millisec after UDEV add event",
                  "Pause of %d millisec after UDEV add event was spent by a suspend. "
                  "Pausing again.");
         }

         if (already_paused_ms < pause_after_resume_ms) {
            // n. returns the milliseconds actually slept, usually 0 if no recent resume;
            // do not credit the full interval, o.w. the coalesce pause below never runs
            already_paused_ms += dw_pause_if_recently_resumed_from_sleep(pause_after_resume_ms);
         }
      }

      int remaining_pause_ms = drain_pause_ms - already_paused_ms;
      if (remaining_pause_ms > 0) {
         // This pause lets events accumulate so dw_udev_drain() takes them as
         // a batch.  A suspend that spends it leaves the drain running the
         // instant the process thaws, before the burst of device
         // re-registration events a resume produces has arrived: it drains the
         // stale pre-suspend set, and every event of the burst is then handled
         // individually -- the spike the coalescing exists to prevent, at the
         // moment it is largest.
         dw_pause_retaking_if_suspended(debug, remaining_pause_ms,
               "Allowing time for events to coalesce: Sleeping for %d milliseconds",
               "Pause of %d millisec allowing events to coalesce was spent by a suspend. "
               "Pausing again.");
      }

      dw_udev_drain();
   }

#ifdef PROFILE_UDEV_WATCH_THREAD
   // See the matching block at the top of this function.
   if (ptd_api_profiling_enabled)
      ptd_profile_function_end(__func__);
#endif

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
   if (monitor_fd < 0) {
      DUAL_MSGX(debug, TRACE_GROUP, DDCA_SYSLOG_ERROR, "monitor_fd mpt set");
      goto bye;
   }

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

bye:
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
