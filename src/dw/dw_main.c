/** @file dw_main.c */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"

#include "util/common_inlines.h"
#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddcutil_types_internal.h"
#include "base/dsa2.h"
#include "base/drm_connector_state.h"
#include "base/i2c_bus_base.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"
/** \endcond */

#include "sysfs/sysfs_base.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_display_ref_reports.h"

#include "dw_status_events.h"
#include "dw_common.h"
#include "dw_udev.h"
#include "dw_poll.h"
#include "dw_xevent.h"

#include "dw_main.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

DDCA_Watch_Mode  watch_displays_mode = DEFAULT_WATCH_MODE;
bool             enable_watch_displays = true;

static GThread * watch_thread = NULL;
static GMutex    watch_thread_mutex;
static DDCA_Display_Event_Class active_watch_displays_classes = DDCA_EVENT_CLASS_NONE;
static Watch_Displays_Data * global_wdd;     // needed to pass to dw_stop_watch_displays()


/** Determines the actual watch mode to be used
 *
 *  @param  initial_mode  mode requested
 *  @param  xev_data_loc  address at which to set the address of a newly allocated
 *                        XEvent_Data struct, if the resolved mode is Watch_Mode_Xevent
 *  @return actual watch mode to be used
 */
STATIC DDCA_Watch_Mode
resolve_watch_mode(DDCA_Watch_Mode initial_mode,  XEvent_Data ** xev_data_loc) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "initial_mode=%s xev_data_loc=%p", watch_mode_name(initial_mode), xev_data_loc);

   DDCA_Watch_Mode resolved_watch_mode = Watch_Mode_Poll;
   XEvent_Data * xevdata = NULL;
   *xev_data_loc = NULL;

#ifndef ENABLE_UDEV
   if (initial_mode == Watch_Mode_Udev)
      initial_mode = Watch_Mode_Poll;
#endif

   if (initial_mode == Watch_Mode_Dynamic) {
      resolved_watch_mode = Watch_Mode_Poll;    // always works, may be slow
      char * xdg_session_type = getenv("XDG_SESSION_TYPE");
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "XDG_SESSION_TYPE=|%s|", xdg_session_type);
      if (xdg_session_type &&         // can xdg_session_type ever not be set
          (streq(xdg_session_type, "x11") || streq(xdg_session_type,"wayland")))
      {
         resolved_watch_mode = Watch_Mode_Xevent;
      }
      else {
         // assert xdg_session_type == "tty"  ?
         char * display = getenv("DISPLAY");
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "xdg_session_type=|%s|, display=|%s|", xdg_session_type, display);
         // possibility of coming in on ssh with a x11 proxy running
         // see https://stackoverflow.com/questions/45536141/how-i-can-find-out-if-a-linux-system-uses-wayland-or-x11
         if (display) {
            resolved_watch_mode = Watch_Mode_Xevent;
         }
      }

      //     dw_watch_mode = Watch_Mode_Udev;
      // sysfs_fully_reliable = is_sysfs_reliable();
      // if (!sysfs_fully_reliable)
      //    dw_watch_mode = Watch_Mode_Poll;
   }
   else {
      resolved_watch_mode = initial_mode;
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "initially resolved watch mode = %s", watch_mode_name(resolved_watch_mode));

#ifdef NO
    if (resolved_watch_mode  == Watch_Mode_Udev) {
      if (!sysfs_fully_reliable)  // ???
         resolved_watch_mode = Watch_Mode_Poll;
   }
#endif

   if (resolved_watch_mode == Watch_Mode_Xevent) {
      xevdata  = dw_init_xevent_screen_change_notification();
      // *xev_data_loc  = ddc_init_xevent_screen_change_notification();
      if (!xevdata) {
         resolved_watch_mode = Watch_Mode_Poll;
         MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "X11 RANDR api unavailable. Switching to Watch_Mode_Poll");
      }
   }

   // DBG( "xevdata=%p, watch_mode = %s", xevdata, dw_watch_mode_name(resolved_watch_mode));

   *xev_data_loc = xevdata;
   // ASSERT_IFF(resolved_watch_mode == Watch_Mode_Xevent, xevdata);
   ASSERT_IFF(resolved_watch_mode == Watch_Mode_Xevent, *xev_data_loc);
   if (*xev_data_loc && IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      dw_dbgrpt_xevent_data(*xev_data_loc,  0);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "resolved_watch_mode: %s. *xev_data_loc: %p",
         watch_mode_name(resolved_watch_mode),  *xev_data_loc);
   return resolved_watch_mode;
}


/** Starts thread that watches for changes in display connection status.
 *
 *  @param  event_classes  types of events to watch for
 *  @return  Error_Info struct if error, possible status codes:
 *           -  DDCRC_INVALID_OPERATION  e.g. watch thread already started, watching disabled
 *           -  DDCRC_ARG                event_classes == DDCA_EVENT_CLASS_NONE
 */
Error_Info *
dw_start_watch_displays(DDCA_Display_Event_Class event_classes) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
        "dw_watch_mode = %s, watch_thread=%p, event_clases=0x%02x, all_video_adapters_implement_drm=%s",
        watch_mode_name(watch_displays_mode), watch_thread, event_classes, SBOOL(all_video_adapters_implement_drm));
   Error_Info * err = NULL;
   XEvent_Data * xev_data = NULL;

   if (!all_video_adapters_implement_drm) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Requires DRM video drivers");
      goto bye;
   }

   if (!enable_watch_displays) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watching for display changes disabled");
      goto bye;
   }

   DDCA_Watch_Mode resolved_watch_mode = resolve_watch_mode(watch_displays_mode, &xev_data);
   ASSERT_IFF(resolved_watch_mode == Watch_Mode_Xevent, xev_data);

   int calculated_watch_loop_millisec = dw_calc_watch_loop_millisec(resolved_watch_mode);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "calc_watch_loop_millisec() returned %d", calculated_watch_loop_millisec);
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,
         "Watching for display connection changes, resolved watch mode = %s, poll loop interval = %d millisec",
         watch_mode_name(resolved_watch_mode), calculated_watch_loop_millisec);

   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"          use_sysfs_connector_id:       %s", SBOOL(use_sysfs_connector_id));    // watch udev only
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"          extra_stabilization_millisec: %d,  stabilization_poll_millisec: %d",
         initial_stabilization_millisec, stabilization_poll_millisec);
   // MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,"          stabilization_poll_millisec:  %d", stabilization_poll_millisec);

   g_mutex_lock(&watch_thread_mutex);
   if (!(event_classes & (DDCA_EVENT_CLASS_DPMS|DDCA_EVENT_CLASS_DISPLAY_CONNECTION))) {
      err = ERRINFO_NEW(DDCRC_ARG, "Invalid event classes");
   }
   else if (watch_thread) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watch thread already running");
   }
   else {
      terminate_watch_thread = false;

      Watch_Displays_Data * wdd = calloc(1, sizeof(Watch_Displays_Data));
      memcpy(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
      wdd->main_process_id = pid();   // getpid();
      wdd->main_thread_id = tid();   get_thread_id();  // alt = syscall(SYS_gettid);
      // event_classes &= ~DDCA_EVENT_CLASS_DPMS;     // *** TEMP ***
      wdd->event_classes = event_classes;
      wdd->watch_mode = resolved_watch_mode;
      wdd->watch_loop_millisec = calculated_watch_loop_millisec;
      if (xev_data)
         wdd->evdata = xev_data;
      global_wdd = wdd;

      GThreadFunc watch_thread_func =
            (resolved_watch_mode == Watch_Mode_Poll || resolved_watch_mode == Watch_Mode_Xevent)
                 ? dw_watch_display_connections
                 : dw_watch_displays_udev;

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling g_thread_new()...");
      watch_thread = g_thread_new(
                       "watch_displays",             // optional thread name
                       watch_thread_func,
                       wdd);
      active_watch_displays_classes = event_classes;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Started watch_thread = %p", watch_thread);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "libddcutil watch thread %p started", watch_thread);
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
dw_stop_watch_displays(bool wait, DDCA_Display_Event_Class* enabled_classes_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "wait=%s, watch_thread=%p", SBOOL(wait), watch_thread );

   DDCA_Status ddcrc = DDCRC_OK;

#ifdef ENABLE_UDEV
   if (enabled_classes_loc)
      *enabled_classes_loc = DDCA_EVENT_CLASS_NONE;

   g_mutex_lock(&watch_thread_mutex);

   if (watch_thread) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "resolved_watch_mode = %s",
                                            watch_mode_name(global_wdd->watch_mode));
      if (global_wdd->watch_mode == Watch_Mode_Xevent) {
         if (terminate_using_x11_event) {   // for testing, does not currently work
            dw_send_x11_termination_message(global_wdd->evdata);
            DW_SLEEP_MILLIS(2*1000, "After ddc_send_x11_termination_message()");
         }
         else {
            terminate_watch_thread = true;
         }
      }
      else {
         terminate_watch_thread = true;  // signal watch thread to terminate
      }

      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
      // usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
      if (wait)
         g_thread_join(watch_thread);
      else
         g_thread_unref(watch_thread);
      watch_thread = NULL;
      if (enabled_classes_loc)
         *enabled_classes_loc = active_watch_displays_classes;
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


bool dw_is_watch_displays_executing() {
   return watch_thread;
}


/** If the watch thread is currently executing, returns the
 *  currently active display event classes as a bit flag.
 *
 *  @param  classes_loc  where to return bit flag
 *  @retval DDCRC_OK
 *  @retval DDCRC_INVALID_OPERATION watch thread not executing
 */
DDCA_Status
dw_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc) {
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "classes_loc = %p", classes_loc);
   DDCA_Status ddcrc = DDCRC_INVALID_OPERATION;
   *classes_loc = DDCA_EVENT_CLASS_NONE;
   g_mutex_lock(&watch_thread_mutex);
   if (watch_thread) {
      *classes_loc = active_watch_displays_classes;
      ddcrc = DDCRC_OK;
   }
   g_mutex_unlock(&watch_thread_mutex);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, ddcrc, "*classes_loc=0x%02x", *classes_loc);
   return ddcrc;
}


/** Called to completely redetect displays */
void
dw_redetect_displays() {
   bool debug = false || debug_locks;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays=%p", all_display_refs);
   SYSLOG2(DDCA_SYSLOG_NOTICE, "Display redetection starting.");

   DDCA_Display_Event_Class enabled_classes = DDCA_EVENT_CLASS_NONE;
   DDCA_Status active_rc = dw_get_active_watch_classes(&enabled_classes);
   if (active_rc == DDCRC_OK) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling ddc_stop_watch_displays()");
      DDCA_Status rc = dw_stop_watch_displays(/*wait*/ true, &enabled_classes);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Called ddc_stop_watch_displays()");
      assert(rc == DDCRC_OK);
   }
   ddc_discard_detected_displays();
   if (dsa2_is_enabled())
      dsa2_save_persistent_stats();
   // free_sysfs_drm_connector_names();

   if (use_drm_connector_states)
      redetect_drm_connector_states();

   // init_sysfs_drm_connector_names();
   // get_sys_drm_connectors(/*rescan=*/true);
   if (dsa2_is_enabled()) {
      Error_Info * erec = dsa2_restore_persistent_stats();
      if (erec) {
         MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Unexpected error from dsa2_restore_persistent_stats(): %s",
               errinfo_summary(erec));
         free(erec);
      }
   }
   i2c_detect_buses();
   g_mutex_lock(&all_display_refs_mutex);
   all_display_refs = ddc_detect_all_displays(&display_open_errors);
   g_mutex_unlock(&all_display_refs_mutex);
   if (debug) {
      ddc_dbgrpt_drefs("all_displays:", all_display_refs, 1);
   }
   if (active_rc == DDCRC_OK) {
      Error_Info * err = dw_start_watch_displays(enabled_classes);
      assert(!err);    // should never fail since restarting with same enabled classes
   }

   SYSLOG2(DDCA_SYSLOG_NOTICE, "Display redetection finished.");
   DBGTRC_DONE(debug, TRACE_GROUP, "all_displays=%p, all_displays->len = %d",
                                   all_display_refs, all_display_refs->len);
}


// Temporary hidden location, will move to api.base.c when/if published

void ddca_get_display_watch_settings(DDCA_DW_Settings * settings) {
   // settings->watch_mode = dw_watch_mode;

   settings->udev_watch_loop_interval_millisec   = udev_watch_loop_millisec;
   settings->poll_watch_loop_interval_millisec   = poll_watch_loop_millisec;
   settings->xevent_watch_loop_interval_millisec = xevent_watch_loop_millisec;

   settings->initial_stabilization_millisec      = initial_stabilization_millisec;
   settings->stabilization_poll_millisec         = stabilization_poll_millisec;

   // settings->watch_retry_thread_sleep_factor_millisec;
}


void ddca_set_display_watch_settings(DDCA_DW_Settings * settings) {
   udev_watch_loop_millisec   =     settings->udev_watch_loop_interval_millisec;
   poll_watch_loop_millisec   =     settings->poll_watch_loop_interval_millisec;
   xevent_watch_loop_millisec =     settings->xevent_watch_loop_interval_millisec;

   initial_stabilization_millisec = settings->initial_stabilization_millisec;
   stabilization_poll_millisec =    settings->stabilization_poll_millisec;

   // settings->watch_retry_thread_sleep_factor_millisec;
}


void init_dw_main() {
   RTTI_ADD_FUNC(dw_start_watch_displays);
   RTTI_ADD_FUNC(dw_stop_watch_displays);
   RTTI_ADD_FUNC(dw_get_active_watch_classes);
   RTTI_ADD_FUNC(resolve_watch_mode);
   RTTI_ADD_FUNC(dw_redetect_displays);
}


