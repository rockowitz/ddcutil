/** @file ddc_watch_displays_main.c */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/drm_common.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/i2c_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
#include "util/udev_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_errno.h"
#include "base/drm_connector_state.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "i2c/i2c_sysfs_base.h"
#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_dpms.h"
#include "i2c/i2c_sys_drm_connector.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_status_events.h"
#include "ddc/ddc_vcp.h"

#include "ddc_watch_displays_common.h"
#include "ddc/ddc_watch_displays_udev.h"
#include "ddc_watch_displays_poll.h"

#include "ddc_watch_displays_main.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

DDC_Watch_Mode   ddc_watch_mode = Watch_Mode_Dynamic;
bool             enable_watch_displays = true;

// some of these go elsewhere
static GThread * watch_thread = NULL;
static DDCA_Display_Event_Class active_classes = DDCA_EVENT_CLASS_NONE;
static GMutex    watch_thread_mutex;


//
// Common to all variants
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
   DBGTRC_STARTING(debug, TRACE_GROUP,
      "ddc_watch_mode = %s, watch_thread=%p, event_clases=0x%02x, drm_enabled=%s",
      ddc_watch_mode_name(ddc_watch_mode), watch_thread, event_classes, SBOOL(drm_enabled));
   Error_Info * err = NULL;

   if (!drm_enabled) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Requires DRM video drivers");
      goto bye;
   }

   if (!enable_watch_displays) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watching for display changes disabled");
      goto bye;
   }

   bool sysfs_fully_reliable = true;
#ifdef WATCH_MODE_DYNAMIC_NOT_SUPPORTED
   sysfs_fully_reliable = false;
#ifndef ENABLE_UDEV
   ddc_watch_mode = Watch_Mode_Poll;
#else
   if (ddc_watch_mode == Watch_Mode_Dynamic) {
      ddc_watch_mode = Watch_Mode_Udev;
      sysfs_fully_reliable = is_sysfs_reliable();
      if (!sysfs_fully_reliable)
         ddc_watch_mode = Watch_Mode_Poll;
   }
#endif
#endif

   int calculated_watch_loop_millisec = calc_poll_loop_millisec(ddc_watch_mode);

   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,
         "Watching for display connection changes, watch mode = %s, poll loop interval = %d millisec",
         ddc_watch_mode_name(ddc_watch_mode), calculated_watch_loop_millisec);

   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"use_sysfs_connector_id:                 %s", SBOOL(use_sysfs_connector_id));    // watch udev only
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"force_sysfs_reliable=%s, force_sysfs_unreliable=%s", sbool(force_sysfs_reliable), sbool(force_sysfs_unreliable));
#ifdef UNUSED
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"sysfs_fully_reliable:                   %s", SBOOL(sysfs_fully_reliable));
#endif
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"use_x37_detection_table:                %s", SBOOL(use_x37_detection_table));   // check_x37_for_businfo()
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"try_get_edid_from_sysfs_first:          %s", SBOOL(try_get_edid_from_sysfs_first));  // i2c_edid_exists()
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"extra_stailization_millisec:            %d", extra_stabilization_millisec);
                                               // ddc_i2c_stabilized_single_bus_by_connector_id, i2c_stabilized_buses_bitset()  (both)
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"stabilization_poll_millisec:            %d", stabilization_poll_millisec);  // same

   extern int       extra_stabilization_millisec;
   extern int       stabilization_poll_millisec;


   g_mutex_lock(&watch_thread_mutex);
   if (!(event_classes & (DDCA_EVENT_CLASS_DPMS|DDCA_EVENT_CLASS_DISPLAY_CONNECTION))) {
      err = ERRINFO_NEW(DDCRC_ARG, "Invalid event classes");
   }
   else if (watch_thread) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watch thread already running");
   }
   else {
      terminate_watch_thread = false;
      Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
      memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
      data->main_process_id = getpid();
      data->main_thread_id = get_thread_id();  // alt = syscall(SYS_gettid);
      // event_classes &= ~DDCA_EVENT_CLASS_DPMS;     // *** TEMP ***
      data->event_classes = event_classes;

      GThreadFunc watch_thread_func = (ddc_watch_mode == Watch_Mode_Poll)
                                        ? ddc_watch_displays_without_udev
                                        : ddc_watch_displays_udev;

      watch_thread = g_thread_new(
                       "watch_displays",             // optional thread name
                       watch_thread_func,
                       data);
      active_classes = event_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "libddcutil watch thread started");
   }
   g_mutex_unlock(&watch_thread_mutex);

bye:
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "watch_thread=%p", watch_thread);
   return err;
}


/** Halts thread that watches for changes in display connection status.
 *
 *  @param   wait                if true, does not return until the watch thread exits,
 *                               if false, returns immediately
 *  @param   enabled_clases_loc  location at which to return watch classes that were active
 *  @retval  DDCRC_OK
 *  @retval  DDCRC_INVALID_OPERATION  watch thread not executing
 */
DDCA_Status
ddc_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "wait=%s, watch_thread=%p", SBOOL(wait), watch_thread );

   DDCA_Status ddcrc = DDCRC_OK;
#ifdef ENABLE_UDEV
   if (enabled_classes_loc)
      *enabled_classes_loc = DDCA_EVENT_CLASS_NONE;
   g_mutex_lock(&watch_thread_mutex);

   if (watch_thread) {
      terminate_watch_thread = true;  // signal watch thread to terminate
      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
      // usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
      if (wait)
         g_thread_join(watch_thread);

      g_thread_unref(watch_thread);
      watch_thread = NULL;
      if (enabled_classes_loc)
         *enabled_classes_loc = active_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread terminated.");
   }
   else {
      ddcrc = DDCRC_INVALID_OPERATION;
   }

   g_mutex_unlock(&watch_thread_mutex);
#endif

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


/** If the watch thread is currently executing, returns the
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


void init_ddc_watch_displays_main() {
   RTTI_ADD_FUNC(ddc_start_watch_displays);
   RTTI_ADD_FUNC(ddc_stop_watch_displays);
   RTTI_ADD_FUNC(ddc_get_active_watch_classes);
}


