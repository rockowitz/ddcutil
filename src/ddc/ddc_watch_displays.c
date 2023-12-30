/** @file ddc_watch_displays.c
 *
 *  Watch for monitor addition and removal
 */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_errno.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_dpms.h"
#include "i2c/i2c_sysfs.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_watch_displays.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

static bool      terminate_watch_thread = false;
static GThread * watch_thread = NULL;
static GMutex    watch_thread_mutex;

DDC_Watch_Mode   ddc_watch_mode = Watch_Mode_Simple_Udev;
bool             ddc_slow_watch = false;

const char * ddc_watch_mode_name(DDC_Watch_Mode mode) {
   char * result = NULL;
   switch (mode) {
   case Watch_Mode_Full_Poll:   result = "Watch_Mode_Full_Poll";   break;
   case Watch_Mode_Simple_Udev: result = "Watch_Mode_Simple_Udev"; break;
   }
   return result;
}


#define WATCH_DISPLAYS_DATA_MARKER "WDDM"
typedef struct {
   char                   marker[4];
   pid_t                  main_process_id;
   pid_t                  main_thread_id;
#ifdef OLD_HOTPLUG_VERSION
   Display_Change_Handler display_change_handler;
   Bit_Set_32             drm_card_numbers;
#endif
} Watch_Displays_Data;


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
bool check_thread_or_process(pid_t id) {
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


//
//  Variant Watch_Mode_Full_Poll
//

/** Primary function to check for changes in display status (disconnect, DPMS),
 *  modify internal data structures, and emit client notifications.
 */
void ddc_recheck_bus() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   // may need to wait on startup
   while (!all_i2c_buses) {
      DBGMSF(debug, "Waiting 1 sec for all_i2c_buses");
      usleep(1000*1000);
   }

   Bit_Set_256  old_attached_buses_bitset  = buses_bitset_from_businfo_array(all_i2c_buses, false);
   Bit_Set_256  old_connected_buses_bitset = buses_bitset_from_businfo_array(all_i2c_buses, true);

   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "old_connected_buses_bitset has %d bits set", bs256_count(old_connected_buses_bitset));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "old_attached_buses_bitset: %s", bs256_to_string_decimal_t(old_attached_buses_bitset, "", ",") );
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "old_connected_buses_bitset: %s",bs256_to_string_decimal_t(old_connected_buses_bitset, "", ","));

   Bit_Set_256  cur_attached_buses_bitset = i2c_detect_attached_buses_as_bitset();
   Bit_Set_256  newly_attached_buses_bitset = bs256_and_not(cur_attached_buses_bitset, old_attached_buses_bitset);
   Bit_Set_256  newly_detached_buses_bitset = bs256_and_not(old_attached_buses_bitset, cur_attached_buses_bitset);

   bool changed = false;
   if (bs256_count(newly_detached_buses_bitset) > 0) {
      Bit_Set_256_Iterator iter = bs256_iter_new(newly_detached_buses_bitset);
      int busno;
      while(true) {
         busno = bs256_iter_next(iter);
         if (busno < 0)
            break;
         DDCA_IO_Path iopath;
         iopath.io_mode = DDCA_IO_I2C;
         iopath.path.i2c_busno = busno;
         ddc_emit_display_detection_event(DDCA_EVENT_BUS_DETACHED, NULL, iopath);
      }
      bs256_iter_free(iter);
      changed = true;
   }

   if (bs256_count(newly_attached_buses_bitset) > 0) {
      Bit_Set_256_Iterator iter = bs256_iter_new(newly_attached_buses_bitset);
      int busno;
      while(true) {
         busno = bs256_iter_next(iter);
         if (busno < 0)
            break;
         DDCA_IO_Path iopath;
         iopath.io_mode = DDCA_IO_I2C;
         iopath.path.i2c_busno = busno;

         I2C_Bus_Info * new_businfo = i2c_new_bus_info(iopath.path.i2c_busno);
         new_businfo->flags = I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME | I2C_BUS_EXISTS;
         i2c_check_bus(new_businfo);
         g_ptr_array_add(all_i2c_buses, new_businfo);
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Added businfo for bus /dev/i2c-%d", new_businfo->busno);
         if (IS_DBGTRC(debug, DDCA_TRC_NONE))
            i2c_dbgrpt_bus_info(new_businfo, 1);
         DBGTRC(true, DDCA_TRC_NONE, "Emitting DDCA_EVENT_BUS_ATTACHED for bus /dev/i2c-%d", iopath.path.i2c_busno);
         ddc_emit_display_detection_event(DDCA_EVENT_BUS_ATTACHED, NULL, iopath);
      }
      bs256_iter_free(iter);
      changed = true;
   }

   if (changed) {
      get_sys_drm_connectors(/*rescan=*/true);
   }

   GPtrArray * new_buses = i2c_detect_buses0();
   Bit_Set_256 new_connected_buses_bitset = buses_bitset_from_businfo_array(new_buses, true);
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "new_bitset has %d bits set", bs256_count(new_bitset));
   
   Bit_Set_256 newly_disconnected_buses_bitset = bs256_and_not(old_connected_buses_bitset, new_connected_buses_bitset);
   Bit_Set_256 newly_connected_buses_bitset    = bs256_and_not(new_connected_buses_bitset, old_connected_buses_bitset);
   int ct =  bs256_count(newly_disconnected_buses_bitset);
   if (ct > 0)
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "newly_disconnected_buses_bitset has %d bits set", ct);
   ct = bs256_count(newly_connected_buses_bitset);
   if (ct > 0)
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "newly_connected_buses_bitset has %d bits set", ct);

   Bit_Set_256_Iterator iter = bs256_iter_new(newly_disconnected_buses_bitset);
   int busno;
   while(true) {
      busno = bs256_iter_next(iter);
      if (busno < 0)
         break;
      I2C_Bus_Info * businfo =  i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
      ddc_remove_display_by_businfo(businfo);
      if (i2c_device_exists(businfo->busno)) {
         i2c_reset_bus_info(businfo);
      }
      else {
         g_ptr_array_remove(all_i2c_buses, businfo);
      }

      get_sys_drm_connectors(/*rescan=*/true);
   }
   bs256_iter_free(iter);
   
   iter = bs256_iter_new(newly_connected_buses_bitset);
   while(true) {
      busno = bs256_iter_next(iter);
      if (busno < 0)
         break;

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Newly detected busno %d", busno);
      int new_index = i2c_find_bus_info_index_in_gptrarray_by_busno(new_buses, busno);
      I2C_Bus_Info * new_businfo = g_ptr_array_index(new_buses, new_index);
      if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "new_businfo: /dev/i2c-%d @%p",
               new_businfo->busno, new_businfo);
         // i2c_dbgrpt_bus_info(new_businfo, 4);
      }
      I2C_Bus_Info * old_businfo = i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
      if (old_businfo) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Updating businfo for /dev/i2c-%d", old_businfo->busno);
         i2c_update_bus_info(old_businfo, new_businfo);
         ddc_add_display_by_businfo(old_businfo);  // performs emit
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "Adding businfo for newly detected /dev/i2c-%d", new_businfo->busno);
         g_ptr_array_steal_index(new_buses, new_index);
         g_ptr_array_add(all_i2c_buses, new_businfo);
         ddc_add_display_by_businfo(new_businfo);  // performs emit
      }
   }
   bs256_iter_free(iter);
   g_ptr_array_set_free_func(new_buses, (GDestroyNotify) i2c_free_bus_info);
   g_ptr_array_free(new_buses, true);

   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Checking sleep state");
   assert(all_i2c_buses);
   for (int ndx = 0; ndx < all_i2c_buses->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(all_i2c_buses, ndx);
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "ndx=%d, businfo=%p", ndx, businfo);
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bus=%d", businfo->busno);
      if (businfo->flags & I2C_BUS_ADDR_0X50) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bus=%d, I2C_BUS_ADDR_0X50 set", businfo->busno);
         bool is_dpms_asleep = dpms_check_drm_asleep(businfo);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, is_dpms_asleep=%s, last_checked_dpms_asleep=%s",
               businfo->busno, sbool(is_dpms_asleep), sbool(businfo->last_checked_dpms_asleep));
         if (is_dpms_asleep != businfo->last_checked_dpms_asleep) {
            Display_Ref * dref = ddc_get_dref_by_busno(businfo->busno);
            assert(dref);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sleep change event for dref=%p->%s", dref, dref_repr_t(dref));
            DDCA_Display_Event_Type event_type = (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE;
            ddc_emit_display_detection_event(event_type, dref, dref->io_path);
            businfo->last_checked_dpms_asleep = is_dpms_asleep;
         }
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


gpointer ddc_watch_displays_using_poll(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0);

   while (!terminate_watch_thread) {
      ddc_recheck_bus();
      int microsec = 3000*1000;
      if (ddc_slow_watch)
         microsec *= 5;
      usleep(microsec);
      // printf("."); fflush(stdout);
   }
   DBGTRC_DONE(true, TRACE_GROUP, "Terminating");
   free_watch_displays_data(wdd);
   g_thread_exit(0);
   return NULL;    // satisfy compiler check that value returned
}


//
// Variant Watch_Displays_Simple_Udev
//
// Watches for udev hotplug events and sends a simple notification to clients,
// with the expectation that they will call then ddca_redetect_displays().
//

GPtrArray* display_hotplug_callbacks = NULL;


/** Registers a display hotplug event callback
 *
 *  @param func function to register
 *
 *  The function must be of type #DDCA_Display_Hotplug_Callback_Func.
 *  It is not an error if the function is already registered.
 */
DDCA_Status ddc_register_display_hotplug_callback(DDCA_Display_Hotplug_Callback_Func func) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "func=%p", func);

   DDCA_Status ddcrc = DDCRC_INVALID_OPERATION;
#ifdef ENABLE_UDEV
   ddcrc = i2c_all_video_devices_drm() &&
            generic_register_callback(&display_hotplug_callbacks, func);
#endif

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


/** Unregisters a display hotplug callback function
 *
 *  @param  function to deregister
 *  @retval DDCRC_OK normal return
 *  @retval DDCRC_NOT_FOUND
 */
DDCA_Status ddc_unregister_display_hotplug_callback(DDCA_Display_Hotplug_Callback_Func func) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "func=%p", func);

   DDCA_Status ddcrc = DDCRC_INVALID_OPERATION;
#ifdef ENABLE_UDEV
   if (i2c_all_video_devices_drm() ) {
       ddcrc = generic_unregister_callback(display_hotplug_callbacks, func);
   }
#endif

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "");
   return ddcrc;
}


// When a display is disconnected and then reconnected, a udev event for
// the disconnection is not received until immediately before the connection
// event.  Address this situation by treating this "double tap" as a single
// hotplug event.  Unfortunately, because a disconnect udev event is not
// received in a timely manner, clients will have to discover that a display
// has been disconnected by failure of an API call.

static uint64_t last_emit_millisec = 0;
static uint64_t double_tap_millisec = 500;


void ddc_emit_display_hotplug_event() {
   bool debug = false || watch_watching;
   uint64_t cur_emit_millisec = cur_realtime_nanosec() / (1000*1000);
   DBGTRC_STARTING(debug, TRACE_GROUP, "last_emit_millisec = %jd, cur_emit_millisec %jd",
                                       last_emit_millisec, cur_emit_millisec);

   SYSLOG2(DDCA_SYSLOG_NOTICE, "DDCA_Display_Hotplug_Event");
   int callback_ct = 0;

   if ( (cur_emit_millisec - last_emit_millisec) > double_tap_millisec) {
      DBGMSF(debug, "emitting");
      if (display_hotplug_callbacks) {
         for (int ndx = 0; ndx < display_hotplug_callbacks->len; ndx++)  {
            DDCA_Display_Hotplug_Callback_Func func = g_ptr_array_index(display_hotplug_callbacks, ndx);
            DDCA_Display_Hotplug_Event event = {NULL, NULL};
            func(event);
         }
         callback_ct =  display_hotplug_callbacks->len;
      }
   }
   else {
      DBGMSF(debug, "double tap ");
   }
   last_emit_millisec = cur_emit_millisec;

   DBGTRC_DONE(debug, TRACE_GROUP, "Executed %d callbacks", callback_ct);
}


#ifdef UNUSED
void set_fd_blocking(int fd) {
   int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
   assert (flags != -1);
   flags &= ~O_NONBLOCK;
   (void) fcntl(fd, F_SETFL, flags);
   assert(rc != -1);
}
#endif


gpointer watch_displays_using_udev(gpointer data) {
   bool debug = false;
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "Caller process id: %d, caller thread id: %d",
         wdd->main_process_id, wdd->main_thread_id);

   pid_t cur_pid = getpid();
   pid_t cur_tid = get_thread_id();
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Our process id: %d, our thread id: %d", cur_pid, cur_tid);

   struct udev* udev;
   udev = udev_new();
   assert(udev);
   struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // alt "hidraw"
   udev_monitor_enable_receiving(mon);

   // make udev_monitor_receive_device() blocking
   // int fd = udev_monitor_get_fd(mon);
   // set_fd_blocking(fd);

  //  GPtrArray * prev_displays = get_sysfs_drm_connector_names();
  //  DBGTRC_NOPREFIX(debug, TRACE_GROUP,
  //         "Initial connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", ") );

   struct udev_device * dev = NULL;
   while (true) {
      dev = udev_monitor_receive_device(mon);
      while ( !dev ) {
         int sleep_secs = 4;
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Sleeping for %d seconds", sleep_secs);
         usleep(sleep_secs * 1000000);
         if (terminate_watch_thread) {
            DBGTRC_DONE(true, TRACE_GROUP, "Terminating thread");
            free_watch_displays_data(wdd);
            // g_ptr_array_free(prev_displays, true);
            g_thread_exit(0);
            assert(false);    // avoid clang warning re wdd use after free
         }

         // Doesn't work to detect client crash, main thread and process remains for some time.
         // 11/2020: is this even needed since terminate_watch_thread check added?
         // #ifdef DOESNT_WORK
         bool pid_found = check_thread_or_process(cur_pid);
         if (!pid_found) {
            DBGMSG("Process %d not found", cur_pid);
         }
         bool tid_found = check_thread_or_process(cur_tid);
         if (!pid_found || !tid_found) {
            DBGMSG("Thread %d not found", cur_tid);
            g_thread_exit(GINT_TO_POINTER(-1));
            break;
         }
         // #endif

         dev = udev_monitor_receive_device(mon);
      }

      if (debug) {
         printf("Got Device\n");
         // printf("   Node: %s\n", udev_device_get_devnode(dev));         // /dev/dri/card0
         // printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));  // drm
         // printf("   Devtype: %s\n", udev_device_get_devtype(dev));      // drm_minor

         printf("   Action:    %s\n", udev_device_get_action(   dev));     // "change"
         printf("   devpath:   %s\n", udev_device_get_devpath(  dev));
         printf("   subsystem: %s\n", udev_device_get_subsystem(dev));     // drm
         printf("   devtype:   %s\n", udev_device_get_devtype(  dev));     // drm_minor
         printf("   syspath:   %s\n", udev_device_get_syspath(  dev));
         printf("   sysname:   %s\n", udev_device_get_sysname(  dev));
         printf("   sysnum:    %s\n", udev_device_get_sysnum(   dev));
         printf("   devnode:   %s\n", udev_device_get_devnode(  dev));     // /dev/dri/card0
         printf("   initialized: %d\n", udev_device_get_is_initialized(  dev));
         printf("   driver:    %s\n", udev_device_get_driver(  dev));

         struct udev_list_entry * entries = NULL;
         entries = udev_device_get_devlinks_list_entry(dev);
         show_udev_list_entries(entries, "devlinks");

         entries = udev_device_get_properties_list_entry(dev);
         show_udev_list_entries(entries, "properties");

         entries = udev_device_get_tags_list_entry(dev);
         show_udev_list_entries(entries, "tags");

         entries = udev_device_get_sysattr_list_entry(dev);
         //show_udev_list_entries(entries, "sysattrs");
         show_sysattr_list_entries(dev,entries);
      }

      const char * hotplug = udev_device_get_property_value(dev, "HOTPLUG");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,"HOTPLUG: %s", hotplug);     // "1"

      // prev_displays = double_check_displays(prev_displays, data);
      ddc_emit_display_hotplug_event();

      udev_device_unref(dev);

      // printf("."); fflush(stdout);
   }  // while

    return NULL;
}


//
// Common to both variants
//

/** Starts thread that watches for addition or removal of displays.
 *
 *  If the thread is already running, does nothing.
 *
 *  \retval  DDCRC_OK
 */
DDCA_Status
ddc_start_watch_displays() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_mode = %s, watch_thread=%p",
                                       ddc_watch_mode_name(ddc_watch_mode),
                                       watch_thread );
   DDCA_Status ddcrc = DDCRC_OK;
   g_mutex_lock(&watch_thread_mutex);
   if (!watch_thread) {
      terminate_watch_thread = false;
      Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
      memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
#ifdef OLD_HOTPLUG_VERSION
   // data->display_change_handler = api_display_change_handler;
   // data->display_change_handler = dummy_display_change_handler;
#endif

      data->main_process_id = getpid();
      // data->main_thread_id = syscall(SYS_gettid);
      data->main_thread_id = get_thread_id();

      watch_thread = g_thread_new(
                       "watch_displays",             // optional thread name
                       (ddc_watch_mode == Watch_Mode_Full_Poll) ?
                             watch_displays_using_udev :
                             ddc_watch_displays_using_poll,
                       data);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread started");
   }
   g_mutex_unlock(&watch_thread_mutex);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


/** Halts thread that watches for addition or removal of displays.
 *  If **wait** is specified, does not return until the watch thread exits.
 *  Otherwise returns immediately.
 *
 *  \retval  DDCRC_OK
 */
DDCA_Status
ddc_stop_watch_displays(bool wait)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "wait=%s, watch_thread=%p", SBOOL(wait), watch_thread );

   DDCA_Status ddcrc = DDCRC_OK;
   g_mutex_lock(&watch_thread_mutex);

   if (watch_thread) {
      terminate_watch_thread = true;  // signal watch thread to terminate
      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
      // usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
      if (wait)
         g_thread_join(watch_thread);

      //  g_thread_unref(watch_thread);
      watch_thread = NULL;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread terminated.");
   }

   g_mutex_unlock(&watch_thread_mutex);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


void init_ddc_watch_displays() {
   RTTI_ADD_FUNC(ddc_start_watch_displays);
   RTTI_ADD_FUNC(ddc_stop_watch_displays);
   RTTI_ADD_FUNC(ddc_recheck_bus);
   RTTI_ADD_FUNC(ddc_watch_displays_using_poll);
   RTTI_ADD_FUNC(ddc_register_display_hotplug_callback);
   RTTI_ADD_FUNC(ddc_unregister_display_hotplug_callback);
   RTTI_ADD_FUNC(ddc_emit_display_hotplug_event);
#ifdef ENABLE_UDEV
   RTTI_ADD_FUNC(watch_displays_using_udev);
#endif
}
