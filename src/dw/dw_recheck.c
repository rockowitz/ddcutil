/** @file dw_recheck.c
 *
 *  Process the queue of display references added in
 *  the main loop for which DDC communication was not
 *  immediately detected as enabled.
 */

// Copyright (C) 2025-2026 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "ddc/ddc_initial_checks.h"

#include "dw_common.h"
#include "dw_status_events.h"
#include "dw_poll.h"   //  or process_event_mutex, todo: move

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



void dw_free_recheck_displays_data(Recheck_Displays_Data * rdd) {
   if (rdd) {
      assert( memcmp(rdd->marker, RECHECK_DISPLAYS_DATA_MARKER, 4) == 0 );
      rdd->marker[3] = 'x';
      free(rdd);
   }
}



static void emit_recheck_debug_msg(
      bool debug,
      DDCA_Syslog_Level syslog_level,
      const char * format, ...)
{
   va_list args;
   va_start(args, format);
   char buffer[200];
   vsnprintf(buffer, 200, format, args);
   va_end(args);

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s", buffer);
   SYSLOG2(syslog_level, "%s", buffer);
}


typedef struct {
   Display_Ref*  dref;
   uint64_t      initial_ts_nanos;
   int           sleepctr;
} Recheck_Queue_Entry;


static void dw_free_recheck_queue_entry(Recheck_Queue_Entry * entry) {
   free(entry);
}


GAsyncQueue *  recheck_queue = NULL;
// GMutex *  recheck_queue_mutex = NULL;


static GAsyncQueue *
init_recheck_queue() {
   recheck_queue = g_async_queue_new();
   return recheck_queue;
}


/** Adds a display reference to the recheck queue
 *
 *  @param dref display reference
 */
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

// START NEW

static GAsyncQueue * finished_worker_queue = NULL;

static GAsyncQueue *
init_finished_worker_queue() {
   finished_worker_queue = g_async_queue_new();
   return finished_worker_queue;
}


/** Perform initial checks on a Display_Ref being rechecked
 *
 *  @param  dref   display reference
 *  @retval Error_Info(DDCRC_LOCKED)  from dref_lock()
 *
 *  from initial_checks_by_dref()
 *  DDCRC_DISCONNECTED
 *  from ddc_open_display()
 *  from initial_checks_by_dref(), _by_dh
 *  if DDCRC_OK, need to check DREF_COMMNICATION_WORKING
 */
STATIC Error_Info *
dw_recheck_dref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dref=%s", dref_reprx_t(dref));
   Error_Info * err = NULL;

   // DDCA_Status ddcrc = dref_lock(dref);
   // if (ddcrc != 0) {
   // err = ERRINFO_NEW(ddcrc, "dref_lock() failed for %s", dref_reprx_t(dref));
   // SYSLOG2(DDCA_SYSLOG_ERROR, "dref_lock() failed for %s", dref_reprx_t(dref));
   // }
   // else {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Obtained lock on %s:", dref_reprx_t(dref));
      err = ddc_initial_checks_by_dref(dref, false);
      dref_unlock(dref);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Released lock on %s:", dref_reprx_t(dref));
   // }

   DBGTRC_RET_ERRINFO(debug, DDCA_TRC_NONE, err, "");
   return err;
}


// NEW WAY


Error_Info *
dw_recheck_and_emit( Recheck_Queue_Entry* rqe) {
   bool debug = false;
   Error_Info * err = NULL;

   uint64_t cur_time_nanos = cur_realtime_nanosec();

    DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Locking master_dw_mutex, thread_id = %d", TID());
    g_mutex_lock(&master_dw_mutex);

    Display_Ref * dref = rqe->dref;
    // DBGMSG("   rechecking %s", dref_repr_t(dref));
    err = dw_recheck_dref(dref);    // <===
    DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "after dw_recheck_dref(), dref->flags=%s",
          interpret_dref_flags_t(dref->flags));
    // dbgrpt_display_ref(dref,false,2);
    // I2C_Bus_Info * businfo = (I2C_Bus_Info *) dref->detail;

    if (err->status_code == DDCRC_DISCONNECTED) {
          emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
              "Display %s no longer detected after %"PRIu64" milliseconds",
              dref_reprx_t(dref),
              NANOS2MILLIS(cur_time_nanos - rqe->initial_ts_nanos));
          dref->dispno = DISPNO_REMOVED;
          DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "locking process_event_mutex");
          g_mutex_lock(&process_event_mutex);
          dw_emit_or_queue_display_status_event(
                DDCA_EVENT_DISPLAY_DISCONNECTED,
                dref->drm_connector,
                dref,
                dref->io_path,
                NULL);   //                    rdd->deferred_event_queue);
          g_mutex_unlock(&process_event_mutex);
          DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "unlocked process_event_mutex");
          // dw_free_recheck_queue_entry(rqe);
    }
    else if (err) {
       // its a real errors
    }
    else {
       assert(!err);
       if (dref->flags&DREF_DDC_COMMUNICATION_WORKING) {
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
          // dw_free_recheck_queue_entry(rqe);
       }

       else {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                 "ddc still not enabled for %s, retrying at sleepctr=%d ...",
                 dref_reprx_t(rqe->dref), rqe->sleepctr + 1);
          // rqe->sleepctr++;   // not here
          // g_async_queue_push(recheck_queue, rqe);
       }
       // .. ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, DDCA_TRC_NONE) ||  is_report_ddc_errors_enabled() );
    }

    DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Unlocking master_dw_mutex, thread_id = %d", TID());
    g_mutex_unlock(&master_dw_mutex);

    return err;
 }

// NEW
gpointer dw_worker_thread_func(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "data=%p", data);
   Recheck_Queue_Entry *  rqe = (Recheck_Queue_Entry*) data;
   int max_sleepctr      = 4;
   // int pop_interval_millis = 100;

    while (!terminate_watch_thread) {
       // uint64_t pop_interval_micros = MILLIS2MICROS(pop_interval_millis);
       if (rqe->sleepctr >= max_sleepctr) {
          emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
                "ddc did not become enabled for %s after %d retries",
                dref_reprx_t(rqe->dref), max_sleepctr);
          // dw_free_recheck_queue_entry(rqe);
          break;
       }

       int sleep_ms = retry_thread_sleep_factor_millisec * (1 << rqe->sleepctr);
       // TO DO: replace with split_sleep()
       SLEEP_MILLIS_WITH_SYSLOG(sleep_ms, "Recheck interval");

       if (terminate_watch_thread) {
          break;
       }

       // uint64_t cur_time_nanos = cur_realtime_nanosec();

       DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Locking master_dw_mutex, thread_id = %d", TID());
       g_mutex_lock(&master_dw_mutex);

       Display_Ref * dref = rqe->dref;
       // DBGMSG("   rechecking %s", dref_repr_t(dref));
       Error_Info * err = dw_recheck_and_emit(rqe);
       DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "after dw_recheck_dref(), dref->flags=%s",
             interpret_dref_flags_t(dref->flags));

       if (err) {
          g_mutex_unlock(&master_dw_mutex);
          break;
       }
       if (!err && dref->flags&DREF_DDC_COMMUNICATION_WORKING) {
          g_mutex_unlock(&master_dw_mutex);
          break;
       }

       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                    "ddc still not enabled for %s, retrying at sleepctr=%d ...",
                    dref_reprx_t(rqe->dref), rqe->sleepctr + 1);
       rqe->sleepctr++;
       ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, DDCA_TRC_NONE) ||  is_report_ddc_errors_enabled() );

       DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Unlocking master_dw_mutex, thread_id = %d", TID());
       g_mutex_unlock(&master_dw_mutex);
    }

    if (terminate_watch_thread) {
       char * s = "recheck worker thread terminating because watch thread terminated";
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s", s);
       SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);
    }

    DBGTRC_DONE(debug, TRACE_GROUP, "terminating recheck worker thread");
    // free_current_traced_function_stack(); // ??

    g_async_queue_push(finished_worker_queue, g_thread_self());
   return NULL;
}


// NEW
gpointer dw_manager_thread_func(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "data=%p", data);
   Recheck_Displays_Data*  rdd = (Recheck_Displays_Data *) data;
   init_recheck_queue();
   // recheck_thread_active = true;

   GPtrArray* worker_threads = g_ptr_array_new();

   init_finished_worker_queue();
   int pop_interval_millis = 100;

     while (!terminate_watch_thread) {
        uint64_t pop_interval_micros = MILLIS2MICROS(pop_interval_millis);
        Recheck_Queue_Entry* rqe = g_async_queue_timeout_pop(recheck_queue, pop_interval_micros);

        if (terminate_watch_thread) {
           if (rqe)
              dw_free_recheck_queue_entry(rqe);
           DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "terminating recheck thread execution");
           break;
        }

        // check if any worker threads have terminated
        // if so, note its return value and remove from worker_threads array
        {
           GThread * done;
           while ((done = g_async_queue_try_pop(finished_worker_queue)) != NULL) {
              gpointer retval = g_thread_join(done);   // returns immediately; thread already finished
              DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Joined terminated worker thread %p, retval=%p", done, retval);
              g_ptr_array_remove(worker_threads, done);
           }
        }

        if (!rqe)    // pop failed with timeout
           continue;

        // create and start worker thread
         GThread * worker_thread = g_thread_new("recheck_worker_thread",             // optional thread name
               dw_worker_thread_func,                       rqe);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Started recheck workerrecheck_thread = %p", worker_thread);
         DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "libddcutil recheck worker_thread %p started", worker_thread);
         g_ptr_array_add(worker_threads, worker_thread);
      }

     dw_free_recheck_displays_data(rdd);   // actually just a free()
     DBGTRC_DONE(debug, TRACE_GROUP, "terminating recheck manager thread");
     // free_current_traced_function_stack();
     // recheck_thread_active = false;
     g_thread_exit(NULL);
     return NULL;     // no effect, but avoids compiler error
}

// END NEW


// TODO: make code conform to doc
/** Function that executes in the recheck thread to check if DDC
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
      SLEEP_MILLIS_WITH_SYSLOG(sleep_interval_millis, "Recheck interval");   // move to end of loop
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
            if (rqe) {
               dw_free_recheck_queue_entry(rqe);
               rqe = NULL;
            }
        }
      }
      if (terminate_watch_thread) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "terminating recheck thread execution");
         break;
      }

      // check if max recheck time has elapsed
      if (cur_time_nanos > rqe->initial_ts_nanos + MILLIS2NANOS(max_sleep_time_millis)) {
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
               "ddc did not become enabled for %s after %d milliseconds",
                dref_reprx_t(rqe->dref), max_sleep_time_millis);
         dw_free_recheck_queue_entry(rqe);
         continue;
      }

      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Locking master_dw_mutex, thread_id = %d", TID());
      g_mutex_lock(&master_dw_mutex);

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
      else {
         if (err->status_code == DDCRC_DISCONNECTED) {
            emit_recheck_debug_msg(debug, DDCA_SYSLOG_NOTICE,
                "Display %s no longer detected after %"PRIu64" milliseconds",
                dref_reprx_t(dref),
                NANOS2MILLIS(cur_time_nanos - rqe->initial_ts_nanos));

            dref->dispno = DISPNO_REMOVED;
            DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "locking process_event_mutex");
            g_mutex_lock(&process_event_mutex);
            dw_emit_or_queue_display_status_event(
                  DDCA_EVENT_DISPLAY_DISCONNECTED,
                  dref->drm_connector,
                  dref,
                  dref->io_path,
                  NULL);   //                    rdd->deferred_event_queue);
            g_mutex_unlock(&process_event_mutex);
            DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "unlocked process_event_mutex");
            dw_free_recheck_queue_entry(rqe);
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                   "ddc still not enabled for %s after %d milliseconds, retrying ...",
                   dref_reprx_t(rqe->dref), sleep_interval_millis);
            g_queue_push_head(to_check_again, rqe);
         }
         ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, DDCA_TRC_NONE) ||  is_report_ddc_errors_enabled() );
      }

      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Unlocking master_dw_mutex, thread_id = %d", TID());
      g_mutex_unlock(&master_dw_mutex);
   }

   if (terminate_watch_thread) {
      char * s = "recheck thread terminating because watch thread terminated";
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s", s);
      SYSLOG2(DDCA_SYSLOG_NOTICE, "%s", s);

      // free what's left on the queues
      while (g_queue_get_length(to_check_again) > 0) {
         Recheck_Queue_Entry * rqe = g_queue_pop_head(to_check_again);
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_ERROR,
               "Flushing to_check_again entry for %s",
                  dref_reprx_t(rqe->dref));
         dw_free_recheck_queue_entry(rqe);
      }
      while (true) {
         Recheck_Queue_Entry * rqe = g_async_queue_try_pop(recheck_queue);
         if (!rqe)
            break;
         emit_recheck_debug_msg(debug, DDCA_SYSLOG_ERROR,
               "Flushing recheck_queue entry for %s",
                  dref_reprx_t(rqe->dref));
         dw_free_recheck_queue_entry(rqe);
      }
   }

   g_queue_free(to_check_again);
   dw_free_recheck_displays_data(rdd);   // actually just a free()
   DBGTRC_DONE(debug, TRACE_GROUP, "terminating recheck thread");
   free_current_traced_function_stack();
   // recheck_thread_active = false;
   g_thread_exit(NULL);
   return NULL;     // no effect, but avoids compiler error
}


void init_dw_recheck() {
   RTTI_ADD_FUNC(dw_put_recheck_queue);
   RTTI_ADD_FUNC(dw_recheck_dref);
   RTTI_ADD_FUNC(dw_recheck_displays_func);
}
