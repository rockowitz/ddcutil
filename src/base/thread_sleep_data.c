/** @file thread_sleep_data.c
 *
 *  Struct Thread_Sleep_Data maintains all per-thread sleep data.
 *
 *  Thie file contains the usual access and report functions, along with
 *  small functions for managing various fields.
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#include <assert.h>
#include <sys/types.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/parms.h"
#include "base/core.h"
#include "base/sleep.h"

#include "base/thread_sleep_data.h"

// across all threads, used for Thread_Sleep_Data initialization
// used in report_dynamic_sleep_data(), avoid having this file call back into
// dynamic_sleep.c, which creates a circular dependency
static bool global_dynamic_sleep_enabled= false;

// Master table of sleep data for all threads
static GHashTable *  thread_sleep_data_hash = NULL;
static GMutex        thread_sleep_data_mutex;
static double        global_sleep_multiplier_factor = 1.0;   // as set by --sleep-multiplier option


void set_global_sleep_multiplier_factor(double factor) {
   bool debug = false;
   DBGMSF(debug, "factor = %5.2f", factor);
   global_sleep_multiplier_factor = factor;
   // set_sleep_multiplier_factor_all(factor);   // only applies to new threads, do not change existing threads
}


double get_global_sleep_multiplier_factor() {
   return global_sleep_multiplier_factor;
}

// this thread only
void tsd_enable_dynamic_sleep(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "enabled = %s", sbool(enabled));
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   data->dynamic_sleep_enabled = enabled;
}


void dbgrpt_thread_sleep_data(Thread_Sleep_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Thread_Sleep_Data", data, depth);
   rpt_bool("initialized",         NULL, data->initialized,        d1);
   rpt_bool("dynamic_sleep_enabled", NULL, data->dynamic_sleep_enabled, d1);

   // Dynamic sleep adjustment:
   rpt_int("current_ok_status_count",     NULL, data->current_ok_status_count,    d1);
   rpt_int("current_error_status_count",  NULL, data->current_error_status_count, d1);

   rpt_int("total_ok_status_count",       NULL, data->total_ok_status_count,      d1);
   rpt_int("total_error",                 NULL, data->total_error_status_count,   d1);
   rpt_int("other_status_ct",             NULL, data->total_other_status_ct,      d1);
   rpt_int("calls_since_last_check",      NULL, data->calls_since_last_check,     d1);
   rpt_int("total_adjustment_checks",     NULL, data->total_adjustment_checks,    d1);
   rpt_int("adjustment_ct",               NULL, data->adjustment_ct,              d1);
   rpt_int("max_adjustment_ct",           NULL, data->max_adjustment_ct,          d1);
   rpt_int("non_adjustment_ct",           NULL, data->non_adjustment_ct,          d1);
   rpt_vstring(d1, "current_sleep_adjustmet_factor     %5.2f", data->current_sleep_adjustment_factor);
   rpt_vstring(d1, "thread_adjustment_increment        %5.2f", data->thread_adjustment_increment);
   rpt_int("adjustment_check_interval",   NULL, data->adjustment_check_interval, d1);

   // Sleep multiplier adjustment:
   rpt_vstring(d1, "sleep-multiplier value:           %5.2f", data->sleep_multiplier_factor);
   rpt_vstring(d1, "current_sleep_adjustment_factor:  %5.2f", data->current_sleep_adjustment_factor);
}


void report_thread_sleep_data(Thread_Sleep_Data * data, int depth) {
   int d1 = depth+1;
   // int d2 = depth+2;
   rpt_vstring(depth, "Per thread sleep data for thread %6d", data->thread_id);
   rpt_label(depth, "General:");
   rpt_vstring(d1,    "Sleep-multiplier option value:   %5.2f", data->sleep_multiplier_factor);
   rpt_vstring(d1,    "Dynamic sleep enabled:           %5s",   sbool(data->dynamic_sleep_enabled));
   if ( data->dynamic_sleep_enabled ) {
      rpt_title("Dynamic Sleep Adjustment:  ", depth);
      rpt_vstring(d1, "Total successful reads:          %5d",   data->total_ok_status_count);
      rpt_vstring(d1, "Total reads with DDC error:      %5d",   data->total_error_status_count);
      rpt_vstring(d1, "Total ignored status codes:      %5d",   data->total_other_status_ct);
      rpt_vstring(d1, "Current sleep adjustment factor: %5.2f", data->current_sleep_adjustment_factor);
      rpt_vstring(d1, "Thread adjustment increment:     %5.2f", data->thread_adjustment_increment);
      rpt_vstring(d1, "Adjustment check interval        %5d",  data->adjustment_check_interval);

   }
   else {
      rpt_label(depth, "Sleep Adjustment:");
   }
   rpt_vstring(d1,    "Calls since last check:          %5d", data->calls_since_last_check);
   rpt_vstring(d1,    "Total adjustment checks:         %5d", data->total_adjustment_checks);
   rpt_vstring(d1,    "Number of adjustments:           %5d",   data->adjustment_ct);
   rpt_vstring(d1,    "Number of excess adjustments:    %5d",   data->max_adjustment_ct);

   rpt_vstring(d1,    "Final sleep adjustment:          %5.2f", data->current_sleep_adjustment_factor);
   rpt_label(depth,   "Multiplier count (set by retries):");
   rpt_vstring(d1,    "Max sleep multiplier count:      %5d", data->max_sleep_multiplier_ct);
   rpt_vstring(d1,    "Number of retry function calls that increased multiplier_count: %d",
                         data->sleep_multipler_changed_ct);
}


// typedef GFunc
void tsd_report_one_thread_data_hash_table_entry(
      gpointer key,
      gpointer value,
      gpointer user_data)
{
   // int thread_id = (int) key;
   Thread_Sleep_Data * data = value;
   int depth = GPOINTER_TO_INT(user_data);
   report_thread_sleep_data(data, depth);
   rpt_nl();
}


void report_all_thread_sleep_data(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   if (!thread_sleep_data_hash) {
      rpt_vstring(depth, "No thread sleep data found");
      rpt_nl();
   }
   else {
      rpt_vstring(depth, "Thread Sleep Data:");
      g_hash_table_foreach(
            thread_sleep_data_hash,
            tsd_report_one_thread_data_hash_table_entry,
            GINT_TO_POINTER(depth+1));
   }
   DBGMSF(debug, "Done");
}




// Registers a Thread_Sleep_Data instance in the master hash table for all threads
// static
void register_thread_sleep_data(Thread_Sleep_Data * per_thread_data) {
   bool debug = false;
   DBGMSF(debug, "per_thread_data=%p", per_thread_data);
   g_mutex_lock(&thread_sleep_data_mutex);
   if (!thread_sleep_data_hash) {
      thread_sleep_data_hash = g_hash_table_new(g_direct_hash, NULL);
   }
   assert(!g_hash_table_contains(thread_sleep_data_hash,
                                 GINT_TO_POINTER(per_thread_data->thread_id)));
   g_hash_table_insert(thread_sleep_data_hash,
                       GINT_TO_POINTER(per_thread_data->thread_id),
                       per_thread_data);
   DBGMSF(debug, "Inserted Thead_Sleep_Data for thread id = %d", per_thread_data->thread_id);
   g_mutex_unlock(&thread_sleep_data_mutex);
}


// initialize a single instance
void init_thread_sleep_data(Thread_Sleep_Data * data) {
   data->dynamic_sleep_enabled = global_dynamic_sleep_enabled;
   data->sleep_multiplier_ct = 1;
   data->max_sleep_multiplier_ct = 1;

   data->current_sleep_adjustment_factor = 1.0;
   data->initialized = true;
   data->sleep_multiplier_factor = global_sleep_multiplier_factor;    // default
   data->thread_adjustment_increment = global_sleep_multiplier_factor;
   data->adjustment_check_interval = 2;
}


// Retrieves Thead_Sleep_Data for the current thread
// Creates and initializes a new instance if not found
// static
Thread_Sleep_Data * get_thread_sleep_data(bool create_if_necessary) {
   bool debug = false;
   DBGMSF(debug, "Starting. create_if_necessary = %s", sbool(create_if_necessary));
   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);

   Thread_Sleep_Data *data = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, data=%p\n", __func__, this_thread, data);

   DBGMSF(debug, "data=%p, create_if_necessary=%s", data, sbool(create_if_necessary));
   if (!data && create_if_necessary) {
      data = g_new0(Thread_Sleep_Data, 1);
      data->thread_id = syscall(SYS_gettid);
      DBGMSF(debug, "Created Thread_Sleep_Data for thread %d", data->thread_id);
      init_thread_sleep_data(data);
      g_private_set(&per_thread_key, data);
      register_thread_sleep_data(data);
   }
   DBGMSF(debug, "Returning: %p", data);
   return data;
}


//
// Sleep time adjustment
//

/* Two multipliers are applied to the sleep time determined from the
 * io mode and event type.
 *
 * sleep_multiplier_factor: set globally, e.g. from arg passed on
 * command line.  Consider making thread specific.
 *
 * sleep_multiplier_ct: Per thread adjustment,initiated by io retries.
 */

// sleep_multiplier_factor is the --sleep-multiplier option value

double tsd_get_sleep_multiplier_factor() {
   bool debug = false;
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   double result = data->sleep_multiplier_factor;
   DBGMSF(debug, "Returning %5.2f", result );
   return result;
}


void tsd_set_sleep_multiplier_factor(double factor) {
   bool debug = false;

   // Need to guard with mutex!

   DBGMSF(debug, "Executing. factor = %5.2f", factor);
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   data->sleep_multiplier_factor = factor;
   data->thread_adjustment_increment = factor;
   DBGMSF(debug, "Done");
}


//
// Sleep Multiplier Count
//
// multiplier_count is set by functions performing retry,
// so always per-thread

/** Gets the multiplier count for the current thread.
 *
 *  \return multiplier count
 */
int tsd_get_sleep_multiplier_ct() {
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   return data->sleep_multiplier_ct;
}


/** Sets the multiplier count for the current thread.
 *
 *  \parsm multipler_ct  value to set
 */
void   tsd_set_sleep_multiplier_ct(int multiplier_ct) {
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   data->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > data->max_sleep_multiplier_ct)
      data->max_sleep_multiplier_ct = multiplier_ct;
   // DBGMSG("Setting sleep_multiplier_ct = %d", settings->sleep_multiplier_ct);
}


// apply the sleep-multiplier to any existing threads
// it will be set for new threads from global_sleep_multiplier_factor
void set_sleep_multiplier_factor_all(double factor) {
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. factor = %5.2f", factor);
   if (thread_sleep_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,thread_sleep_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Thread_Sleep_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         data->sleep_multiplier_factor = factor;
      }
   }
}

void tsd_enable_dynamic_sleep_all(bool enable) {
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. enable = %s", sbool(enable) );
   global_dynamic_sleep_enabled = enable;  // for initializing new threads
   if (thread_sleep_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,thread_sleep_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Thread_Sleep_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         tsd_enable_dynamic_sleep(enable);
      }
   }
}



// Number of function executions that changed the multiplier
void tsd_bump_sleep_multiplier_changer_ct() {
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   data->sleep_multipler_changed_ct++;
}

