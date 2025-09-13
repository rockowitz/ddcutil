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
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/i2c_util.h"
#include "util/libdrm_aux_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
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

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"

#include "dw_common.h"
#include "dw_status_events.h"

#include "dw/dw_udev2.h"


// globals
bool    report_udev_events = false;

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


// typedef struct udev_monitor Udev_Monitor;
// static Udev_Monitor * mon;
static struct udev_monitor *mon = NULL;
static struct udev* udev = NULL;
static int fd= 0;

void dw_udev_setup() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   udev = udev_new();
   mon = udev_monitor_new_from_netlink(udev, "udev");
   // Alternative subsystem devtype values that did not detect changes:
   // drm_dp_aux_dev, kernel, i2c-dev, i2c, hidraw
    udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // detects
   // testing for hub changes
// #ifdef UDEV_I2C_DEV
    // i2c-dev report i2c device number, i2c does not, but still not useful
    udev_monitor_filter_add_match_subsystem_devtype(mon, "i2c-dev", NULL);
// #endif
   // udev_monitor_filter_add_match_subsystem_devtype(mon, "i2c", NULL);
   udev_monitor_enable_receiving(mon);
   fd = udev_monitor_get_fd(mon);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}

void dw_udev_teardown() {
   bool debug = false;
   udev_monitor_unref(mon);
   udev_unref(udev);
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "");
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
   fds.fd = fd;
   fds.events = POLLIN;

   bool found = false;
   int pollctr = 0;
   while(!found && !terminate_watch_thread) {
      int j = ++pollctr%10;
      // DBGF(true, "pollctr=%d, j=%d", pollctr, j);
      if (j == 1)
         DBGTRC_NOPREFIX((debug && (j == 1)), DDCA_TRC_NONE, "Calling poll()...(%d)", pollctr);
      int rc = poll(&fds, 1, poll_timeout_millisec);   // consider using ppol()
      if (rc == 0) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "poll() timed out");
      }
      else if (rc < 0) {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "poll() failed, errno=%d", errno);
         SYSLOG2(DDCA_SYSLOG_ERROR,  "poll() failed, errno=%d", errno);
      }
      else {
         if (fds.events&POLLIN) {
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
               DBGTRC(debug, DDCA_TRC_NONE, "Udev event detected");
               SYSLOG2(DDCA_SYSLOG_NOTICE, "Udev event detected");

               if (debug || report_udev_events) {
                  Udev_Event_Detail * detail = collect_udev_event_detail(dev);
                  dbgrpt_udev_event_detail(detail,1);
                  free_udev_event_detail(detail);
               }

               udev_device_unref(dev);
               found = true;
            }
            else {
               DBGTRC(true, DDCA_TRC_NONE, "udev_monitor_receive_device() failed");
               SYSLOG2(DDCA_SYSLOG_ERROR,  "udev_monitor_receive_device() failed");
            }
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Not for us. fds.events=0x%04x", fds.events);
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
