/** @file dw_udev.c
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
#ifdef ENABLE_UDEV
#include <libudev.h>
#endif
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
bool    use_sysfs_connector_id = true;
bool    report_udev_events = false;

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


// Read sysfs "status" file for a DRM connector
const char* get_connector_status(const char *syspath) {
    static char status[32];
    char path[512];
    snprintf(path, sizeof(path), "%s/status", syspath);

    FILE *f = fopen(path, "r");
    if (!f) return "unknown";

    if (fgets(status, sizeof(status), f)) {
        // Strip newline
        status[strcspn(status, "\n")] = 0;
        fclose(f);
        return status;
    }

    fclose(f);
    return "unknown";
}


void dbgrpt_udev_device3(struct udev_device * dev, bool verbose, int depth) {
   const char *action = udev_device_get_action(dev);
   const char *devnode = udev_device_get_devnode(dev);
   const char *subsystem = udev_device_get_subsystem(dev);
   const char *devtype = udev_device_get_devtype(dev);
   const char *sysname = udev_device_get_sysname(dev);
   const char *syspath = udev_device_get_syspath(dev);

   printf("Event: action=%s subsystem=%s devtype=%s sysname=%s syspath=%s, devnode=%s\n",
          action ? action : "unknown",
          subsystem ? subsystem : "none",
          devtype ? devtype : "none",
          sysname ? sysname : "none",
          syspath ? syspath : "none",
          devnode ? devnode : "none");

   const char *status  = get_connector_status(syspath);
    printf("Event: %s on %s → status=%s\n",
                       action, syspath, status);

}


// globals
// bool    use_sysfs_connector_id = true;
// bool    report_udev_events = false;


void dbgrpt_udev_device2(struct udev_device * dev, bool verbose, int depth) {
   rpt_structure_loc("udev_device", dev, depth);
   int d1 = depth+1;
   // printf("   Node: %s\n", udev_device_get_devnode(dev));         // /dev/dri/card0
   // printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));  // drm
   // printf("   Devtype: %s\n", udev_device_get_devtype(dev));      // drm_minor

   rpt_vstring(d1, "Action:      %s", udev_device_get_action(   dev));     // "change"
   rpt_vstring(d1, "devpath:     %s", udev_device_get_devpath(  dev));
   rpt_vstring(d1, "subsystem:   %s", udev_device_get_subsystem(dev));     // drm
   rpt_vstring(d1, "devtype:     %s", udev_device_get_devtype(  dev));     // drm_minor
   rpt_vstring(d1, "syspath:     %s", udev_device_get_syspath(  dev));
   rpt_vstring(d1, "sysname:     %s", udev_device_get_sysname(  dev));
   rpt_vstring(d1, "sysnum:      %s", udev_device_get_sysnum(   dev));
   rpt_vstring(d1, "devnode:     %s", udev_device_get_devnode(  dev));     // /dev/dri/card0
   rpt_vstring(d1, "initialized: %d", udev_device_get_is_initialized(  dev));
   rpt_vstring(d1, "driver:      %s", udev_device_get_driver(  dev));

   if (verbose) {
      struct udev_list_entry * entries = NULL;

#ifdef NOT_USEFUL     // see udevadm -p
      entries = udev_device_get_devlinks_list_entry(dev);
      show_udev_list_entries(entries, "devlinks");

      entries = udev_device_get_tags_list_entry(dev);
      show_udev_list_entries(entries, "tags");
#endif

      entries = udev_device_get_properties_list_entry(dev);
      show_udev_list_entries(entries, "properties");

      entries = udev_device_get_sysattr_list_entry(dev);
      //show_udev_list_entries(entries, "sysattrs");
      show_sysattr_list_entries(dev,entries);
   }
}


typedef struct {
   const char * prop_subsystem;
   const char * prop_action;
   const char * prop_connector;
   const char * prop_devname;
   const char * prop_hotplug;
   const char * sysname;
   const char * attr_name;
} Udev_Event_Detail2;


Udev_Event_Detail2* collect_udev_event_detail2(struct udev_device * dev) {
   Udev_Event_Detail2 * cd = calloc(1, sizeof(Udev_Event_Detail2));
   cd->prop_subsystem = udev_device_get_property_value(dev, "SUBSYSTEM");
   cd->prop_action    = udev_device_get_property_value(dev, "ACTION");     // always "changed"
   cd->prop_connector = udev_device_get_property_value(dev, "CONNECTOR");  // drm connector number
   cd->prop_devname   = udev_device_get_property_value(dev, "DEVNAME");    // e.g. /dev/dri/card0
   cd->prop_hotplug   = udev_device_get_property_value(dev, "HOTPLUG");    // always 1
   cd->sysname        = udev_device_get_sysname(dev);                      // e.g. card0, i2c-27
   cd-> attr_name     = udev_device_get_sysattr_value(dev, "name");
   return cd;
}


void free_udev_event_detail2(Udev_Event_Detail2 * detail) {
   free(detail);
}


void dbgrpt_udev_event_detail2(Udev_Event_Detail2 * detail, int depth) {
   assert(detail);
   rpt_structure_loc("Udev_Event_Detail", detail, depth);
   int d1 = depth + 1;
   rpt_vstring(d1, "prop_subsystem:  %s", detail->prop_subsystem);
   rpt_vstring(d1, "prop_action:     %s", detail->prop_action);
   rpt_vstring(d1, "prop_connector:  %s", detail->prop_connector);
   rpt_vstring(d1, "prop_devname:    %s", detail->prop_devname);
   rpt_vstring(d1, "prop_hotplug:    %s", detail->prop_hotplug);
   rpt_vstring(d1, "sysname:         %s", detail->sysname);
   rpt_vstring(d1, "attr_name:       %s", detail->attr_name);
}


// typedef struct udev_monitor Udev_Monitor;
// static Udev_Monitor * mon;
static struct udev_monitor *mon = NULL;
static struct udev* udev = NULL;
static int fd= 0;

void dw2_setup() {
   bool debug = true;
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

void dw2_teardown() {
   udev_monitor_unref(mon);
   udev_unref(udev);
}


void dw2_watch() {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   // fd_set fds;

   // select
   // FD_ZERO(&fds);
   // FD_SET(fd, &fds);

   struct pollfd fds;
   fds.fd = fd;
   fds.events = POLLIN;

   bool found = false;
   while(!found && !terminate_watch_thread) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Calling select()...");
      // int ret = select(fd+1, &fds, NULL, NULL, NULL);
      int poll_timeout_millis = 5000;   // *** TEMP ***, -1 = wait forever
      int ret = poll(&fds, 1, poll_timeout_millis);   // consider using ppol()
      // if (ret > 0 && FD_ISSET(fd, &fds)) {   // select
      if (ret > 0 && (fds.events&POLLIN)) {
          struct udev_device *dev = udev_monitor_receive_device(mon);

          if (dev&& debug) {
             dbgrpt_udev_device3(dev, true, 1);
             dbgrpt_udev_device2(dev, true, 1);
             Udev_Event_Detail2 * detail = collect_udev_event_detail2(dev);
             dbgrpt_udev_event_detail2(detail,1);
             free_udev_event_detail2(detail);

              // Here you can add logic to detect connection/disconnection involving
              // displays by examining the sysname or devnode,
              // for example, sysname like "card0-DP-1" or similar DRM outputs.

              // Then you can query /sys/class/drm/card0-HDMI-A-1/status (connected/disconnected) to know the actual state.

              udev_device_unref(dev);
              found = true;
          }
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP,"");
   return;
}


void init_dw2_udev() {
   RTTI_ADD_FUNC(dw2_setup);
   RTTI_ADD_FUNC(dw2_teardown);
   RTTI_ADD_FUNC(dw2_watch);
}
