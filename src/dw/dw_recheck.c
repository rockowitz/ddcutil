/** @file dw_recheck.c */

// Copyright (C) 2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>


#include "public/ddcutil_types.h"

#include "util/timestamp.h"
#include "util/traced_function_stack.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "ddc/ddc_displays.h"

#include "dw_common.h"
#include "dw_dref.h"
#include "dw_status_events.h"
#include "dw_poll.h"   // for process_event_mutex, todo: move

#include "dw_recheck.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


#ifdef UNUSED
static int simple_ipow(int base, int exponent) {
   assert(exponent >= 0);
   int result = 1;
   for (int i = 0; i < exponent; i++) {
      result = result * base;
   }
   return result;
}
#endif


void
emit_recheck_debug_msg(
      bool debug,
      DDCA_Syslog_Level syslog_level,
      const char * format, ...)
{
   va_list(args);
   va_start(args, format);
   char buffer[200];
   vsnprintf(buffer, 200, format, args);
   va_end(args);

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s", buffer);
   SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", buffer);
}


typedef struct {
   Display_Ref*  dref;
   uint64_t      initial_ts_nanos;
   int           sleepctr;
} Recheck_Queue_Entry;


void dw_free_recheck_queue_entry(Recheck_Queue_Entry * entry) {
   free(entry);
}

GAsyncQueue *  recheck_queue = NULL;
GMutex *  recheck_queue_mutex = NULL;


GAsyncQueue * init_recheck_queue() {
   recheck_queue = g_async_queue_new();
   return recheck_queue;
}


void dw_put_recheck_queue(Display_Ref* dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "dref=%s", dref_reprx_t(dref));

   Recheck_Queue_Entry * entry = calloc(1, sizeof(Recheck_Queue_Entry));
   entry->dref = dref;
   entry->initial_ts_nanos = cur_realtime_nanosec();
   entry->sleepctr = 0;

   // g_mutex_lock(recheck_queue_mutex);
   g_async_queue_push(recheck_queue, entry);
   // g_mutex_unlock(recheck_queue_mutex);

   DBGTRC_DONE(debug, DDCA_TRC_CONN, "");
}


#ifdef NO
typedef struct {
   GArray *    deferred_event_queue;
   GMutex *    deferred_event_queue_mutex;
} Recheck_Displays_Data;
#endif


/** Function that executes in the recheck thread thread to check if a DDC
 *  communication has become enabled for newly added display refs for which
 *  DDC communication was not initially detected as working.
 *
 *  @param   data  pointer to a #Recheck_Displays_Data struct
 *
 *  At increasing time intervals, each display ref is checked to see if DDC
 *  communication is working.  If so, a #DDC_Display_Status event of type
 *  #DDCA_EVENT_DDC_ENABLED is emitted, and the display ref is removed from
 *  from the array.
 *
 *  The time intervals are calculated as follows. The intervals are numbered
 *  from 0. The base interval is global variable #retry_thread_sleep_factor_millis.
 *  An adjustment factor is calculated as 2**i, where i is the interval number,
 *  i.e. 1, 2, 4, 8 etc.
 *
 *  The thread terminates when either all display refs have been removed from
 *  the array because communication has succeeded, or because the maximum of
 *  sleep intervals have occurred, i.e. until a total of
 *  (1+2+4+8)*retry_thread_sleep_factor_millis milliseconds have been slept.
 *
 *  On termination the **displays to recheck** array is freed.  Note that the
 *  display references themselves are not; display references are persistent.
 */
gpointer dw_recheck_displays_func(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "data=%p", data);
   Recheck_Displays_Data*  rdd = (Recheck_Displays_Data *) data;
   init_recheck_queue();
   // recheck_thread_active = true;

   // GPtrArray * displays_to_recheck = g_ptr_array_new();
#ifdef DEBUG
   for (int ndx = 0; ndx < displays_to_recheck->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(displays_to_recheck, ndx);
      DBGMSG("dref=%s", dref_reprx_t(dref));
   }
#endif

   GQueue * to_check_again = g_queue_new();

   int sleep_interval_millis = 200;    // temp
   int max_sleep_time_millis = 3000;
   int pop_interval_millis   = 100;

   while (!terminate_watch_thread) {
      DW_SLEEP_MILLIS(sleep_interval_millis, "Recheck interval");   // move to end of loop
      uint64_t cur_time_nanos = cur_realtime_nanosec();

      Recheck_Queue_Entry* rqe = NULL;
      while (!rqe && !terminate_watch_thread) {
         while (g_queue_get_length(to_check_again) > 0) {
            rqe = g_queue_pop_head(to_check_again);
            g_async_queue_push_front(recheck_queue, rqe);
         }
         uint64_t pop_interval_micros = MILLIS2MICROS(pop_interval_millis);
         rqe = g_async_queue_timeout_pop(recheck_queue, pop_interval_micros);
         if (terminate_watch_thread) {
            continue;
         }
      }
      if (terminate_watch_thread) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "terminating recheck thread execution");
         break;
      }

      if (cur_time_nanos > rqe->initial_ts_nanos + MILLIS2NANOS(max_sleep_time_millis)) {
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
               "ddc did not become enabled for %s after %d milliseconds",
                dref_reprx_t(rqe->dref), max_sleep_time_millis);
         dw_free_recheck_queue_entry(rqe);
         continue;
      }

      Display_Ref * dref = rqe->dref;
      // DBGMSG("   rechecking %s", dref_repr_t(dref));
      Error_Info * err = dw_recheck_dref(dref);    // <===
      DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "after dw_recheck_dref(), dref->flags=%s",
            interpret_dref_flags_t(dref->flags));
      // dbgrpt_display_ref(dref,false,2);
      if (!err) {
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
             "ddc became enabled for %s after %ld milliseconds",
              dref_reprx_t(dref), NANOS2MILLIS(cur_realtime_nanosec() - rqe->initial_ts_nanos));
         dref->dispno = ++dispno_max;

         DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "locking process_event_mutex");
         g_mutex_lock(&process_event_mutex);
         dw_emit_or_queue_display_status_event(
               DDCA_EVENT_DDC_ENABLED,
               dref->drm_connector,
               dref,
               dref->io_path,
               NULL);    //  deferred_event_queue);
         g_mutex_unlock(&process_event_mutex);
         DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "unlocked process_event_mutex");
         dw_free_recheck_queue_entry(rqe);
      }
      else if (err->status_code == DDCRC_DISCONNECTED) {
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
             "Display %s no longer detected after %u milliseconds",
             dref_reprx_t(dref),
             NANOS2MILLIS(cur_time_nanos - rqe->initial_ts_nanos));

         dref->dispno = DISPNO_REMOVED;
          dw_emit_or_queue_display_status_event(
                DDCA_EVENT_DISPLAY_DISCONNECTED,
                dref->drm_connector,
                dref,
                dref->io_path,
                NULL);   //                    rdd->deferred_event_queue);
          dw_free_recheck_queue_entry(rqe);
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                "ddc still not enabled for %s after %d milliseconds, retrying ...",
                dref_reprx_t(rqe->dref), sleep_interval_millis);
         g_queue_push_head(to_check_again, rqe);

      }
   }

   if (terminate_watch_thread) {
      char * s = "recheck thread terminating because watch thread terminated";
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s", s);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);

      // free what's left on the queue
      while (true) {
         Recheck_Queue_Entry * rqe = g_async_queue_timeout_pop(recheck_queue, 0);
         if (!rqe)
            break;
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_ERROR,
               "Flushing request queue entry for %s ",
                  dref_reprx_t(rqe->dref));
      }
   }

   free(rdd);
   DBGTRC_DONE(debug, TRACE_GROUP, "terminating recheck thread");
   free_current_traced_function_stack();
   // recheck_thread_active = false;
   g_thread_exit(NULL);
   return NULL;     // no effect, but avoids compiler error
}


void init_dw_recheck() {
   RTTI_ADD_FUNC(dw_recheck_displays_func);
}


