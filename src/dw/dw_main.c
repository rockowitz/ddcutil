/** @file dw_main.c */

// Copyright (C) 2018-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"

#include "util/common_inlines.h"
#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/error_info.h"
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
#include "i2c/i2c_edid.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_display_ref_reports.h"

#include "dw_status_events.h"
#include "dw_common.h"
#include "dw_udev2.h"
#include "dw_recheck.h"
#include "dw_poll.h"
#ifdef USE_X11
#include "dw_xevent.h"
#endif

#include "dw_main.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;

DDC_Watch_Mode  watch_displays_mode = DEFAULT_WATCH_MODE;
bool            enable_watch_displays = true;
bool            disable_check_all_edids_readable_using_i2c = false;

static GThread * watch_thread = NULL;
static GThread * recheck_thread = NULL;
// static GThread * callback_thread;
static GMutex    watch_thread_mutex;
static DDCA_Display_Event_Class active_watch_displays_classes = DDCA_EVENT_CLASS_NONE;
static Watch_Displays_Data * global_wdd;     // needed to pass to dw_stop_watch_displays()


typedef enum {
   unchecked,
   failed,
   succeeded,
} Watch_Mode_X11_Initialization;

Watch_Mode_X11_Initialization x11_init_state = unchecked;

STATIC bool is_watch_mode_x11_available() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   bool result = false;
#ifdef USE_X11
   if (!(x11_init_state == failed)) {
      char * xdg_session_type = getenv("XDG_SESSION_TYPE");
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "XDG_SESSION_TYPE=|%s|", xdg_session_type);
      if (xdg_session_type &&         // can xdg_session_type ever not be set
           (streq(xdg_session_type, "x11") || streq(xdg_session_type,"wayland")))
      {
         result = true;
      }
      else {
         // assert xdg_session_type == "tty"  ?
         char * display = getenv("DISPLAY");
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "xdg_session_type=|%s|, display=|%s|", xdg_session_type, display);
         // possibility of coming in on ssh with a x11 proxy running
         // see https://stackoverflow.com/questions/45536141/how-i-can-find-out-if-a-linux-system-uses-wayland-or-x11
         if (display) {
            result = true;
         }
      }
   }
#endif

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
}


STATIC bool is_watch_mode_udev_available() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   bool result = false;
   struct udev* udev = udev_new();
   if (udev) {
      result = true;
      udev_unref(udev);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
}


/** Determines the actual watch mode to be used
 *
 *  @param  initial_mode  mode requested
 *  @return actual watch mode to be used
 */
STATIC DDC_Watch_Mode
resolve_watch_mode(DDC_Watch_Mode initial_mode) {
  bool debug = false;
  DBGTRC_STARTING(debug, TRACE_GROUP, "initial_mode=%s", watch_mode_name(initial_mode));

   if (initial_mode == Watch_Mode_Xevent && !is_watch_mode_x11_available())
      initial_mode = Watch_Mode_Dynamic;
   if (initial_mode == Watch_Mode_Udev && !is_watch_mode_udev_available())
      initial_mode = Watch_Mode_Dynamic;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "After availability check, initial_mode = %s",
                                         watch_mode_name(initial_mode));

   char * xdg_session_type = getenv("XDG_SESSION_TYPE");
   DDC_Watch_Mode resolved_watch_mode = Watch_Mode_Poll;   // always works, may be slow
   if (initial_mode == Watch_Mode_Dynamic) {
      if (streq(xdg_session_type, "x11")) {
         if ( is_watch_mode_x11_available())
            resolved_watch_mode = Watch_Mode_Xevent;
         else if ( is_watch_mode_udev_available() )
            resolved_watch_mode = Watch_Mode_Udev;
      }
      else if (streq(xdg_session_type, "wayland")) {
         if (is_watch_mode_udev_available() )     // pathological if false
            resolved_watch_mode = Watch_Mode_Udev;
      }
      else {    // other session type, almost certainly "tty"
         if (is_watch_mode_udev_available() )
            resolved_watch_mode = Watch_Mode_Udev;
      }
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "After resolving watch_mode dynamic, resolved_watch_mode = %s",
            watch_mode_name(resolved_watch_mode));
   }
   else {
      resolved_watch_mode = initial_mode;
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "resolved_watch_mode: %s", watch_mode_name(resolved_watch_mode));
   return resolved_watch_mode;
}


/** Tests that a /dev/i2c bus can be opened for reading and writing.
 *
 *  @param  busno   i2c bus number
 *  @return NULL if success, Error_Info struct if failure
 */
Error_Info * simple_rw_test(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);

   int fd;
   Error_Info * err = i2c_open_bus_basic_by_busno(busno, CALLOPT_NONE, &fd);
   if (!err) {
      i2c_close_bus_basic(busno, fd, CALLOPT_NONE);
   }
   else {
      // MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Error opening /dev/i2c-%d: %s", busno, errinfo_summary(err));
   }

   DBGTRC_RET_ERRINFO(debug, DDCA_TRC_NONE, err, "busno=%d", busno);
   return err;
}


/** Checks that all /dev/i2c buses that may possibly be used for DDC
 *  communication can be read and written.
 *
 *  @return Error_Info struct if one or more buses are inaccessible,
 *          NULL if no problem
 */
Error_Info * all_relevant_i2c_buses_rw() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   GPtrArray * err_accumulator = NULL;

   BS256 attached_buses = nonlaptop_buses_bitset_from_businfo_array(all_i2c_buses, /*only_connected*/ false);
   Bit_Set_256_Iterator iter = bs256_iter_new(attached_buses);
   int busno = -1;
   while ( (busno = bs256_iter_next(iter)) >= 0) {
      Error_Info * err = simple_rw_test(busno);
      if (err) {
         if (!err_accumulator)
            err_accumulator = g_ptr_array_new_with_free_func((void*)errinfo_free);
         g_ptr_array_add(err_accumulator,err);
      }
   }
   bs256_iter_free(iter);

#ifdef ALT
   Byte_Value_Array bva =
   i2c_get_device_numbers_using_udev(/*include_ignorable_devices*/ false);
   for (int ndx=0; ndx<bva_length(bva); ndx++) {
      int busno = bva_get(bva, ndx);
      Error_Info * err = simple_rw_test(busno);
      if (err) {
         if (!err_accumulator)
            err_accumulator = g_ptr_array_new_with_free_func((void*)errinfo_free);
         g_ptr_array_add(err_accumulator,err);
      }
   }
   bva_free(bva);
#endif

   Error_Info * final_result = NULL;
   if (err_accumulator) {
      final_result = errinfo_new_with_causes_gptr(DDCRC_INVALID_OPERATION, err_accumulator, __func__,
            "Display change detection requires RW access to all I2C buses that might be used for DDC.");
      g_ptr_array_free(err_accumulator, true);
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, final_result, "");
   return final_result;
}


/** Checks that all EDIDS for Display_Refs of type I2C are actually
 *  readable using I2C. There are some cases, e.g. DisplayLink devices,
 *  where the EDID can be read only from /sys.
 *
 * @return true/false
 */
bool all_edids_readable_using_i2c() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   bool result = true;
   Error_Info * errs = all_relevant_i2c_buses_rw();
   if (errs) {
      result = false;
      syslog(LOG_WARNING, "%s", errs->detail);
      for (int ndx = 0; ndx < errs->cause_ct; ndx++) {
         syslog(LOG_WARNING, "   %s", errs->causes[ndx]->detail);
      }
      errinfo_free(errs);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "");
   return result;
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
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "thread_id = %d, traced_function_stack=%p", TID(), traced_function_stack);
   Error_Info * err = NULL;

   if (!all_video_adapters_implement_drm) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Requires DRM video drivers");
      goto bye;
   }

   if (!enable_watch_displays) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watching for display changes disabled");
      goto bye;
   }

   err = all_relevant_i2c_buses_rw();
   if (err) {
      syslog(LOG_ERR, "%s", err->detail);
      for (int ndx = 0; ndx < err->cause_ct; ndx++) {
         Error_Info * cause = err->causes[ndx];
         syslog(LOG_ERR, "   %s", cause->detail);
      }
      goto bye;
   }

   if (disable_check_all_edids_readable_using_i2c) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Suppressing call to all_edids_readable_using_i2c()");
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Suppressing call to all_edids_readable_using_i2c()");
   }
   else {
      if (!all_edids_readable_using_i2c()) {
         MSG_W_SYSLOG(DDCA_SYSLOG_WARNING,
               "EDID(s) readable from /sys but not using I2C. Display change detection unreliable.");
         // err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Requires EDIDs readable using I2C");
         // goto bye;
      }
   }

   DDC_Watch_Mode resolved_watch_mode = resolve_watch_mode(watch_displays_mode);
#ifdef USE_X11
   XEvent_Data * xevdata  = NULL;
   if (resolved_watch_mode == Watch_Mode_Xevent) {
      xevdata  = dw_init_xevent_screen_change_notification();
      if (xevdata) {
         x11_init_state = succeeded;
      }
      else {
         x11_init_state = failed;
         MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "X11 RANDR API unavailable. Switching to Watch_Mode_Dynamic");
         resolved_watch_mode = resolve_watch_mode(Watch_Mode_Dynamic);
      }
   }
#endif

   int calculated_watch_loop_millisec = dw_calc_watch_loop_millisec(resolved_watch_mode);
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "calc_watch_loop_millisec() returned %d", calculated_watch_loop_millisec);
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,
         "Watching for display connection changes, resolved watch mode = %s, poll loop interval = %d millisec",
         watch_mode_name(resolved_watch_mode), calculated_watch_loop_millisec);
   MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE,
         "                                         extra_stabilization_millisec: %d,  stabilization_poll_millisec: %d",
         initial_stabilization_millisec, stabilization_poll_millisec);

   g_mutex_lock(&watch_thread_mutex);
   if (!(event_classes & (DDCA_EVENT_CLASS_DPMS|DDCA_EVENT_CLASS_DISPLAY_CONNECTION))) {
      err = ERRINFO_NEW(DDCRC_ARG, "Invalid event classes");
   }
   else if (watch_thread) {
      err = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "Watch thread already running");
   }
   else {
      terminate_watch_thread = false;

      // Start recheck thread
      Recheck_Displays_Data * rdd = calloc(1, sizeof(Recheck_Displays_Data));
      memcpy(rdd->marker, RECHECK_DISPLAYS_DATA_MARKER,4);
      recheck_thread = g_thread_new("display_recheck_thread",             // optional thread name
                                    dw_recheck_displays_func,
                                    rdd);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Started recheck_thread = %p", recheck_thread);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "libddcutil recheck thread %p started", recheck_thread);

      // Start watch thread
      Watch_Displays_Data * wdd = calloc(1, sizeof(Watch_Displays_Data));
      memcpy(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
      wdd->main_process_id = pid();
      wdd->main_thread_id = tid();    // alt = syscall(SYS_gettid);
      // event_classes &= ~DDCA_EVENT_CLASS_DPMS;     // *** TEMP ***
      wdd->event_classes = event_classes;
      wdd->watch_mode = resolved_watch_mode;
      wdd->watch_loop_millisec = calculated_watch_loop_millisec;
#ifdef USE_X11
      if (xevdata)
         wdd->evdata = xevdata;
#endif
      global_wdd = wdd;   // so that it's available to ddc_stop_watch_displays() 

#ifdef CALLBACK_DISPLAYS_THREAD
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling g_thread_new()...");
      Callback_Displays_Data * cdd = dw_new_callback_displays_data();
      callback_thread = g_thread_new(
                       "callback_displays_thread",             // optional thread name
                       dw_callback_displays_func,
                       cdd);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Started callback_thread = %p", callback_thread);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "libddcutil callback thread %p started", callback_thread);
#endif

#ifdef OLD
      GThreadFunc watch_thread_func =
            (resolved_watch_mode == Watch_Mode_Poll || resolved_watch_mode == Watch_Mode_Xevent)
                 ? dw_watch_display_connections
                 : dw_watch_displays_udev;
#endif
      GThreadFunc watch_thread_func = dw_watch_display_connections;

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


/** Halts threads that watch for changes in display connection status.
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

   if (enabled_classes_loc)
      *enabled_classes_loc = DDCA_EVENT_CLASS_NONE;

   g_mutex_lock(&watch_thread_mutex);

   if (watch_thread) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "resolved_watch_mode = %s",
                                            watch_mode_name(global_wdd->watch_mode));
#ifdef USE_X11
      if (global_wdd->watch_mode == Watch_Mode_Xevent) {
         if (terminate_using_x11_event) {   // for testing, does not currently work
            dw_send_x11_termination_message(global_wdd->evdata);
            SLEEP_MILLIS_WITH_SYSLOG(2*1000, "After ddc_send_x11_termination_message()");
         }
         else {
            terminate_watch_thread = true;
         }
      }
      else {
         terminate_watch_thread = true;  // signal watch thread to terminate
      }
#else
      terminate_watch_thread = true;
#endif

      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Waiting %d millisec for watch thread to terminate...", 4000);
      // usleep(4000*1000);  // greater than the sleep in watch_displays_using_poll()
      if (wait) {
         g_thread_join(watch_thread);
         g_thread_join(recheck_thread);
      }
      else {
         g_thread_unref(watch_thread);
         g_thread_unref(recheck_thread);
      }
      watch_thread = NULL;
      if (enabled_classes_loc)
         *enabled_classes_loc = active_watch_displays_classes;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Watch thread terminated.");
   }
   else {
      ddcrc = DDCRC_INVALID_OPERATION;
   }

   g_mutex_unlock(&watch_thread_mutex);

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


/** Returns the current Display Watch parameter settings in a
 *  buffer provided by the caller.
 *
 *  @param settings  #DDCA_DW_Settings struct to fill ine
 */
void dw_get_display_watch_settings(DDCA_DW_Settings * settings) {
   assert(settings);
   // settings->watch_mode = dw_watch_mode;

   // settings->udev_watch_interval_millisec   = udev_watch_loop_millisec;
   settings->poll_watch_interval_millisec    = poll_watch_loop_millisec;
   settings->xevent_watch_interval_millisec  = xevent_watch_loop_millisec;

   settings->initial_stabilization_millisec  = initial_stabilization_millisec;
   settings->stabilization_poll_millisec     = stabilization_poll_millisec;

   settings->watch_retry_interval_millisec = retry_thread_sleep_factor_millisec;
}


/** Updates Display Watch tuning parameters
 *
 *  @param settings  #DDCA_DW_Settings struct containing the parameters
 */
DDCA_Status dw_set_display_watch_settings(DDCA_DW_Settings * settings) {
   assert(settings);
   // udev_watch_loop_millisec   =     settings->udev_watch_udev_interval_millisec;
   poll_watch_loop_millisec   =     settings->poll_watch_interval_millisec;
   xevent_watch_loop_millisec =     settings->xevent_watch_interval_millisec;

   initial_stabilization_millisec = settings->initial_stabilization_millisec;
   stabilization_poll_millisec =    settings->stabilization_poll_millisec;

   retry_thread_sleep_factor_millisec = settings->watch_retry_interval_millisec;

   return DDCRC_OK;
}


void init_dw_main() {
   RTTI_ADD_FUNC(all_edids_readable_using_i2c);
   RTTI_ADD_FUNC(all_relevant_i2c_buses_rw);
   RTTI_ADD_FUNC(dw_get_active_watch_classes);
   RTTI_ADD_FUNC(dw_redetect_displays);
   RTTI_ADD_FUNC(dw_start_watch_displays);
   RTTI_ADD_FUNC(dw_stop_watch_displays);
   RTTI_ADD_FUNC(is_watch_mode_udev_available);
   RTTI_ADD_FUNC(is_watch_mode_x11_available);
   RTTI_ADD_FUNC(resolve_watch_mode);
   RTTI_ADD_FUNC(simple_rw_test);
}

