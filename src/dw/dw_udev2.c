/** @file dw_udev2.c
 *
 *  Watch for monitor addition and removal using UDEV
 */

// Copyright (C) 2021-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "public/ddcutil_types.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"
#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/rtti.h"
#include "base/sleep.h"
/** \endcond */

#include "dw_common.h"

#include "dw/dw_udev2.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

// globals
bool    report_udev_events = false;

static struct udev* udev = NULL;
static struct udev_monitor *mon = NULL;
static int monitor_fd= 0;


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


bool exclude_event( Udev_Event_Detail * detail) {
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

   struct pollfd fds;
   fds.fd = monitor_fd;
   fds.events = POLLIN;

   bool found = false;
   int pollctr = 0;
   while(!found && !terminate_watch_thread) {
      int j = ++pollctr%10;
      // DBGF(true, "pollctr=%d, j=%d", pollctr, j);
      if (j == 1)
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling poll()...(%d)", pollctr);
      int rc = poll(&fds, 1, poll_timeout_millisec);   // consider using ppol()
      if (rc == 0) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "poll() timed out");
      }
      else if (rc < 0) {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "poll() failed, errno=%d", errno);
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,  "poll() failed, errno=%d", errno);
      }
      else {
         if (fds.revents&POLLIN) {
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
               DBGTRC(debug, DDCA_TRC_NONE, "Udev event detected");
               DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Udev event detected");

               Udev_Event_Detail * detail = collect_udev_event_detail(dev);
               if (!exclude_event(detail)) {
                  found = true;
                  if (debug || report_udev_events) {
                     dbgrpt_udev_event_basic_detail(detail,1);
                  }
                  char * connector_msg = NULL;
                  if (detail->prop_connector) {
                     I2C_Bus_Info * businfo = NULL;
                     int ival = 0;
                     bool valid_int = str_to_int(detail->prop_connector, &ival, 10);
                     if (valid_int) {
                        businfo = i2c_find_businfo_by_drm_connector_id(ival);
                        if (businfo) {
                           connector_msg = g_strdup_printf(
                               "prop_connector = %d -> /dev/i2c-%d",
                                 ival, businfo->busno);
                        }
                     }
                     if (!businfo) {
                        connector_msg = g_strdup_printf(
                              "Could not find I2C_Bus_Info for connector id: %s",
                              detail->prop_connector);
                     }
                  }
                  GPtrArray* collector = udev_event_detail_to_collector(detail, NULL);  // allocates collector
                  if (connector_msg) {
                     g_ptr_array_add(collector, strdup(connector_msg));
                     free(connector_msg);
                  }
                  g_ptr_array_to_syslog(LOG_DEBUG, collector, /*ornament*/ true, /*tag*/ NULL);
                  g_ptr_array_free(collector, true);
               }

               // TODO: refine the test
               // if (streq(detail->sysname, "i2c-dev") || streq(detail->sysnamm, "drm"))
               if (streq(detail->prop_action, "add")) {
                  int pause_millis = 1000;
                  LOGGABLE_SLEEP(pause_millis, SLEEP_OPT_TRACEABLE,DDCA_SYSLOG_NOTICE,
                        "Pausing %d millisec after UDEV add event", pause_millis);
               }
               free_udev_event_detail(detail);
               udev_device_unref(dev);
            }
            else {
               DBGTRC(true, DDCA_TRC_NONE, "udev_monitor_receive_device() failed");
               DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,  "udev_monitor_receive_device() failed");
            }
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Not for us. fds.revents=0x%04x", fds.revents);
         }
      }
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, terminate_watch_thread, "");
   return terminate_watch_thread;
}


void init_dw_udev2() {
   RTTI_ADD_FUNC(dw_udev_setup);
   RTTI_ADD_FUNC(dw_udev_teardown);
   RTTI_ADD_FUNC(dw_udev_watch);
}
