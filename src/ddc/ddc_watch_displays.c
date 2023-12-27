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


// Experimental code
// static bool watch_displays_enabled = false;
bool ddc_watching_using_udev = false;  // if false watching using poll

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

static bool terminate_watch_thread = false;
static GThread * watch_thread = NULL;
static GMutex    watch_thread_mutex;

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


bool slow_watch = false;


#ifdef OLD_HOTPLUG_VERSION
const char * displays_change_type_name(Displays_Change_Type change_type) {
   char * result = NULL;
   switch(change_type)
   {
   case Changed_None:    result = "Changed_None";    break;
   case Changed_Added:   result = "Changed_Added";   break;
   case Changed_Removed: result = "Changed_Removed"; break;
   case Changed_Both:    result = "Changed_Both";    break;
   }
   return result;
}
#endif



/** Checks that a thread or process id is valid.
 *
 *  @param id  thread or process id
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
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "old_attached_buses_bitset: %s",  bs256_to_string_decimal_t(old_attached_buses_bitset, "", ",") );
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
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "new_businfo: /dev/i2c-%d @%p", new_businfo->busno, new_businfo);
         // i2c_dbgrpt_bus_info(new_businfo, 4);
      }
      I2C_Bus_Info * old_businfo = i2c_find_bus_info_in_gptrarray_by_busno(all_i2c_buses, busno);
      if (old_businfo) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Updating businfo for /dev/i2c-%d", old_businfo->busno);
         i2c_update_bus_info(old_businfo, new_businfo);
         ddc_add_display_by_businfo(old_businfo);  // performs emit
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding businfo for newly detected /dev/i2c-%d", new_businfo->busno);
         g_ptr_array_steal_index(new_buses, new_index);
         g_ptr_array_add(all_i2c_buses, new_businfo);
         ddc_add_display_by_businfo(new_businfo);  // performs emit
      }
   }
   bs256_iter_free(iter);
   g_ptr_array_set_free_func(new_buses, (GDestroyNotify) i2c_free_bus_info);
   g_ptr_array_free(new_buses, true);

   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Checking sleep state");
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


/** Tests if communication working for a Display_Ref
 *
 *  @param  dref   display reference
 *  @return true/false
 */
bool is_dref_alive(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s", dref_repr_t(dref));
   Display_Handle* dh = NULL;
   Error_Info * erec = NULL;
   bool is_alive = true;
   if (dref->io_path.io_mode == DDCA_IO_I2C) {   // punt on DDCA_IO_USB for now
      bool check_needed = true;
      if (dref->drm_connector) {
         char * status = NULL;
         int depth = ( IS_DBGTRC(debug, TRACE_GROUP) ) ? 2 : -1;
         RPT_ATTR_TEXT(depth,&status, "/sys/class/drm", dref->drm_connector, "status");
         if (!streq(status, "connected"))
            check_needed = false;
         free(status);
      }
      if (check_needed) {
         erec = ddc_open_display(dref, CALLOPT_WAIT, &dh);
         assert(!erec);
         Parsed_Nontable_Vcp_Response * parsed_nontable_response = NULL;  // vs interpreted ..
         erec = ddc_get_nontable_vcp_value(dh, 0x10, &parsed_nontable_response);
         // seen: -ETIMEDOUT, DDCRC_NULL_RESPONSE then -ETIMEDOUT, -EIO, DDCRC_DATA
         // if (erec && errinfo_all_causes_same_status(erec, 0))
         if (erec)
            is_alive = false;
         ERRINFO_FREE_WITH_REPORT(erec, IS_DBGTRC(debug, TRACE_GROUP));
         ddc_close_display_wo_return(dh);
      }
   }
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, is_alive, "");
   return is_alive;
}


/** Check all display references to determine if they are active.
 *  Sets or clears the DREF_ALIVE flag in the display reference.
 */
void check_drefs_alive() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (ddc_displays_already_detected()) {
      GPtrArray * all_displays = ddc_get_all_displays();
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         bool alive = is_dref_alive(dref);
         if (alive != (dref->flags & DREF_ALIVE) )
            (void) dref_set_alive(dref, alive);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "dref=%s, is_alive=%s",
                         dref_repr_t(dref), SBOOL(dref->flags & DREF_ALIVE));
      }
   }
   else {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "displays not yet detected");
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


#ifdef OLD_HOTPLUG_VERSION
/** Obtains a list of currently connected displays and compares it to the
 *  previously detected list
 *
 *  Reports each change to the display_change_handler() in the Watch_Displays_Data struct
 *
 *  @param prev_displays   GPtrArray of previously detected displays
 *  @param data  pointer to a Watch_Displays_Data struct
 *  @return GPtrArray of currently detected monitors
 */
static GPtrArray * check_displays(GPtrArray * prev_displays, gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "prev_displays=%s",
                 join_string_g_ptr_array_t(prev_displays, ", "));

   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );

   // typedef enum _change_type {Changed_None = 0, Changed_Added = 1, Changed_Removed = 2, Changed_Both = 3 } Change_Type;
   Displays_Change_Type change_type = Changed_None;

   GPtrArray * cur_displays = get_sysfs_drm_connector_names();
   if ( !gaux_unique_string_ptr_arrays_equal(prev_displays, cur_displays) ) {
      if ( IS_DBGTRC( debug, TRACE_GROUP) ) {
         DBGMSG("Active DRM connectors changed!");
         DBGMSG("Previous active connectors: %s", join_string_g_ptr_array_t(prev_displays, ", "));
         DBGMSG("Current  active connectors: %s", join_string_g_ptr_array_t(cur_displays,  ", "));
      }

      GPtrArray * removed = gaux_unique_string_ptr_arrays_minus(prev_displays, cur_displays);
      if (removed->len > 0) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                "Removed DRM connectors: %s", join_string_g_ptr_array_t(removed, ", ") );
         change_type = Changed_Removed;
      }

      GPtrArray * added = gaux_unique_string_ptr_arrays_minus(cur_displays, prev_displays);
      if (added->len > 0) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                "Added DRM connectors: %s", join_string_g_ptr_array_t(added, ", ") );
         change_type = (change_type == Changed_None) ? Changed_Added : Changed_Both;
      }

   //    if (change_type != Changed_None) {
      // assert( change_type != Changed_Both);
      // DBGMSG("wdd->display_change_handler = %p (%s)",
      //         wdd->display_change_handler,
      //         rtti_get_func_name_by_addr(wdd->display_change_handler) );
      if (wdd->display_change_handler) {
         wdd->display_change_handler( change_type, removed, added);
      }
      // }
      g_ptr_array_free(removed,       true);
      g_ptr_array_free(added,         true);
   }

   // g_ptr_array_free(prev_displays, true);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s",
                              join_string_g_ptr_array_t(cur_displays, ", "));
   return cur_displays;
}


//static
GPtrArray* double_check_displays(GPtrArray* prev_displays, gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "prev_displays = %s",
                 join_string_g_ptr_array_t(prev_displays, ", "));

   GPtrArray * result = NULL;
   GPtrArray * cur_displays = check_displays(prev_displays,  data);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "cur_displays  = %s",
         join_string_g_ptr_array_t(cur_displays, ", "));
   if (gaux_unique_string_ptr_arrays_equal(prev_displays, cur_displays) ) {
      result = cur_displays;
   }
   else {
      DBGMSG("Double checking");
      usleep(1000*1000);  // 1 second
      result = check_displays(prev_displays, data);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "after recheck:  %s", join_string_g_ptr_array_t(result, ", "));
      g_ptr_array_free(cur_displays, true);
   }
   g_ptr_array_free(prev_displays, true);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning:      %s",
                              join_string_g_ptr_array_t(result, ", "));
   return result;
}
#endif



// How to detect main thread crash?

#ifdef OLD_HOTPLUG_VERSION
gpointer ddc_watch_displays_using_poll(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0);

   GPtrArray * prev_displays = get_sysfs_drm_displays();  // GPtrArray of DRM connector names
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
          "Initial active DRM connectors: %s", join_string_g_ptr_array_t(prev_displays, ", ") );

   while (!terminate_watch_thread) {
      prev_displays = double_check_displays(prev_displays, data);
      check_drefs_alive();
      usleep(3000*1000);
      // printf("."); fflush(stdout);
   }
   DBGTRC_DONE(true, TRACE_GROUP, "Terminating");
   free_watch_displays_data(wdd);
   g_thread_exit(0);
   return NULL;    // satisfy compiler check that value returned
}
#endif



gpointer ddc_watch_displays_using_poll(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0);

   while (!terminate_watch_thread) {
      ddc_recheck_bus();
      int microsec = 3000*1000;
      if (slow_watch)
         microsec *= 5;
      usleep(microsec);
      // printf("."); fflush(stdout);
   }
   DBGTRC_DONE(true, TRACE_GROUP, "Terminating");
   free_watch_displays_data(wdd);
   g_thread_exit(0);
   return NULL;    // satisfy compiler check that value returned
}


#ifdef UNUSED
void set_fd_blocking(int fd) {
   int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
   assert (flags != -1);
   flags &= ~O_NONBLOCK;
#ifndef NDEBUG
   int rc =
#endif
   fcntl(fd, F_SETFL, flags);
   assert(rc != -1);
}
#endif

// #ifdef ENABLE_UDEV
gpointer watch_displays_using_udev(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Caller process id: %d, caller thread id: %d", wdd->main_process_id, wdd->main_thread_id);
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
      assert(dev);
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
#ifdef NO
      else {
         // Failure indicates main thread has died.  Kill this one too.
         int errsv=errno;
         DBGTRC_DONE(debug, TRACE_GROUP,
                            "No Device from udev_monitor_receive_device()."
                            " An error occurred. errno=%d=%s. Terminating thread.",
                            errsv, linux_errno_name(errsv));
         g_thread_exit(GINT_TO_POINTER(-1));
         // break;
      }
#endif

      // printf(".");
      // fflush(stdout);
   }  // while

    return NULL;
}
// #endif

#ifdef OLD_HOTPLUG_VERSION
void dummy_display_change_handler(
        Displays_Change_Type changes,
        GPtrArray *          removed,
        GPtrArray *          added)
{
   bool debug = true;
   // DBGTRC_STARTING(debug, TRACE_GROUP, "changes = %s", displays_change_type_name(changes));
   if (removed && removed->len > 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Removed displays: %s", join_string_g_ptr_array_t(removed, ", ") );
   }
   if (added && added->len > 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Added   displays: %s", join_string_g_ptr_array_t(added, ", ") );
   }
   // DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void api_display_change_handler(
        Displays_Change_Type changes,
        GPtrArray *          removed,
        GPtrArray *          added)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "changes = %s", displays_change_type_name(changes));

#ifdef  DETAILED_DISPLAY_CHANGE_HANDLING
   if (removed && removed->len > 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Removed displays: %s", join_string_g_ptr_array_t(removed, ", ") );
      if (removed) {
         for (int ndx = 0; ndx < removed->len; ndx++) {
             bool ok = ddc_remove_display_by_drm_connector(g_ptr_array_index(removed, ndx));
             if (!ok)
                DBGMSG("Display with drm connector %s not found", g_ptr_array_index(removed,ndx));
         }
      }
   }
   if (added && added->len > 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Added   displays: %s", join_string_g_ptr_array_t(added, ", ") );
      if (added) {
         for (int ndx = 0; ndx < added->len; ndx++) {
             bool ok = ddc_add_display_by_drm_connector(g_ptr_array_index(added, ndx));
             if (!ok)
                DBGMSG("Display with drm connector %s already exists", g_ptr_array_index(added,ndx));
         }
      }
   }
#endif

   // simpler
   // ddc_emit_display_hotplug_event();


   DBGTRC_DONE(debug, TRACE_GROUP, "");
}
#endif


/** Starts thread that watches for addition or removal of displays.
 *
 *  If the thread is already running, does nothing.
 *
 *  \retval  DDCRC_OK
 */
DDCA_Status
ddc_start_watch_displays(bool use_udev_if_possible)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "use_udev_if_possible=%s, watch_thread=%p",
                                       SBOOL(use_udev_if_possible), watch_thread );
   DDCA_Status ddcrc = DDCRC_OK;
   ddc_watching_using_udev = use_udev_if_possible;

   g_mutex_lock(&watch_thread_mutex);
   if (!watch_thread) {
      terminate_watch_thread = false;
      Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
      memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
   // data->display_change_handler = api_display_change_handler;
   // data->display_change_handler = dummy_display_change_handler;

      data->main_process_id = getpid();
      // data->main_thread_id = syscall(SYS_gettid);
      data->main_thread_id = get_thread_id();

      watch_thread = g_thread_new(
                       "watch_displays",             // optional thread name
                       (use_udev_if_possible) ?
                             watch_displays_using_udev :
                             ddc_watch_displays_using_poll,
                       data);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread started");
   }
   g_mutex_unlock(&watch_thread_mutex);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


// only makes sense if polling!
// locks udev_monitor_receive_device() blocks

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



#ifdef OLD_HOTPLUG_VERSION
/** Starts thread that watches for addition or removal of displays
 *
 *  \retval  DDCRC_OK
 *  \retval  DDCRC_INVALID_OPERATION  thread already running
 */
DDCA_Status
ddc_start_watch_displays(bool use_udev_if_possible)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_displays_enabled=%s, use_udev_if_possible=%s",
                                       SBOOL(watch_displays_enabled), SBOOL(use_udev_if_possible) );
   DDCA_Status ddcrc = DDCRC_OK;

   watch_displays_enabled = true;   // n. changing the meaning of watch_displays_enabled

   if (watch_displays_enabled) {
      char * class_drm_dir =
   #ifdef TARGET_BSD
            "/compat/sys/class/drm";
   #else
            "/sys/class/drm";
   #endif

      Bit_Set_32 drm_card_numbers = get_sysfs_drm_card_numbers();
      if (bs32_count(drm_card_numbers) == 0) {
         MSG_W_SYSLOG(DDCA_SYSLOG_ERROR,
               "No DRM enabled video cards found in %s. Disabling detection of display hotplug events.",
               class_drm_dir);
         ddcrc = DDCRC_INVALID_OPERATION;
      }
      else {
         if (!all_video_devices_drm()) {
            MSG_W_SYSLOG(DDCA_SYSLOG_WARNING,
               "Not all video cards support DRM.  Hotplug events are not not detected for connected monitors.");
         }
         g_mutex_lock(&watch_thread_mutex);

         if (watch_thread)
            ddcrc = DDCRC_INVALID_OPERATION;
         else {
            terminate_watch_thread = false;
            Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
            memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
         // data->display_change_handler = api_display_change_handler;
            data->display_change_handler = dummy_display_change_handler;
            data->main_process_id = getpid();
            // data->main_thread_id = syscall(SYS_gettid);
            data->main_thread_id = get_thread_id();
            data->drm_card_numbers = drm_card_numbers;
            void * watch_func = ddc_watch_displays_using_poll;
            ddc_watching_using_udev = false;
#ifdef ENABLE_UDEV
            if (use_udev_if_possible) {
               watch_func = watch_displays_using_udev;
               ddc_watching_using_udev = true;
            }
#endif
            watch_thread = g_thread_new(
                             "watch_displays",             // optional thread name
                             watch_func,
#ifdef OLD
      #if ENABLE_UDEV
                             (use_udev_if_possible) ? watch_displays_using_udev : ddc_watch_displays_using_poll,
      #else
                             ddc_watch_displays_using_poll,
      #endif
#endif
                             data);
            SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread started");
         }
         g_mutex_unlock(&watch_thread_mutex);
      }
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_displays_enabled=%s. watch_thread=%p",
                                        SBOOL(watch_displays_enabled), watch_thread);
   return ddcrc;
}


// only makes sense if polling!
// locks udev_monitor_receive_device() blocks

/** Halts thread that watches for addition or removal of displays.
 *
 *  Does not return until the watch thread exits.
 *
 *  \retval  DDCRC_OK
 *  \retval  DDCRC_INVALID_OPERATION  no watch thread running
 */
DDCA_Status
ddc_stop_watch_displays()
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_displays_enabled=%s", SBOOL(watch_displays_enabled) );
   DDCA_Status ddcrc = DDCRC_OK;

   if (watch_displays_enabled) {
      g_mutex_lock(&watch_thread_mutex);

      // does not work if watch_displays_using_udev(), loop doesn't wake up unless there's a udev event
      if (watch_thread) {
         if (ddc_watching_using_udev) {

#ifdef ENABLE_UDEV
            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting for watch thread to terminate...");
            terminate_watch_thread = true;  // signal watch thread to terminate
            // if using udev, thread never terminates because udev_monitor_receive_device() is blocking,
            // so termiate flag doesn't get checked
            // no big deal, ddc_stop_watch_displays() is only called at program termination to
            // release resources for tidyness
#else
            PROGRAM_LOGIC_ERROR("watching_using_udev set when ENABLE_UDEV not set");
#endif
         }
         else {     // watching using poll
            terminate_watch_thread = true;  // signal watch thread to terminate
            DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
            usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
            g_thread_join(watch_thread);
            //  g_thread_unref(watch_thread);
         }
         watch_thread = NULL;
         SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread terminated.");
      }
      else
         ddcrc = DDCRC_INVALID_OPERATION;

      g_mutex_unlock(&watch_thread_mutex);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}
#endif


void init_ddc_watch_displays() {
   RTTI_ADD_FUNC(check_drefs_alive);
   RTTI_ADD_FUNC(ddc_start_watch_displays);
   RTTI_ADD_FUNC(ddc_stop_watch_displays);
   RTTI_ADD_FUNC(i2c_detect_buses);
   RTTI_ADD_FUNC(i2c_detect_buses0);
   RTTI_ADD_FUNC(is_dref_alive);
   RTTI_ADD_FUNC(ddc_recheck_bus);
   RTTI_ADD_FUNC(ddc_watch_displays_using_poll);
#ifdef OLD_HOTPLUG_VERSION
   RTTI_ADD_FUNC(check_displays);
   RTTI_ADD_FUNC(dummy_display_change_handler);
   RTTI_ADD_FUNC(api_display_change_handler);
#ifdef ENABLE_UDEV
   RTTI_ADD_FUNC(watch_displays_using_udev);
#endif
#endif
}
