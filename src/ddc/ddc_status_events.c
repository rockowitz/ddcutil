/** @file ddc_status_events.c */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"

#include "config.h"

#include "util/timestamp.h"

#include "base/core.h"
#include "base/rtti.h"

#include "i2c/i2c_sys_drm_connector.h"

#include "ddc_status_events.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

//
// Display Status Events
//

GPtrArray* display_detection_callbacks = NULL;

/** Registers a display status change event callback
 *
 *  @param  func      function to register
 *  @retval DDCRC_OK
 *  @retval DDCRC_INVALID_OPERATION ddcutil not built with UDEV support,
 *                                  or not all video devices support DRM
 *
 *  The function must be of type DDDCA_Display_Detection_Callback_Func.
 *  It is not an error if the function is already registered.
 */
DDCA_Status ddc_register_display_status_callback(DDCA_Display_Status_Callback_Func func) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "func=%p", func);

   DDCA_Status result = DDCRC_INVALID_OPERATION;
#ifdef ENABLE_UDEV
   // if (check_all_video_adapters_implement_drm()) {   // unnecessary, performed in caller
      // uint64_t t0 = cur_realtime_nanosec();
      generic_register_callback(&display_detection_callbacks, func);
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "generic_register_callback() took %"PRIu64" micoseconds",
      //      NANOS2MICROS(cur_realtime_nanosec()-t0) );
      result = DDCRC_OK;
   // }
#endif

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "");
   return result;
}


/** Unregisters a detection event callback function
 *
 *  @param  function of type DDCA_Display_Detection_Callback_func
 *  @retval DDCRC_OK normal return
 *  @retval DDCRC_NOT_FOUND function not in list of registered functions
 *  @retval DDCRC_INVALID_OPERATION ddcutil not built with UDEV support,
 *                                  or not all video devices support DRM
 */
DDCA_Status ddc_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "func=%p", func);

   DDCA_Status result = DDCRC_INVALID_OPERATION;
#ifdef ENABLE_UDEV
   if (check_all_video_adapters_implement_drm()) {
       result = generic_unregister_callback(display_detection_callbacks, func);
   }
#endif

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "");
   return result;
}


const char * ddc_display_event_class_name(DDCA_Display_Event_Class class) {
   char * result = NULL;
   switch(class) {
   case DDCA_EVENT_CLASS_NONE:               result = "DDCA_EVENT_CLASS_NONE";               break;
   case DDCA_EVENT_CLASS_DPMS:               result = "DDCA_EVENT_CLASS_DPMS";               break;
   case DDCA_EVENT_CLASS_DISPLAY_CONNECTION: result = "DDCA_EVENT_CLASS_DISPLAY_CONNECTION"; break;
   case DDCA_EVENT_CLASS_UNUSED1:            result = "DDCA_EVENT_CLASS_UNUSED1";            break;
   }
   return result;
}


const char * ddc_display_event_type_name(DDCA_Display_Event_Type event_type) {
   char * result = NULL;
   switch(event_type) {
   case DDCA_EVENT_DISPLAY_CONNECTED:    result = "DDCA_EVENT_DISPLAY_CONNECTED";    break;
   case DDCA_EVENT_DISPLAY_DISCONNECTED: result = "DDCA_EVENT_DISPLAY_DISCONNECTED"; break;
   case DDCA_EVENT_DPMS_AWAKE:           result = "DDCA_EVENT_DPMS_AWAKE";           break;
   case DDCA_EVENT_DPMS_ASLEEP:          result = "DDCA_EVENT_DPMS_ASLEEP";          break;
   case DDCA_EVENT_DDC_ENABLED:          result = "DDCA_EVENT_DDC_ENABLED";          break;
   case DDCA_EVENT_UNUSED2:              result = "DDCA_EVENT_UNUSED2";              break;
   }
   return result;
}


char * display_status_event_repr(DDCA_Display_Status_Event evt) {
   char * s = g_strdup_printf(
      "DDCA_Display_Status_Event[%s:  %s, %s, dref: %s, io_path:/dev/i2c-%d]",
      formatted_time_t(evt.timestamp_nanos),   // will this clobber a wrapping DBGTRC?
      ddc_display_event_type_name(evt.event_type),
      evt.connector_name,
      ddca_dref_repr_t(evt.dref),
      evt.io_path.path.i2c_busno);
   return s;
}


char * display_status_event_repr_t(DDCA_Display_Status_Event evt) {
   static GPrivate  display_status_repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&display_status_repr_key, 200);

   char * repr = display_status_event_repr(evt);
   g_snprintf(buf, 200, "%s", repr);
   free(repr);
   return buf;
}


DDCA_Display_Status_Event
ddc_create_display_status_event(
      DDCA_Display_Event_Type event_type,
      const char *            connector_name,
      Display_Ref*            dref,
      DDCA_IO_Path            io_path)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "event_type=%d, connector_name=%s, dref=%s, io_path=%s",
         event_type, connector_name, dref_reprx_t(dref), dpath_short_name_t(&io_path) );
   DDCA_Display_Status_Event evt;
   DBGMSF(debug, "sizeof(DDCA_Display_Status_Event) = %d, sizeof(evt) = %d",
         sizeof(DDCA_Display_Status_Event), sizeof(evt));
   evt.timestamp_nanos = elapsed_time_nanosec();
   evt.dref = dref_to_ddca_dref(dref);
   evt.event_type = event_type;
   if (connector_name)
      g_snprintf(evt.connector_name, sizeof(evt.connector_name), "%s", connector_name);
   else
      memset(evt.connector_name,0,sizeof(evt.connector_name));
   evt.io_path = (dref) ? dref->io_path : io_path;
   evt.unused[0] = 0;
   evt.unused[1] = 0;

   DBGTRC_RET_STRING(debug, DDCA_TRC_NONE, display_status_event_repr_t(evt), "");
   return evt;
}


/** Performs the actual work of executing the registered callbacks.
 *
 *  @param  evt
 */
void ddc_emit_display_status_record(
      DDCA_Display_Status_Event  evt)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "evt=%s", display_status_event_repr_t(evt));
   SYSLOG2(DDCA_SYSLOG_NOTICE, "Emitting %s",  display_status_event_repr_t(evt));

   if (display_detection_callbacks) {
      for (int ndx = 0; ndx < display_detection_callbacks->len; ndx++)  {
         DDCA_Display_Status_Callback_Func func = g_ptr_array_index(display_detection_callbacks, ndx);
         func(evt);
      }
   }

   SYSLOG2(DDCA_SYSLOG_NOTICE, "Executed %d registered callbacks.",
         (display_detection_callbacks) ? display_detection_callbacks->len : 0);
   DBGTRC_DONE(debug, TRACE_GROUP, "Executed %d callbacks",
         (display_detection_callbacks) ? display_detection_callbacks->len : 0);
}


GMutex emit_or_queue_mutex;

/** Assembles a #DDCA_Display_Status_Event record and either calls
 *  #ddc_emit_display_status_record to emit it immediately or adds it
 *  to a queue of event records
 *
 *  @param  event_type  e.g. DDCA_EVENT_CONNECTED, DDCA_EVENT_AWAKE
 *  @param  connector_name
 *  @param  dref        display reference, NULL if DDCA_EVENT_BUS_ATTACHED
 *                                              or DDCA_EVENT_BUS_DETACHED
 *  @param  io_path     for DDCA_EVENT_BUS_ATTACHED or DDCA_EVENT_BUS_DETACHED
 *  @param  queue       if non-null, append status event record
 */
void ddc_emit_or_queue_display_status_event(
      DDCA_Display_Event_Type event_type,
      const char *            connector_name,
      Display_Ref*            dref,
      DDCA_IO_Path            io_path,
      GArray*                 queue)
{
   bool debug = false;
   if (dref) {
      DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%p->%s, dispno=%d, DREF_REMOVED=%s, event_type=%d=%s, connector_name=%s",
            dref, dref_reprx_t(dref), dref->dispno, SBOOL(dref->flags&DREF_REMOVED),
            event_type, ddc_display_event_type_name(event_type), connector_name);
#ifdef NEW
      DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%p->%s, event_type=%d=%s",
            dref, dref_reprx_t(dref),
            event_type, ddc_display_event_type_name(event_type));
#endif
   }
   else {
      DBGTRC_STARTING(debug, TRACE_GROUP, "connector_name=%s, io_path=%s, event_type=%d=%s, dispno=%d",
            connector_name,
            dpath_repr_t(&io_path),
            event_type, ddc_display_event_type_name(event_type),
            dref->dispno);
   }

   DDCA_Display_Status_Event evt = ddc_create_display_status_event(
         event_type,
         connector_name,
         dref,
         io_path);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "event: %s", display_status_event_repr_t(evt));
   // SYSLOG2(DDCA_SYSLOG_NOTICE, "event: %s", display_status_event_repr(evt));

   g_mutex_lock(&emit_or_queue_mutex);  // or &emit_queue_mutex ???
   if (queue)
      g_array_append_val(queue,evt);   // TODO also need to lock where queue flushed
   else
      ddc_emit_display_status_record(evt);
   g_mutex_unlock(&emit_or_queue_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void init_ddc_status_events() {
   RTTI_ADD_FUNC(ddc_create_display_status_event);
   RTTI_ADD_FUNC(ddc_emit_or_queue_display_status_event);
   RTTI_ADD_FUNC(ddc_emit_display_status_record);
   RTTI_ADD_FUNC(ddc_register_display_status_callback);
   RTTI_ADD_FUNC(ddc_unregister_display_status_callback);
}
