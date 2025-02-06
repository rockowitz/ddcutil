/** @file dw_status_events.c */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"

#include "config.h"

#include "util/debug_util.h"
#include "util/timestamp.h"
#include "util/traced_function_stack.h"

#include "base/core.h"
#include "base/sleep.h"
#include "base/rtti.h"

#include "sysfs/sysfs_sys_drm_connector.h"

#include "ddc/ddc_display_ref_reports.h"

#include "dw_common.h"

#include "dw_status_events.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


/** Use a static_assert() to ensure that the allocated size of
 *  DDCA_Display_Status_Event is unchanged from that in a prior
 *  release.
 */
void assert_ddca_display_status_event_size_unchanged() {
   typedef struct {
      uint64_t                timestamp_nanos;
      DDCA_Display_Event_Type event_type;
      DDCA_IO_Path            io_path;
      char                    connector_name[32];
      DDCA_Display_Ref        dref;
      void *                  unused[2];
   } DDCA_Display_Status_Event_2_1_4;

   static_assert(sizeof(DDCA_Display_Status_Event) == sizeof(DDCA_Display_Status_Event_2_1_4));
}


//
// Thread that performs callbacks
//

GAsyncQueue *  callback_queue = NULL;
GMutex *  callback_queue_mutex = NULL;


#ifdef USE_CALLBACK_QUEUE
void dw_free_callback_queue_entry(Callback_Queue_Entry * entry) {
   // free_display_status_event(entry->event);  // ???
   free(entry);
}


GAsyncQueue * init_callback_queue() {
   callback_queue = g_async_queue_new();
   return callback_queue;
}


void dw_put_callback_queue(DDCA_Display_Status_Callback_Func func,
                           DDCA_Display_Status_Event         event)
{
   bool debug =  true;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "event=%s, func=%p", display_status_event_repr_t(event), func);

   Callback_Queue_Entry * entry = calloc(1, sizeof(Callback_Queue_Entry));
   entry->func = func;
   entry->event = event;   // or make copy?
   g_async_queue_push(callback_queue, entry);

   DBGTRC_DONE(debug, DDCA_TRC_CONN, "");
}


Callback_Queue_Entry * next_callback_queue_entry() {
   bool debug =  true;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "");

   int sleep_interval_millis = 200;    // temp
   int pop_interval_millis   = 100;
   uint64_t pop_interval_micros = MILLIS2MICROS(pop_interval_millis);

   Callback_Queue_Entry* cqe = NULL;
   while (!cqe && !terminate_watch_thread) {
      cqe = g_async_queue_timeout_pop(callback_queue, pop_interval_micros);
      sleep_millis(sleep_interval_millis);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %p", cqe);
   return cqe;
}


/** Function that runs in a thread to invoke user callback functions
 *
 *  @param  data  Callback_Displays_Data struct
 *  @return ??
 */
gpointer dw_callback_displays_func(gpointer data) {
   bool debug =  true;
   traced_function_stack_enabled = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "data=%p", data);
   Callback_Displays_Data*  cdd = (Callback_Displays_Data *) data;
   init_callback_queue();

   while (true) {
      Callback_Queue_Entry * entry = next_callback_queue_entry();
      if (!entry)
         break;
      DBGTRC_DONE(debug, TRACE_GROUP, "Invoking callback for event %s",
            display_status_event_repr_t(entry->event));
      entry->func(entry->event);
      DBGTRC_DONE(debug, TRACE_GROUP, "Callback function for event %s complete",
            display_status_event_repr_t(entry->event));
   }

   //clean up queue
   dw_free_callback_displays_data(cdd);

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "terminating callback thread execution");
   // g_thread_exit(NULL);   // not needed
   return NULL;   // avoid compiler warning
}

#endif



STATIC void
free_callback_queue_entry(Callback_Queue_Entry* q) {
   free(q);
}


/** Function that runs in a thread to invoke a single user callback functions
 *
 *  @param  data  Callback_Displays_Data struct
 *  @return ??
 */
gpointer dw_execute_callback_func(gpointer data) {
   bool debug = false;

   traced_function_stack_suspended = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "data=%p", data);
   Callback_Queue_Entry *  cqe = (Callback_Queue_Entry *) data;

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Invoking callback for event %s in this thread",
         display_status_event_repr_t(cqe->event));
   cqe->func(cqe->event);


   DBGTRC_DONE(debug, TRACE_GROUP, "Callback function for event %s complete",
         display_status_event_repr_t(cqe->event));
   free_callback_queue_entry(cqe);
   traced_function_stack_suspended = false;

   free_current_traced_function_stack();
   return NULL;   // terminates thread
}


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
DDCA_Status dw_register_display_status_callback(DDCA_Display_Status_Callback_Func func) {
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
DDCA_Status dw_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func) {
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


const char * dw_display_event_class_name(DDCA_Display_Event_Class class) {
   char * result = NULL;
   switch(class) {
   case DDCA_EVENT_CLASS_NONE:               result = "DDCA_EVENT_CLASS_NONE";               break;
   case DDCA_EVENT_CLASS_DPMS:               result = "DDCA_EVENT_CLASS_DPMS";               break;
   case DDCA_EVENT_CLASS_DISPLAY_CONNECTION: result = "DDCA_EVENT_CLASS_DISPLAY_CONNECTION"; break;
   case DDCA_EVENT_CLASS_UNUSED1:            result = "DDCA_EVENT_CLASS_UNUSED1";            break;
   }
   return result;
}


const char * dw_display_event_type_name(DDCA_Display_Event_Type event_type) {
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
      "DDCA_Display_Status_Event[%s:  %s, %s, dref: %s, io_path:/dev/i2c-%d, ddc working: %s]",
      formatted_time_t(evt.timestamp_nanos),   // will this clobber a wrapping DBGTRC?
      dw_display_event_type_name(evt.event_type),
                                  evt.connector_name,
                                  ddca_dref_repr_t(evt.dref),
                                  evt.io_path.path.i2c_busno,
                                  sbool(evt.flags&DDCA_DISPLAY_EVENT_DDC_WORKING));
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
dw_create_display_status_event(
      DDCA_Display_Event_Type event_type,
      const char *            connector_name,
      Display_Ref*            dref,
      DDCA_IO_Path            io_path)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "event_type=%d, connector_name=%s, dref=%p=%s, io_path=%s",
         event_type, connector_name, dref, dref_reprx_t(dref), dpath_short_name_t(&io_path) );
   assert(dref);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "dref->flags = %s", interpret_dref_flags_t(dref->flags));
   DDCA_Display_Status_Event evt;
   memset(&evt, 0, sizeof(evt));
   DBGMSF(debug, "sizeof(DDCA_Display_Status_Event) = %d, sizeof(evt) = %d",
         sizeof(DDCA_Display_Status_Event), sizeof(evt));
   evt.timestamp_nanos = elapsed_time_nanosec();
   evt.dref = dref_to_ddca_dref(dref);  // 0 if dref == NULL
   evt.event_type = event_type;
   if (connector_name)
      g_snprintf(evt.connector_name, sizeof(evt.connector_name), "%s", connector_name);
   else
      memset(evt.connector_name,0,sizeof(evt.connector_name));
   evt.io_path = (dref) ? dref->io_path : io_path;
   if (event_type == DDCA_DISPLAY_EVENT_DDC_WORKING)
      ASSERT_WITH_BACKTRACE(dref->flags&DREF_DDC_COMMUNICATION_WORKING);
   if ((event_type == DDCA_EVENT_DISPLAY_CONNECTED && (dref->flags&DREF_DDC_COMMUNICATION_WORKING))
      || event_type == DDCA_DISPLAY_EVENT_DDC_WORKING)
         evt.flags |= DDCA_DISPLAY_EVENT_DDC_WORKING;
   // evt.unused[0] = 0;
   // evt.unused[1] = 0;

   DBGTRC_RET_STRING(debug, DDCA_TRC_NONE, display_status_event_repr_t(evt), "");
   return evt;
}


/** Performs the actual work of executing the registered callbacks.
 *
 *  @param  evt
 */
void dw_emit_display_status_record(
      DDCA_Display_Status_Event  evt)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "evt=%s", display_status_event_repr_t(evt));
   SYSLOG2(DDCA_SYSLOG_NOTICE, "Emitting %s",  display_status_event_repr_t(evt));

   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "evet->dref -> ", dref_reprx_t(evt->dref));
   Display_Ref * dref0 = dref_from_published_ddca_dref(evt.dref);
   // DBGMSG("dref0=%p", dref0);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "event->dref -> %s", dref_reprx_t(dref0));

   // debug_current_traced_function_stack(false);
   // show_backtrace(0);
   // dbgrpt_display_ref(dref0, true, 2);
#ifdef OLD
   if (display_detection_callbacks) {
      traced_function_stack_suspended = true;
      for (int ndx = 0; ndx < display_detection_callbacks->len; ndx++)  {
         DDCA_Display_Status_Callback_Func func = g_ptr_array_index(display_detection_callbacks, ndx);
         func(evt);
      }
      traced_function_stack_suspended = false;
   }
#endif

#ifdef NEWER
   int callback_ct = (display_detection_callbacks) ? display_detection_callbacks->len : 0;
   if (display_detection_callbacks) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Putting %d event notifications on display_detection_callbacks queue",
            callback_ct);
      for (int ndx = 0; ndx < callback_ct; ndx++)  {
         DDCA_Display_Status_Callback_Func func = g_ptr_array_index(display_detection_callbacks, ndx);
         dw_put_callback_queue(func, evt);
      }
   }
   SYSLOG2(DDCA_SYSLOG_NOTICE,
         "Put %d event notification callbacks on display detection callbacks queue.",
         callback_ct);
   DBGTRC_DONE(debug, TRACE_GROUP,
         "Put %d event notification callbacks on display detection callbacks queue",
         callback_ct);
   }
#endif

   int callback_ct = (display_detection_callbacks) ? display_detection_callbacks->len : 0;
   if (display_detection_callbacks) {
      for (int ndx = 0; ndx < display_detection_callbacks->len; ndx++)  {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling g_thread_new()...");
         Callback_Queue_Entry * cqe = calloc(1, sizeof (Callback_Queue_Entry));
         cqe->event = evt;
         cqe->func =  g_ptr_array_index(display_detection_callbacks, ndx);
         // traced_function_stack_suspended = true;
         GThread * callback_thread = g_thread_new(
                                       "single_callback_worker",  // optional thread name
                                       dw_execute_callback_func,
                                       cqe);
         // traced_function_stack_suspended = false;
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Started callback_thread = %p", callback_thread);
         SYSLOG2(DDCA_SYSLOG_NOTICE, "libddcutil callback thread %p started", callback_thread);
      }
   }

   SYSLOG2(DDCA_SYSLOG_NOTICE, "Started %d event callback thread(s)", callback_ct);
   DBGTRC_DONE(debug, TRACE_GROUP, "Started %d event callback thread(s)", callback_ct);
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
void dw_emit_or_queue_display_status_event(
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
            event_type, dw_display_event_type_name(event_type), connector_name);
#ifdef NEW
      DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%p->%s, event_type=%d=%s",
            dref, dref_reprx_t(dref),
            event_type, dw_display_event_type_name(event_type));
#endif
   }
   else {
      DBGTRC_STARTING(debug, TRACE_GROUP, "connector_name=%s, io_path=%s, event_type=%d=%s",
            connector_name,
            dpath_repr_t(&io_path),
            event_type, dw_display_event_type_name(event_type));
   }
   // debug_current_traced_function_stack(false);   // ** TEMP **/

   DDCA_Display_Status_Event evt = dw_create_display_status_event(
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
      dw_emit_display_status_record(evt);
   g_mutex_unlock(&emit_or_queue_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
   // debug_current_traced_function_stack(false);   // ** TEMP **/
}


void init_dw_status_events() {
   RTTI_ADD_FUNC(dw_create_display_status_event);
   RTTI_ADD_FUNC(dw_emit_or_queue_display_status_event);
   RTTI_ADD_FUNC(dw_emit_display_status_record);
   RTTI_ADD_FUNC(dw_register_display_status_callback);
   RTTI_ADD_FUNC(dw_unregister_display_status_callback);
}
