// watch_displays_archived.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later




#ifdef  DETAILED_DISPLAY_CHANGE_HANDLING
//
// Modify local data structures before invoking client callback functions.
// Too many edge cases
//

/** Process a display removal event.
 *
 *  The currently active Display_Ref for the specified DRM connector
 *  name is located.  It is marked removed, and the associated
 *  I2C_Bus_Info struct is reset.
 *
 *  @param drm_connector    connector name, e.g. card0-DP-1
 *  @remark
 *  Does not handle displays using USB for communication
 */
bool ddc_remove_display_by_drm_connector(const char * drm_connector) {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "drm_connector = %s", drm_connector);

   // DBGTRC_NOPREFIX(true, TRACE_GROUP, "All existing Bus_Info recs:");
   // i2c_dbgrpt_buses(/* report_all */ true, 2);

   bool found = false;
   assert(all_display_refs);
   for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
      // If a display is repeatedly removed and added on a particular connector,
      // there will be multiple Display_Ref records.  All but one should already
      // be flagged DDCA_DISPLAY_REMOVED, and should not have a pointer to
      // an I2C_Bus_Info struct.
      Display_Ref * dref = g_ptr_array_index(all_display_refs, ndx);
      assert(dref);
      DBGMSG("Checking dref %s", dref_repr_t(dref));
      dbgrpt_display_ref(dref, 2);
      if (dref->io_path.io_mode == DDCA_IO_I2C) {
         if (dref->flags & DDCA_DISPLAY_REMOVED)  {
            DBGMSG("DDCA_DISPLAY_REMOVED set");
            continue;
         }
         I2C_Bus_Info * businfo = dref->detail;
         // DBGMSG("businfo = %p", businfo);
         assert(businfo);
         DBGMSG("Checking I2C_Bus_Info for %d", businfo->busno);
         if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED))
            i2c_check_businfo_connector(businfo);
         DBGMSG("drm_connector_found_by = %s (%d)",
               drm_connector_found_by_name(businfo->drm_connector_found_by),
               businfo->drm_connector_found_by);
         if (businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_FOUND) {
            DBGMSG("comparing %s", businfo->drm_connector_name);
            if (streq(businfo->drm_connector_name, drm_connector)) {
               DBGMSG("Found drm_connector %s", drm_connector);
               dref->flags |= DREF_REMOVED;
               i2c_reset_bus_info(businfo);
               DDCA_Display_Detection_Report report;
               report.operation = DDCA_DISPLAY_REMOVED;
               report.dref = dref;
               ddc_emit_display_detection_event(report);
               found = true;
               break;
            }
         }
      }
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, found, "");
   return found;
}


bool ddc_add_display_by_drm_connector(const char * drm_connector_name) {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "drm_connector_name = %s", drm_connector_name);

   bool ok = false;
   Sys_Drm_Connector * conrec = find_sys_drm_connector(-1, NULL, drm_connector_name);
   if (conrec) {
      int busno = conrec->i2c_busno;
      // TODO: ensure that there's no I2c_Bus_Info record for the bus
      I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
      if (!businfo)
         businfo = i2c_new_bus_info(busno);
      if (businfo->flags&I2C_BUS_PROBED) {
         SEVEREMSG("Display added for I2C bus %d still marked in use", busno);
         i2c_reset_bus_info(businfo);
      }

      i2c_check_bus(businfo);
      if (businfo->flags & I2C_BUS_ADDR_0X50) {
         Display_Ref * old_dref = ddc_get_display_ref_by_drm_connector(drm_connector_name, /*ignore_invalid*/ false);
         if (old_dref) {
            SEVEREMSG("Active Display_Ref already exists for DRM connector %s", drm_connector_name);
            // how to handle?
            old_dref->flags |= DREF_REMOVED;
         }
         Display_Ref * dref = create_bus_display_ref(busno);
         dref->dispno = DISPNO_INVALID;   // -1, guilty until proven innocent
         dref->pedid = copy_parsed_edid(businfo->edid);    // needed?
         dref->mmid  = monitor_model_key_new(
                          dref->pedid->mfg_id,
                          dref->pedid->model_name,
                          dref->pedid->product_code);

         // drec->detail.bus_detail = businfo;
         dref->detail = businfo;
         dref->flags |= DREF_DDC_IS_MONITOR_CHECKED;
         dref->flags |= DREF_DDC_IS_MONITOR;

         g_ptr_array_add(all_display_refs, dref);

         DDCA_Display_Detection_Report report = {dref, DDCA_DISPLAY_ADDED};
         ddc_emit_display_detection_event(report);

         ok = true;
      }
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, ok, "");
   return ok;
}

#endif



#ifdef OLD_HOTPLUG_VERSION
typedef enum {Changed_None    = 0,
              Changed_Added   = 1,
              Changed_Removed = 2,
              Changed_Both    = 3,  // n. == Changed_Added | Changed_Removed
} Displays_Change_Type;

const char * displays_change_type_name(Displays_Change_Type change_type);


const char * displays_change_type_name(Displays_Change_Type change_type) {
   char * result = NULL;
   switch(change_type)
   {
   case Changed_None:    result = "Changed_None";        break;
   case Changed_Added:   result = "Changed_Added";   break;
   case Changed_Removed: result = "Changed_Removed"; break;
   case Changed_Both:    result = "Changed_Both";    break;
   }
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





#include "ddc_watch_displays_extended_poll.h"
