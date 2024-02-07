/** @file ddc_watch_displays.c
 *
 *  Watch for monitor addition and removal
 */

// Copyright (C) 2021-2024 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "ddc/ddc_status_events.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_watch_displays.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

static bool      terminate_watch_thread = false;
static GThread * watch_thread = NULL;
static DDCA_Display_Event_Class active_classes = DDCA_EVENT_CLASS_NONE;
static GMutex    watch_thread_mutex;

DDC_Watch_Mode   ddc_watch_mode = Watch_Mode_Simple_Udev;
bool             ddc_slow_watch = false;
int              extra_stabilize_seconds = DEFAULT_EXTRA_STABILIZE_SECS;

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
   char                     marker[4];
   pid_t                    main_process_id;
   pid_t                    main_thread_id;
   DDCA_Display_Event_Class event_classes;
// #ifdef OLD_HOTPLUG_VERSION
   Display_Change_Handler display_change_handler;
   Bit_Set_32             drm_card_numbers;
// #endif
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

// #ifdef WATCH_MODE_FULL_POLL

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
#ifdef FUTURE
         DDCA_IO_Path iopath;
         iopath.io_mode = DDCA_IO_I2C;
         iopath.path.i2c_busno = busno;
         // connector events not currently being reported
         // ddc_emit_display_detection_event(DDCA_EVENT_CONNECTOR_REMOVED, NULL, NULL, iopath);
#endif
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
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Added businfo for bus /dev/i2c-%d", new_businfo->busno);
         if (IS_DBGTRC(debug, DDCA_TRC_NONE))
            i2c_dbgrpt_bus_info(new_businfo, 1);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Emitting DDCA_EVENT_BUS_ATTACHED for bus /dev/i2c-%d", iopath.path.i2c_busno);
         // connector events not currently being reported
         // ddc_emit_display_detection_event(DDCA_EVENT_CONNECTOR_ADDED, new_businfo->drm_connector_name, NULL, iopath);
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
      Display_Ref * dref = ddc_remove_display_by_businfo(businfo);
      ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
                                      businfo->drm_connector_name,
                                      dref, dref->io_path,
                                      NULL);
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
         Display_Ref * dref = ddc_add_display_by_businfo(old_businfo);
         ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_CONNECTED,
                             old_businfo->drm_connector_name, dref, dref->io_path, NULL);
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "Adding businfo for newly detected /dev/i2c-%d", new_businfo->busno);
         // g_ptr_array_steal_index() requires glib 2.58
         // g_ptr_array_steal_index(new_buses, new_index);
         // should not be set, but just in case:
         g_ptr_array_set_free_func(new_buses, NULL);
         g_ptr_array_remove_index(new_buses, new_index);
         g_ptr_array_add(all_i2c_buses, new_businfo);
         Display_Ref * dref = ddc_add_display_by_businfo(new_businfo);
         ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_CONNECTED,
                             new_businfo->drm_connector_name, dref, dref->io_path, NULL);

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
         bool is_dpms_asleep = dpms_check_drm_asleep_by_businfo(businfo);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, is_dpms_asleep=%s, last_checked_dpms_asleep=%s",
               businfo->busno, sbool(is_dpms_asleep), sbool(businfo->last_checked_dpms_asleep));
         if (is_dpms_asleep != businfo->last_checked_dpms_asleep) {
            Display_Ref * dref = ddc_get_dref_by_busno_or_connector(businfo->busno, NULL, /*ignore_invalid*/ true);
            assert(dref);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sleep change event for dref=%p->%s", dref, dref_repr_t(dref));
            DDCA_Display_Event_Type event_type = (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE;
            ddc_emit_display_status_event(event_type, dref->drm_connector, dref, dref->io_path, NULL);
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
// #else
// gpointer ddc_watch_displays_using_poll(gpointer data) {
//    return NULL;
// }
// #endif


#ifdef WHERE_DOES_THIS_GO
// When a display is disconnected and then reconnected, a udev event for
// the disconnection is not received until immediately before the connection
// event.  Address this situation by treating this "double tap" as a single
// hotplug event.  Unfortunately, because a disconnect udev event is not
// received in a timely manner, clients will have to discover that a display
// has been disconnected by failure of an API call.

// No longer a problem since udev_monitor_receive_device() no longer blocks

static uint64_t last_emit_millisec = 0;
static uint64_t double_tap_millisec = 5000;

void ddc_emit_display_hotplug_event() {
   bool debug = false;
   uint64_t cur_emit_millisec = cur_realtime_nanosec() / (1000*1000);
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "last_emit_millisec = %jd, cur_emit_millisec = %jd, double_tap_millisec = %jd",
         last_emit_millisec, cur_emit_millisec, double_tap_millisec);

   SYSLOG2(DDCA_SYSLOG_NOTICE, "DDCA_Display_Hotplug_Event");
   int callback_ct = 0;

   uint64_t delta_millisec = cur_emit_millisec - last_emit_millisec;
   if ( delta_millisec > double_tap_millisec) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "delta_millisec = %jd, invoking registered callback functions", delta_millisec);
      SYSLOG2(DDCA_SYSLOG_NOTICE,
            "delta_millisec = %jd, invoking registered callback functions", delta_millisec);
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
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "delta_millisec = %jd, double tap", delta_millisec);
      SYSLOG2(DDCA_SYSLOG_NOTICE,
         "delta_millisec = %jd, double tap", delta_millisec);
   }
   last_emit_millisec = cur_emit_millisec;

   DBGTRC_DONE(debug, TRACE_GROUP, "Executed %d callbacks", callback_ct);
}
#endif


// Connector add or removal are not currently reported in the API
#define DDCA_EVENT_CONNECTOR_ADDED   DDCA_EVENT_UNUSED1
#define DDCA_EVENT_CONNECTOR_REMOVED DDCA_EVENT_UNUSED2



bool ddc_hotplug_change_handler(
        GPtrArray *          connectors_removed,
        GPtrArray *          connectors_added,
        GPtrArray *          connectors_having_edid_removed,
        GPtrArray *          connectors_having_edid_added,
        GArray *             events_queue)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   bool event_emitted = false;

   if (connectors_removed && connectors_removed->len > 0) {
      char * s =  join_string_g_ptr_array_t(connectors_removed, ", ");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_removed: %s", s );
      SYSLOG2(DDCA_SYSLOG_NOTICE, "(ddc_hotplug_change_handler) DRM connectors removed: %s", s);
      for (int ndx = 0; ndx < connectors_removed->len; ndx++) {
         char * connector_name = g_ptr_array_index(connectors_removed, ndx);
         Sys_Drm_Connector * conn = find_sys_drm_connector(-1, NULL, connector_name);
         if (!conn) {
            // connector has already been removed
            char buf[80];
            g_snprintf(buf, 80, "Sys_Drm_Connector not found for connector %s", connector_name);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", s);
            SYSLOG2(DDCA_SYSLOG_ERROR, "(%s) %s", __func__, s);

            // connector was already removed, nothing to do
            // DDCA_IO_Path path;
            // path.io_mode = DDCA_IO_I2C;
            // path.path.i2c_busno = BUSNO_NOT_SET;
            // ddc_emit_display_detection_event(DDCA_EVENT_CONNECTOR_REMOVED, connector_name, NULL, path);
         }
         else {
#ifdef FUTURE
            DDCA_IO_Path path;
            path.io_mode = DDCA_IO_I2C;
            path.path.i2c_busno = conn->i2c_busno;
            // Do not emit, not useful to client
            // ddc_emit_display_detection_event(DDCA_EVENT_CONNECTOR_REMOVED, connector_name, NULL, path);
#endif
         }
      }
   }

   if (connectors_added && connectors_added->len > 0) {
      char * s =  join_string_g_ptr_array_t(connectors_added, ", ");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "DRM connectors added: %s", s);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "(%s) DRM connectors added: %s", __func__, s);
      for (int ndx = 0; ndx < connectors_added->len; ndx++) {
         char * connector_name = g_ptr_array_index(connectors_added, ndx);
         Sys_Drm_Connector * conn = find_sys_drm_connector(-1, NULL, connector_name);
         if (!conn) {
            char buf[100];
            g_snprintf(buf, 100, "Sys_Drm_Connector not found for connector %s", connector_name);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", s);
            SYSLOG2(DDCA_SYSLOG_ERROR, "(%s) %s", __func__, s);
#ifdef FUTURE
            DDCA_IO_Path path;
            path.io_mode = DDCA_IO_I2C;
            path.path.i2c_busno = BUSNO_NOT_SET;
            // Do not emit, not useful to client
            // ddc_emit_display_detection_event(DDCA_EVENT_CONNECTOR_ADDED, connector_name, NULL, path);
#endif
         }
         else {
#ifdef FUTURE
            DDCA_IO_Path path;
            path.io_mode = DDCA_IO_I2C;
            path.path.i2c_busno = conn->i2c_busno;
            // Do not emit, not useful to client
            // ddc_emit_display_detection_event(DDCA_EVENT_CONNECTOR_ADDED, connector_name, NULL, path);
#endif
         }
      }
   }


   if (connectors_having_edid_removed && connectors_having_edid_removed->len > 0) {
      char * s =  join_string_g_ptr_array_t(connectors_having_edid_removed, ", ");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_having_edid_removed: %s", s);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "DRM connectors with newly disconnected displays: %s", s);
      for (int ndx = 0; ndx < connectors_having_edid_removed->len; ndx++) {
         char * connector_name = g_ptr_array_index(connectors_having_edid_removed, ndx);
         Display_Ref * dref = DDC_GET_DREF_BY_CONNECTOR(connector_name, /*ignore_invalid*/ true);
         if (dref) {
            assert(!(dref->flags & DREF_REMOVED));
            dref->flags |= DREF_REMOVED;
            // dref->detail = NULL;
            char buf[100];
            g_snprintf(buf, 100, "Removing connected display, drm_connector: %s, dref %s", connector_name, dref_repr_t(dref));
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
            SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf); // *** TEMP ***
#ifdef MAYBE
            DDCA_Display_Status_Event evt = ddc_create_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED,
                                            connector_name,
                                            dref,
                                            dref->io_path);
#endif
            ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED, connector_name, dref, dref->io_path, events_queue);
            event_emitted = true;
         }
         else {
            // dref has already been logically removed
            // As there's no dref, report the io path for the display
            Sys_Drm_Connector * conn = find_sys_drm_connector(-1, NULL, connector_name);
            if (!conn) {
                char buf[100];
                g_snprintf(buf, 100, "INTERNAL ERROR: Sys_Drm_Connector not found for connector %s", connector_name);
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
                SYSLOG2(DDCA_SYSLOG_ERROR, "%s", buf);
             }
             else {
                DDCA_IO_Path path;
                path.io_mode = DDCA_IO_I2C;
                path.path.i2c_busno = conn->i2c_busno;
                char buf[100];
                g_snprintf(buf, 100, "Removing connected display with bus %s, drm connector %s", dpath_repr_t(&path), connector_name);
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
                SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf); // *** TEMP ***
                ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_DISCONNECTED, connector_name, NULL, path, events_queue);
                event_emitted = true;
             }
         }
      }
   }


   if (connectors_having_edid_added && connectors_having_edid_added->len > 0) {
      char * s = join_string_g_ptr_array_t(connectors_having_edid_added, ", ");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_having_edid_added: %s", s);
      // SYSLOG2(DDCA_SYSLOG_NOTICE, "DRM connectors having newly connected displays: %s", s);
      for (int ndx = 0; ndx < connectors_having_edid_added->len; ndx++) {
         char * connector_name = g_ptr_array_index(connectors_having_edid_added, ndx);
         Display_Ref * dref = DDC_GET_DREF_BY_CONNECTOR(
                                  connector_name, /* ignore_invalid */ true);
         if (!dref) {
            // Expected since display changes detected do not presently modify
            // the list of display refs.

            // As there's no dref, report the io path for the display
            Sys_Drm_Connector * conn = find_sys_drm_connector(-1, NULL, connector_name);
            if (!conn) {
               char buf[100];
               g_snprintf(buf, 100, "INTERNAL ERROR: Sys_Drm_Connector not found for connector %s", connector_name);
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
               SYSLOG2(DDCA_SYSLOG_ERROR, "%s", buf);
            }
            else {
               // The expected path: dref not found, Sys_Drm_Connector rec found
               DDCA_IO_Path path;
               path.io_mode = DDCA_IO_I2C;
               path.path.i2c_busno = conn->i2c_busno;
               char buf[100];
               g_snprintf(buf, 100, "Adding connected display with bus %s, drm connector %s", dpath_repr_t(&path), connector_name);
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
               // SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buf);   // *** TEMP ***
               ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_CONNECTED, connector_name, NULL, path, events_queue);
               event_emitted = true;
            }
         }
         else {
            char buf[200];
            g_snprintf(buf, 200, "INTERNAL ERROR: dref=%s exists for newly added display on drm connector %s",
                  dref_repr_t(dref), connector_name);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,"%s", buf);
            SYSLOG2(DDCA_SYSLOG_ERROR, "(%s) %s", __func__, buf);
            ddc_emit_display_status_event(DDCA_EVENT_DISPLAY_CONNECTED, connector_name, dref, dref->io_path, events_queue);
            event_emitted = true;
         }
      }
   }


   DBGTRC_RET_BOOL(debug, TRACE_GROUP,event_emitted, "");
   return event_emitted;
}


/** Repeatedly calls #get_sysfs_drm_connector_names() until the value read
 *  equals the prior value.
 *
 *  @oaram prior  initial connector value
 *  @param some_displays_disconnected
 *  @return stabilized value
 */
Sysfs_Connector_Names
stabilized_connector_names(Sysfs_Connector_Names prior,
                           bool                  some_displays_disconnected) {
   bool debug = false;
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_STARTING(true, DDCA_TRC_NONE,"prior:");
      dbgrpt_sysfs_connector_names(prior, 2);
   }

   // Special handling for case of apparently disconnected displays.
   // It has been observed that in some cases (Samsung U32H750) a disconnect is followed a
   // few seconds later by a connect. Wait a few seconds to avoid triggering events
   // in this case.
   if (some_displays_disconnected) {
      if (extra_stabilize_seconds > 0) {
         char * s = g_strdup_printf(
               "Delaying %d seconds to avoid a false disconnect/connect sequence...", extra_stabilize_seconds);
         DBGTRC(debug, TRACE_GROUP, "%s", s);
         SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);
         free(s);
         usleep(extra_stabilize_seconds * 1000000);
      }
   }

   int stablect = 0;
   bool stable = false;
   while (!stable) {
      usleep(1000*1000);  // 1 second
      Sysfs_Connector_Names latest = get_sysfs_drm_connector_names();
      if (sysfs_connector_names_equal(prior, latest))
            stable = true;
      free_sysfs_connector_names_contents(prior);
      prior = latest;
      stablect++;
   }
   if (stablect > 1) {
      DBGTRC(debug, TRACE_GROUP,   "Required %d extra calls to get_sysfs_drm_connector_names()", stablect+1);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s required %d extra calls to get_sysfs_drm_connector_names()", __func__, stablect-1);
   }

   DBGTRC_RET_STRUCT_VALUE(debug, DDCA_TRC_NONE, Sysfs_Connector_Names, dbgrpt_sysfs_connector_names, prior);
   return prior;
}


/** Obtains a list of currently connected displays and compares it to the
 *  previously detected list
 *
 *  If any changes were detected, calls the hotplug_change_handler
 *
 *  @param prev_displays   GPtrArray of previously detected displays
 *  @return GPtrArray of currently detected monitors
 */
//static
Sysfs_Connector_Names ddc_check_displays(
      Sysfs_Connector_Names prev_connector_names,
      GArray * events_queue)
{
   bool debug = false;
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGTRC_STARTING(true, DDCA_TRC_NONE, "prev_connector_names:");
      dbgrpt_sysfs_connector_names(prev_connector_names, 2);
   }


   Sysfs_Connector_Names new_connector_names = get_sysfs_drm_connector_names();

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      DBGMSG("new_connector_names:");
      dbgrpt_sysfs_connector_names(new_connector_names, 1);
   }

   if (!sysfs_connector_names_equal(prev_connector_names, new_connector_names)) {
      // Detect need for special handling for case of display disconnected.
      GPtrArray * connectors_having_edid_removed =
            gaux_unique_string_ptr_arrays_minus(prev_connector_names.connectors_having_edid,
                                                 new_connector_names.connectors_having_edid);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_having_edid_removed: %s",
               join_string_g_ptr_array_t(connectors_having_edid_removed, ", ") );
      bool detected_displays_removed = connectors_having_edid_removed->len > 0;
      g_ptr_array_free(connectors_having_edid_removed, true);

      Sysfs_Connector_Names stabilized_names = stabilized_connector_names(new_connector_names, detected_displays_removed);
      if (IS_DBGTRC(debug, DDCA_TRC_NONE))  {
         DBGTRC(true, DDCA_TRC_NONE, "stabilized_names:");
         dbgrpt_sysfs_connector_names(stabilized_names, 2);
      }
      // free_connector_names_arrays(new_connector_names); // done in stabilized_arrays
      new_connector_names = stabilized_names;
   }

   bool hotplug_change_handler_emitted = false;
   bool connector_names_changed = !sysfs_connector_names_equal(prev_connector_names, new_connector_names);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "connector_names_changed = %s", SBOOL(connector_names_changed));

   if (connector_names_changed) {
         GPtrArray * connectors_removed =
               gaux_unique_string_ptr_arrays_minus(prev_connector_names.all_connectors,
                                                    new_connector_names.all_connectors);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_removed: %s",
                   join_string_g_ptr_array_t(connectors_removed, ", ") );

         GPtrArray * connectors_added =
               gaux_unique_string_ptr_arrays_minus( new_connector_names.all_connectors,
                                                   prev_connector_names.all_connectors);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors added: %s",
                  join_string_g_ptr_array_t(connectors_added, ", ") );

         GPtrArray * connectors_having_edid_removed =
               gaux_unique_string_ptr_arrays_minus(prev_connector_names.connectors_having_edid,
                                                    new_connector_names.connectors_having_edid);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_having_edid_removed: %s",
                  join_string_g_ptr_array_t(connectors_having_edid_removed, ", ") );

         GPtrArray * connectors_having_edid_added =
               gaux_unique_string_ptr_arrays_minus( new_connector_names.connectors_having_edid,
                                                   prev_connector_names.connectors_having_edid);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "connectors_having_edid_added: %s",
                  join_string_g_ptr_array_t(connectors_having_edid_added, ", ") );

        hotplug_change_handler_emitted =ddc_hotplug_change_handler(connectors_removed,
                                    connectors_added,
                                    connectors_having_edid_removed,
                                    connectors_having_edid_added,
                                    events_queue);
         // if (wdd->display_change_handler)
         //    wdd->display_change_handler( change_type, removed, added);

         g_ptr_array_free(connectors_removed,             true);
         g_ptr_array_free(connectors_added,               true);
         g_ptr_array_free(connectors_having_edid_removed, true);
         g_ptr_array_free(connectors_having_edid_added,   true);
   }

   free_sysfs_connector_names_contents(prev_connector_names);

   if (connector_names_changed || hotplug_change_handler_emitted)
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "connector_names_changed == %s, hotplug_change_handler_emitted = %s",
            sbool(connector_names_changed), sbool (hotplug_change_handler_emitted));

   // *connectors_changed_loc = connector_names_changed;

   DBGTRC_RET_STRUCT_VALUE(debug, TRACE_GROUP, Sysfs_Drm_Connector_Names,
                                  dbgrpt_sysfs_connector_names, new_connector_names);
   return new_connector_names;
}


void  ddc_check_asleep(GPtrArray * active_connectors,
      GPtrArray * sleepy_connectors,
      GArray* display_status_events) // array of DDCA_Display_Status_Event
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "active_connectors =%s",
         join_string_g_ptr_array_t(active_connectors, ", "));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sleepy_connectors= %s",
         join_string_g_ptr_array_t(sleepy_connectors, ", "));
// #ifdef TEMP
   // remove from the sleepy_connectors array any connector that is not currently active
   // so that it will not be marked asleep when it becomes active
   // i.e. turn off is asleep if connector no longer has a monitor
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Initial sleepy_connectors->len=%d", sleepy_connectors->len);
   int sleepy_ndx = 0;
   while (sleepy_ndx < sleepy_connectors->len) {
   // for (int sleepy_ndx = 0; sleepy_ndx < sleepy_connectors->len; sleepy_ndx++) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Top of loop 1. sleepy_ndx=%d, sleepy_connectors->len=%d", sleepy_ndx, sleepy_connectors->len);
      char * sleepy_connector = g_ptr_array_index(sleepy_connectors, sleepy_ndx);
      guint found_loc = 0;
      bool currently_connected = g_ptr_array_find_with_equal_func(
                                 active_connectors, sleepy_connector, gaux_streq, &found_loc);
      if (!currently_connected) {
         g_ptr_array_remove_index(sleepy_connectors, sleepy_ndx);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "after removed %s, sleepy_connectors->len=%d",
               sleepy_connector, sleepy_connectors->len);
      }
      else
         sleepy_ndx++;
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "bottom loop 1: sleepy_connectors after removing inactive connectors, len=%d, : %s", sleepy_connectors->len,
          join_string_g_ptr_array_t(sleepy_connectors, ", "));


   for (int active_conn_ndx = 0; active_conn_ndx < active_connectors->len; active_conn_ndx++) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "top of loop 2. active_conn_index=%d, active_connectors->len=%d",
            active_conn_ndx, active_connectors->len);

      char * connector = g_ptr_array_index(active_connectors, active_conn_ndx);
      bool is_dpms_asleep = dpms_check_drm_asleep_by_connector(connector);
      guint found_sleepy_loc = 0;
      bool last_checked_dpms_asleep = g_ptr_array_find_with_equal_func(
                           sleepy_connectors, connector, g_str_equal, &found_sleepy_loc);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "connector =%s, last_checked_dpms_asleep=%s, is_dpms_asleep=%s",
            connector, sbool (last_checked_dpms_asleep), sbool(is_dpms_asleep));

      if (is_dpms_asleep != last_checked_dpms_asleep) {
         Display_Ref * dref = DDC_GET_DREF_BY_CONNECTOR(connector, /* ignore_invalid */ true);
#ifdef REDUNDANT
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "connector = %s, sleep event: %s",
               connector,
               (is_dpms_asleep) ? "Asleep" : "Awake");
#endif
         DDCA_Display_Status_Event evt;
         if (!dref) {
            SYSLOG2(DDCA_SYSLOG_WARNING, "Sleep event. connector=%s, dref not set", connector);
            int busno = sys_drm_get_busno_by_connector(connector);
            DDCA_IO_Path io_path = i2c_io_path(busno);
            evt = ddc_create_display_status_event(
                        (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE,
                        connector,
                        NULL,
                        io_path);
         }
         else {
            evt = ddc_create_display_status_event(
                        (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE,
                        connector,
                        dref,
                        dref->io_path);
         }
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Queueing %s", display_status_event_repr_t(evt));
         g_array_append_val(display_status_events,evt);
#ifdef OLD
            ddc_emit_display_status_event(
                  (is_dpms_asleep) ? DDCA_EVENT_DPMS_ASLEEP : DDCA_EVENT_DPMS_AWAKE,
                  connector,
                  dref,
                  dref->io_path);
#endif

         if (is_dpms_asleep) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding %s to sleepy_connectors", connector);
            g_ptr_array_add(sleepy_connectors, g_strdup(connector));
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Removing %s from sleepy_connectors", connector);
            g_ptr_array_remove_index(sleepy_connectors, found_sleepy_loc);
         }
      }

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "bottom of loop 2, active_connectors->len = %d, sleepy_connectors->len=%d",
            active_connectors->len, sleepy_connectors->len);
   }
// #endif
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "active_connectors len %d. contents %s",
         active_connectors->len,
         join_string_g_ptr_array_t(active_connectors, ", "));
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sleepy_connectors->len=%d, contents: %s",
         sleepy_connectors->len,
         join_string_g_ptr_array_t(sleepy_connectors, ", "));
}


#ifdef ENABLE_UDEV
#ifdef UNUSED
void set_fd_blocking(int fd) {
   int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
   assert (flags != -1);
   flags &= ~O_NONBLOCK;
   (void) fcntl(fd, F_SETFL, flags);
   assert(rc != -1);
}
#endif


void dbgrpt_udev_device(struct udev_device * dev, int depth) {
   rpt_structure_loc("udev_device", dev, depth);
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


void filter_sleep_events(GArray * events) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "Initial events queue length: %d", events->len);
   assert(events);
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
             if (!streq(evt2.connector_name, evt.connector_name) ){
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Different connector");
                // for a different connector, ignore
                continue;
             }
             if (evt.event_type == DDCA_EVENT_DPMS_ASLEEP) {
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
            SYSLOG2(DDCA_SYSLOG_NOTICE, "Filtered out sleep event: %s", display_status_event_repr_t(evt_asleep));
            SYSLOG2(DDCA_SYSLOG_NOTICE, "Filtered out sleep event: %s", display_status_event_repr_t(evt_awake));
            g_array_remove_index(events, awake_ndx);
            g_array_remove_index(events, asleep_ndx);
            initial_ndx = asleep_ndx - 1;
         }
      }
      initial_ndx++;
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Final events queue length: %d", events->len);
}


gpointer ddc_watch_displays_using_udev(gpointer data) {
   bool debug = false;
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "Caller process id: %d, caller thread id: %d",
         wdd->main_process_id, wdd->main_thread_id);

   pid_t cur_pid = getpid();
   pid_t cur_tid = get_thread_id();
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Our process id: %d, our thread id: %d", cur_pid, cur_tid);

   GPtrArray * sleepy_connectors = g_ptr_array_new_with_free_func(g_free);

   struct udev* udev;
   udev = udev_new();
   assert(udev);
   struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
   // udev_monitor_filter_add_match_subsystem_devtype(mon, "drm_dp_aux_dev", NULL);   // alt "hidraw"
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // detects
   // udev_monitor_filter_add_match_subsystem_devtype(mon, "kernel", NULL);   // alt "hidraw" // does not detect
   // udev_monitor_filter_add_match_subsystem_devtype(mon,  "i2c-dev", NULL);  // does not detect
   //udev_monitor_filter_add_match_subsystem_devtype(mon,  "i2c", NULL); // does not detect
   udev_monitor_enable_receiving(mon);

   // make udev_monitor_receive_device() blocking
   // int fd = udev_monitor_get_fd(mon);
   // set_fd_blocking(fd);

   Sysfs_Connector_Names current_connector_names = get_sysfs_drm_connector_names();
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
        "Initial existing connectors: %s", join_string_g_ptr_array_t(current_connector_names.all_connectors, ", ") );
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
        "Initial connectors having edid: %s", join_string_g_ptr_array_t(current_connector_names.connectors_having_edid, ", ") );

   GArray * deferred_events = g_array_new( false,      // zero_terminated
                                          false,      // clear
                                          sizeof(DDCA_Display_Status_Event));
   struct udev_device * dev = NULL;
   while (true) {
      if (wdd->event_classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION) {
         dev = udev_monitor_receive_device(mon);
      }
      while ( !dev ) {
         int sleep_secs = 2;   // default sleep time on each loop
         if (ddc_slow_watch)
            sleep_secs *= 3;
         if (debug)
            DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Sleeping for %d seconds", sleep_secs);
         const int max_sleep_microsec = sleep_secs * 1000000;
         const int sleep_step_microsec = MIN(200000, max_sleep_microsec);     // .2 sec
         int slept = 0;
         for (; slept < max_sleep_microsec && !terminate_watch_thread; slept += sleep_step_microsec)
            usleep(sleep_step_microsec);

         if (deferred_events->len > 0) {
            if (deferred_events->len > 1) {  // FUTURE ENHANCMENT, filter out meaningless events
               // check for cancellation events
               for (int ndx = 0; ndx < deferred_events->len; ndx++) {
                  DDCA_Display_Status_Event evt = g_array_index(deferred_events, DDCA_Display_Status_Event, ndx);
                  DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Event %d in queue: %s", ndx, display_status_event_repr_t(evt));
               }
               filter_sleep_events(deferred_events);
            }
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Emitting %d deferred events", deferred_events->len);
            for (int ndx = 0; ndx < deferred_events->len; ndx++) {
               DDCA_Display_Status_Event evt = g_array_index(deferred_events, DDCA_Display_Status_Event, ndx);
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Emitting deferred event %s", display_status_event_repr_t(evt));
               ddc_emit_display_status_record(evt);
            }
            g_array_remove_range(deferred_events,0, deferred_events->len);
         }

         if (terminate_watch_thread) {
            DBGTRC_DONE(debug, TRACE_GROUP, "Terminating thread.  Final polling sleep was %d millisec.", slept/1000);
            free_watch_displays_data(wdd);
            free_sysfs_connector_names_contents(current_connector_names);
            g_thread_exit(0);
            assert(false);    // avoid clang warning re wdd use after free
         }

         if (wdd->event_classes & DDCA_EVENT_CLASS_DPMS)
            ddc_check_asleep(current_connector_names.connectors_having_edid, sleepy_connectors, deferred_events);

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

         if (wdd->event_classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION)
            dev = udev_monitor_receive_device(mon);
      }

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "udev event received");

      if (debug) {
         printf("Got Device\n");
         dbgrpt_udev_device(dev, 2);
      }

      const char * prop_action    = udev_device_get_property_value(dev, "ACTION");
      const char * prop_connector = udev_device_get_property_value(dev, "CONNECTOR");
      const char * prop_devname   = udev_device_get_property_value(dev, "DEVNAME");
      const char * prop_hotplug   = udev_device_get_property_value(dev, "HOTPLUG");
      const char * attr_sysname   = udev_device_get_sysname(dev);

      DBGTRC_NOPREFIX(debug, TRACE_GROUP,"ACTION: %s, CONNECTOR: %s, DEVNAME: %s, HOTPLUG: %s, sysname: %s",
            prop_action,
            prop_connector,
            prop_devname,
            prop_hotplug,     // "1"
            attr_sysname);

      current_connector_names = ddc_check_displays(current_connector_names, deferred_events);

      udev_device_unref(dev);

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "udev event processed");

      // printf("."); fflush(stdout);
   }  // while

   udev_monitor_unref(mon);
   g_ptr_array_free(sleepy_connectors, true);
   return NULL;
}
#endif

//
// Common to both variants
//

/** Starts thread that watches for changes in display connection status.
 *
 *  \return  Error_Info struct if error:
 *           -  DDCRC_INVALID_OPERATION  watch thread already started
 *           -  DDCRC_ARG                event_classes == DDCA_EVENT_CLASS_NONE
 */
Error_Info *
ddc_start_watch_displays(DDCA_Display_Event_Class event_classes) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "watch_mode = %s, watch_thread=%p, event_clases=0x%02x, drm_enabled=%s",
                                       ddc_watch_mode_name(ddc_watch_mode),
                                       watch_thread, event_classes, SBOOL(drm_enabled) );
   // DDCA_Status ddcrc = DDCRC_OK;
   Error_Info * err = NULL;

   if (!drm_enabled) {
      // ddcrc = DDCRC_INVALID_OPERATION;
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Requires DRM video drivers");
      goto bye;
   }

   g_mutex_lock(&watch_thread_mutex);
   if (!(event_classes & (DDCA_EVENT_CLASS_DPMS|DDCA_EVENT_CLASS_DISPLAY_CONNECTION))) {
      // ddcrc = DDCRC_ARG;
      err = ERRINFO_NEW(DDCRC_ARG, "Invalid event classes");
   }
   else if (watch_thread) {
      // ddcrc = DDCRC_INVALID_OPERATION;
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watch thread already running");
   }
   else {
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
      data->event_classes = event_classes;

      watch_thread = g_thread_new(
                       "watch_displays",             // optional thread name
#ifdef ENABLE_UDEV
                       (ddc_watch_mode == Watch_Mode_Full_Poll) ?
                             ddc_watch_displays_using_poll :
                             ddc_watch_displays_using_udev,
#else
                       ddc_watch_displays_using_poll,
#endif
                       data);
      active_classes = event_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread started");
   }
   g_mutex_unlock(&watch_thread_mutex);

bye:
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "watch_thread=%p", watch_thread);
   // DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return err;
}


/** Halts thread that watches for changes in display connection status.
 *  If **wait** is specified, does not return until the watch thread exits.
 *  Otherwise returns immediately.
 *
 *  \retval  DDCRC_OK
 */
DDCA_Status
ddc_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "wait=%s, watch_thread=%p", SBOOL(wait), watch_thread );

   DDCA_Status ddcrc = DDCRC_OK;
   *enabled_classes_loc = DDCA_EVENT_CLASS_NONE;
   g_mutex_lock(&watch_thread_mutex);

   if (watch_thread) {
      terminate_watch_thread = true;  // signal watch thread to terminate
      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
      // usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
      if (wait)
         g_thread_join(watch_thread);

      //  g_thread_unref(watch_thread);
      watch_thread = NULL;
      *enabled_classes_loc = active_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread terminated.");
   }
   else {
      ddcrc = DDCRC_INVALID_OPERATION;
   }

   g_mutex_unlock(&watch_thread_mutex);

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


/** If the watch thread is currently executing returns, reports the
 *  currently active display event classes as a bit flag.
 *
 *  @param  classes_loc  where to return bit flag
 *  @retval DDCRC_OK
 *  @retval DDCRC_INVALID_OPERATION watch thread not executing
 */
DDCA_Status
ddc_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc) {
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "classes_loc = %p", classes_loc);
   DDCA_Status ddcrc = DDCRC_INVALID_OPERATION;
   *classes_loc = DDCA_EVENT_CLASS_NONE;
   g_mutex_lock(&watch_thread_mutex);
   if (watch_thread) {
      *classes_loc = active_classes;
      ddcrc = DDCRC_OK;
   }
   g_mutex_unlock(&watch_thread_mutex);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "*classes_loc=0x%02x", *classes_loc);
   return ddcrc;
}


void init_ddc_watch_displays() {
   RTTI_ADD_FUNC(ddc_start_watch_displays);
   RTTI_ADD_FUNC(ddc_stop_watch_displays);
   RTTI_ADD_FUNC(ddc_get_active_watch_classes);
// #ifdef WATCH_USING_POLL
   RTTI_ADD_FUNC(ddc_recheck_bus);
//  #endif
   RTTI_ADD_FUNC(ddc_watch_displays_using_poll);
#ifdef ENABLE_UDEV
   RTTI_ADD_FUNC(ddc_check_asleep);
   RTTI_ADD_FUNC(stabilized_connector_names);
   RTTI_ADD_FUNC(ddc_check_displays);
   RTTI_ADD_FUNC(ddc_watch_displays_using_udev);
   RTTI_ADD_FUNC(ddc_hotplug_change_handler);
   RTTI_ADD_FUNC(filter_sleep_events);
#endif
}
