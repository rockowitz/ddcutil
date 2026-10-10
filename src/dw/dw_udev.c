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
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/debug_util.h"
#include "util/linux_util.h"
#include "util/string_util.h"
#include "dw_suspend_resume.h"
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
#include "dw_poll.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

/** When defined, each udev event reported by #dbgrpt_udev_event_basic_detail()
 *  is followed by #dbgrpt_interpreted_udev_event_detail(), which explains what
 *  the event means for display watching rather than listing its fields again:
 *  the significance of its subsystem and action, the settling pause that an add
 *  outside /dev/dri will cause, and the connector and I2C bus named by its
 *  CONNECTOR property.  Reported only when that basic report is, i.e. when
 *  debug is set in dw_udev_watch() or report_udev_events is set by --f18.
 *
 *  Experimental, and used only in this file.
 *
 *  What it adds over the basic report.  That report prints sysname, which for
 *  a DRM event is the card -- "card1" -- and has its prop_connector line
 *  commented out, so the log otherwise says only that something on the card
 *  changed.  The interpretation resolves the connector id to a connector name
 *  and an I2C bus, using the same two lookups dw_udev_watch() uses for its own
 *  log message, and records that both answer from what the last scan found and
 *  so can be stale.
 *
 *  Measured, on one power cycle each of a DisplayPort and an HDMI display,
 *  four events, every one drm/change with HOTPLUG=1 and a CONNECTOR property:
 *
 *     Interpretation:
 *        A connector on this DRM card changed state.  This is how the kernel
 *        announces that a display was connected or disconnected.  It does not
 *        say which way, nor whether DDC will work; the watch thread answers
 *        that by rescanning every bus for an EDID and comparing with the
 *        previous scan.
 *        CONNECTOR 507 is card1-DP-1, served by /dev/i2c-6.
 *        Both lookups answer from the last scan, so either can be stale.
 *
 *  Unexercised as yet, for want of the events that reach them: a drm change
 *  without HOTPLUG, drm add and remove, the i2c-dev branches and the settling
 *  pause note that an add outside /dev/dri triggers, a CONNECTOR naming a
 *  connector with no known bus as a display behind an MST hub has, a CONNECTOR
 *  matching nothing the last scan found, and an unrecognized subsystem.  The
 *  i2c-dev and add/remove branches should appear on a driver reload or a resume
 *  from sleep.
 *
 *  Volume.  The explanatory paragraph above repeats for every event, so a burst
 *  -- a resume emits one -- fills the log with it.  If that becomes a nuisance,
 *  the paragraph belongs once per watch session, leaving the connector line as
 *  the per-event output.
 *
 *  See #READ_DPCD_ON_UDEV_EVENT below for the one part of the interpretation
 *  that is off by default.
 */
#define INTERPRET_UDEV_EVENT

#ifdef INTERPRET_UDEV_EVENT
/** When defined, the interpretation of a udev event reports DisplayPort sink
 *  and link status, read from DPCD through the connector's AUX device.  That
 *  says whether a sink is attached independently of whether its EDID still
 *  reads, which a monitor that is merely switched off may well do.
 *
 *  Experimental, and used only in this file, so it is defined here rather than
 *  in dw_common.h.
 *
 *  Off by default because it is the one part of event interpretation that
 *  performs I/O on the hardware: a short AUX transaction, read only, issued
 *  from the watch thread while the driver may still be retraining the link the
 *  event announced.  It also requires read access to /dev/drm_dp_aux*, normally
 *  mode 0600 root:root, so for most invocations it would report only that the
 *  device is not readable with the privileges the process has.
 */
// #define READ_DPCD_ON_UDEV_EVENT
#endif

#if defined(READ_DPCD_ON_UDEV_EVENT) && !defined(INTERPRET_UDEV_EVENT)
#error "READ_DPCD_ON_UDEV_EVENT requires INTERPRET_UDEV_EVENT"
#endif

// globals
bool    report_udev_events = false;   // --f18

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
int     udev_watch_stats_interval_sec = DEFAULT_UDEV_WATCH_STATS_INTERVAL_SEC;  // --i15

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
 *  Both formats take a single %d, the interval.  DUAL_MSGNV() and
 *  DECORATED_SYSLOG() both accept a runtime format.  Both call sites pass
 *  string literals -- they are not caller data, despite the shape.
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
   int paused_ms = 0;
   for (int attempt = 1; attempt <= MAX_SETTLING_PAUSE_ATTEMPTS; attempt++) {
      DUAL_MSGNV(debug, DDCA_SYSLOG_NOTICE, start_fmt, millisec);
      paused_ms += millisec;
      if (!dw_sleep_spent_by_suspend(millisec))
         break;
      if (terminate_watch_thread)
         break;
      DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, retake_fmt, millisec);
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


#ifdef READ_DPCD_ON_UDEV_EVENT
/** Reports the DisplayPort sink and link status of a DRM connector, read from
 *  DPCD through the connector's AUX device.
 *
 *  Says whether a sink is attached and whether the link is trained, which the
 *  EDID cannot: a monitor that is switched off may still answer EDID reads.
 *
 *  Requires read access to /dev/drm_dp_aux*, normally mode 0600 root:root, and
 *  applies only to DisplayPort; a connector with no AUX device is reported as
 *  such, as is a device that cannot be opened.  The read is a short AUX
 *  transaction, failing with EIO or ETIMEDOUT when the link is down rather
 *  than blocking.
 *
 *  @param  connector_name  DRM connector name, e.g. card1-DP-1
 *  @param  depth           logical indentation depth
 */
STATIC void dbgrpt_connector_dpcd_status(const char * connector_name, int depth) {
   assert(connector_name);
   char   sysdir[PATH_MAX];
   char * aux_name = NULL;
   g_snprintf(sysdir, PATH_MAX, "/sys/class/drm/%s", connector_name);
   DIR * dp = opendir(sysdir);
   if (dp) {
      struct dirent * ep;
      while ( (ep = readdir(dp)) ) {
         if (str_starts_with(ep->d_name, "drm_dp_aux")) {
            aux_name = g_strdup(ep->d_name);
            break;
         }
      }
      closedir(dp);
   }

   if (!aux_name)
      rpt_vstring(depth, "%s has no AUX device, so it is not DisplayPort.", connector_name);
   else {
      char devpath[PATH_MAX];
      g_snprintf(devpath, PATH_MAX, "/dev/%s", aux_name);
      int fd = open(devpath, O_RDONLY);
      int errsv = errno;
      if (fd < 0 && errsv == EACCES) {
         rpt_vstring(depth, "%s is not readable with the privileges this process has.",
                            devpath);
         rpt_label(depth,   "These devices are normally mode 0600 root:root.");
      }
      else if (fd < 0)
         rpt_vstring(depth, "open(%s) failed: %s", devpath, strerror(errsv));
      else {
         // DPCD 0x200 SINK_COUNT, 0x202 LANE0_1_STATUS, 0x204 LANE_ALIGN_STATUS_UPDATED,
         // 0x205 SINK_STATUS
         Byte buf[6];
         ssize_t ct = pread(fd, buf, 6, 0x200);
         int errsv = errno;
         close(fd);
         if (ct != 6)
            rpt_vstring(depth, "Reading DPCD 0x200 from %s returned %zd: %s",
                               devpath, ct, (ct < 0) ? strerror(errsv) : "short read");
         else {
            int  sink_count = buf[0] & 0x3f;
            bool cr_done    = buf[2] & 0x01;
            bool eq_done    = buf[2] & 0x02;
            bool sym_locked = buf[2] & 0x04;
            bool aligned    = buf[4] & 0x01;
            bool rx_port0   = buf[5] & 0x01;
            rpt_vstring(depth, "DPCD via %s: SINK_COUNT=%d, lane 0 CR=%s EQ=%s SYMLOCK=%s,"
                               " interlane aligned=%s, receive port 0=%s",
                               aux_name, sink_count, sbool(cr_done), sbool(eq_done),
                               sbool(sym_locked), sbool(aligned), sbool(rx_port0));
            if (sink_count == 0)
               rpt_vstring(depth, "SINK_COUNT 0: no sink is attached.  A display that still"
                                  " answers EDID reads is powered off, not connected.");
            else if (cr_done && eq_done && sym_locked && aligned)
               rpt_vstring(depth, "A sink is attached and the link is trained, so the display"
                                  " is present and driven.");
            else
               rpt_vstring(depth, "A sink is attached but the link is not fully trained, as"
                                  " during the retraining that follows a hotplug.");
         }
      }
      free(aux_name);
   }
}
#endif


#ifdef INTERPRET_UDEV_EVENT
/** Explains what a udev event means for display watching.
 *
 *  Reports the significance of the event rather than its fields, which
 *  #dbgrpt_udev_event_basic_detail() has already listed.
 *
 *  @param  detail  event detail
 *  @param  depth   logical indentation depth
 */
STATIC void dbgrpt_interpreted_udev_event_detail(Udev_Event_Detail * detail, int depth) {
   assert(detail);
   int d1 = depth + 1;
   rpt_label(depth, "Interpretation:");

   bool hotplug = streq(detail->prop_hotplug, "1");
#ifdef READ_DPCD_ON_UDEV_EVENT
   char * connector_name = NULL;   // of the connector the event names, if known
#endif

   if (streq(detail->prop_subsystem, "drm")) {
      if (streq(detail->prop_action, "change")) {
         if (hotplug) {
            rpt_label(d1, "A connector on this DRM card changed state.  This is how the kernel");
            rpt_label(d1, "announces that a display was connected or disconnected.  It does not");
            rpt_label(d1, "say which way, nor whether DDC will work; the watch thread answers");
            rpt_label(d1, "that by rescanning every bus for an EDID and comparing with the");
            rpt_label(d1, "previous scan.");
         }
         else {
            rpt_label(d1, "A DRM property changed with no HOTPLUG flag, a mode set for instance.");
            rpt_label(d1, "Not a connection change, though the watch thread rescans regardless.");
         }
      }
      else if (streq(detail->prop_action, "add")) {
         rpt_label(d1, "A DRM device node was registered, as when the video driver loads.");
      }
      else if (streq(detail->prop_action, "remove")) {
         rpt_label(d1, "A DRM device node was unregistered, as when the video driver unloads.");
      }
      else {
         rpt_vstring(d1, "Action \"%s\" is not one display watching interprets for subsystem drm.",
                         detail->prop_action);
      }
   }
   else if (streq(detail->prop_subsystem, "i2c-dev")) {
      if (streq(detail->prop_action, "add")) {
         rpt_label(d1, "An I2C device node appeared.  Buses are added when a driver loads and");
         rpt_label(d1, "again after a resume from sleep.");
      }
      else if (streq(detail->prop_action, "remove")) {
         rpt_label(d1, "An I2C device node disappeared, as when a driver unloads.  A bus that");
         rpt_label(d1, "carried a display takes the display with it.");
      }
      else {
         rpt_vstring(d1, "Action \"%s\" is not one display watching interprets for subsystem"
                         " i2c-dev.", detail->prop_action);
      }
   }
   else {
      rpt_vstring(d1, "Subsystem \"%s\" is not one display watching interprets.",
                      detail->prop_subsystem);
   }

   // The test that sets add_event_detected in dw_udev_watch(), whose only
   // consequence is the pause taken after the event burst.
   if (streq(detail->prop_action, "add") && !str_starts_with(detail->prop_devname, "/dev/dri")) {
      rpt_vstring(d1, "An add outside /dev/dri, so the watch thread will pause %d milliseconds",
                      (int) pause_after_add_ms);
      rpt_label(d1,   "after the event burst, letting the remaining device nodes appear.");
   }

   if (detail->prop_connector) {
      int connector_id = 0;
      if (str_to_int(detail->prop_connector, &connector_id, 10)) {
         char *         cname   = get_drm_connector_name_by_connector_id(connector_id);
         I2C_Bus_Info * businfo = i2c_find_businfo_by_drm_connector_id(connector_id);
#ifdef READ_DPCD_ON_UDEV_EVENT
         connector_name = (cname) ? g_strdup(cname) : NULL;
#endif
         if (cname && businfo)
            rpt_vstring(d1, "CONNECTOR %d is %s, served by /dev/i2c-%d.",
                            connector_id, cname, businfo->busno);
         else if (cname)
            rpt_vstring(d1, "CONNECTOR %d is %s.  No I2C bus is known for it, as for a"
                            " display behind an MST hub.", connector_id, cname);
         else
            rpt_vstring(d1, "CONNECTOR %d matches no connector the last scan found.",
                            connector_id);
         rpt_label(d1, "Both lookups answer from the last scan, so either can be stale.");
         free(cname);
      }
      else
         rpt_vstring(d1, "CONNECTOR property \"%s\" is not a number.", detail->prop_connector);
   }
   else if (streq(detail->prop_subsystem, "drm")) {
      rpt_label(d1, "No CONNECTOR property: the event does not say which connector changed,");
      rpt_label(d1, "so every bus has to be rescanned.");
   }

#ifdef READ_DPCD_ON_UDEV_EVENT
   // No privilege test: the question is whether the device can be opened, which
   // only the open() can answer.  A uid test would be both too strict, a local
   // udev rule or ACL being free to grant access, and too lax, since root says
   // nothing about whether the AUX read will succeed.  access() would be wrong
   // here for the reason i2c_check_device_access() documents: it tests the real
   // uid, so it misreports under setuid root, which this library supports.
   if (connector_name) {
      rpt_label(d1, "DPCD answers what the EDID cannot: whether a sink is attached,");
      rpt_label(d1, "independently of whether its EDID still reads.");
      dbgrpt_connector_dpcd_status(connector_name, d1+1);
   }

   free(connector_name);
#endif
}
#endif


/** Poll udev to watch for display connection/disconnection
 *
 *  @param  watch_loop_millisec
 *  @param  event_busno_loc  where to return the I2C bus number of the DRM
 *                           connector the event names, -1 if the event names
 *                           no connector or the connector serves no known bus
 *  @retval true   returning because watching terminated
 *  @retval false  display change detected
 */
#ifdef EVENT_BUS_NUMBER
bool dw_udev_watch(int watch_loop_millisec, int * event_busno_loc) {
#else
bool dw_udev_watch(int watch_loop_millisec) {
#endif
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_loop_millisec=%d", watch_loop_millisec);
#ifdef EVENT_BUS_NUMBER
   *event_busno_loc = -1;
#endif

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
#ifdef INTERPRET_UDEV_EVENT
                     dbgrpt_interpreted_udev_event_detail(detail,1);
#endif
                  }
                  char * connector_msg = NULL;
                  char * connector_msg2 = NULL;
                  if (detail->prop_connector) {
                     I2C_Bus_Info * businfo = NULL;
                     char * conn_name = NULL;
                     int ival = 0;
                     bool valid_int = str_to_int(detail->prop_connector, &ival, 10);
                     if (valid_int) {
                        // The name is copied out under the connector lock rather than
                        // holding the Sys_Drm_Connector.  This runs on the watch thread,
                        // which is itself what removes connectors, so the instance a
                        // search returns can be freed at any point afterwards.
                        //
                        // The name can still be stale, the array being whatever the last
                        // scan found.  That is tolerable here because it is only logged;
                        // if it is ever used for more than that, search sysfs directly.
                        conn_name = get_drm_connector_name_by_connector_id(ival);
                        if (conn_name)
                            connector_msg2 = g_strdup_printf("prop_connector = %d -> %s",
                                  ival, conn_name);
                        businfo = i2c_find_businfo_by_drm_connector_id(ival);
                        if (businfo) {
                           connector_msg = g_strdup_printf(
                               "prop_connector = %d -> /dev/i2c-%d",
                                 ival, businfo->busno);
                           // Unlike the connector name above, this is used rather than
                           // only logged: dw_process_screen_change_event() honors a
                           // disconnect on the bus the event names even when
                           // stabilization would otherwise overrule it.  The bus table
                           // is the right source precisely because it holds what the
                           // last scan found - a display now disconnecting was on this
                           // bus when it was last seen, which is what sysfs can no
                           // longer tell us once the connector has changed.
#ifdef EVENT_BUS_NUMBER
                           *event_busno_loc = businfo->busno;
#endif
                        }
                     }
                     if (!conn_name)
                        connector_msg2 = g_strdup_printf(
                              "Could not find DRM connector for connector id: %s",
                              detail->prop_connector);
                     free(conn_name);
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

      if (!skip_settling_pauses ) {
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

      // n. a remainder, not an interval: whatever settling time has already
      // been spent above counts toward it.  That has a consequence which is
      // easy to miss, since it follows from the subtraction rather than from
      // any explicit condition.
      //
      // The settling pauses default to 500 ms, well over drain_pause_ms, so
      // whenever one of them runs already_paused_ms exceeds the whole budget,
      // remaining_pause_ms is negative, and no coalescing happens at all.  And
      // they run precisely when skip_settling_pauses is false, i.e.
      // when access to /dev/i2c comes from the udev uaccess ACL rather than
      // from group i2c and the inode bits.  So on the uaccess route the
      // coalesce pause is unreachable, and --i16 has nothing to act on; on the
      // group i2c route the settling pauses are bypassed and this is the only
      // pause that runs.
      //
      // Two logs from one machine show the inversion exactly: with the user in
      // group i2c, 6 coalesce pauses and 0 settling pauses; on the uaccess
      // route, 0 coalesce pauses and 2 settling pauses.
      //
      // Note that a 500 ms settling pause gives events more time
      // to accumulate than DEFAULT_DRAIN_PAUSE_MS of coalescing would -- but the
      // coupling is implicit, and worth knowing before either value is tuned.
      // It holds only while the settling pauses remain the larger of the two.
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
