// thread_sleep_data.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#include <assert.h>
#include <sys/types.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/debug_util.h"
#include "util/report_util.h"

#include "base/parms.h"
#include "base/dynamic_sleep.h"
#include "base/execution_stats.h"
#include "base/sleep.h"

#include "base/thread_sleep_data.h"


// Master table of sleep data for all threads
static GHashTable *  Thread_Sleep_Data_hash = NULL;
static GMutex        thread_sleep_data_mutex;
static double        global_sleep_multiplier_factor = 1.0;   // as set by --sleep-multiplier option

void set_global_sleep_multiplier_factor(double factor) {
   global_sleep_multiplier_factor = factor;
}


void dbgrpt_thread_sleep_data(Thread_Sleep_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Thread_Sleep_Data", data, depth);
   rpt_bool("initialized",         NULL, data->initialized,        d1);

   // Dynamic sleep adjustment:
   rpt_int("ok_status_count",      NULL, data->ok_status_count,    d1);
   rpt_int("error_status_count",   NULL, data->error_status_count, d1);
   rpt_int("other_status_ct",      NULL, data->other_status_ct,    d1);
   rpt_int("total_ok",             NULL, data->total_ok,           d1);
   rpt_int("total_error",          NULL, data->total_error,        d1);

   rpt_int("adjustment_ct",        NULL, data->adjustment_ct,      d1);
   rpt_int("max_adjustment_ct",    NULL, data->max_adjustment_ct,  d1);
   rpt_int("non_adjustment_ct",    NULL, data->non_adjustment_ct,  d1);

   // Sleep multiplier adjustment:
   rpt_vstring(d1, "sleep-multiplier value:    %5.2f", data->sleep_multiplier_factor);
   rpt_vstring(d1, "current_sleep_adjustment_factor:         %5.2f", data->current_sleep_adjustment_factor);
}


void report_thread_sleep_data(Thread_Sleep_Data * data, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(depth, "Per thread sleep data for thread %d", data->thread_id);
   rpt_nl();
   rpt_title("Dynamic Sleep Adjustment:  ", d1);
   rpt_vstring(d1, "Total successful reads:       %5d",   data->total_ok);
   rpt_vstring(d1, "Total reads with DDC error:   %5d",   data->total_error);
   rpt_vstring(d1, "Total ignored status codes:   %5d",   data->other_status_ct);

   rpt_vstring(d1, "Number of adjustments:        %5d",   data->adjustment_ct);
   rpt_vstring(d1, "Number of excess adjustments: %5d",   data->max_adjustment_ct);
   rpt_vstring(d1, "Final sleep adjustment:       %5.2f", data->current_sleep_adjustment_factor);
   rpt_nl();
   rpt_label(d1, "Multiplier count:");
   rpt_vstring(d2, "Max sleep multiplier count:     %d", data->max_sleep_multiplier_ct);
   rpt_vstring(d2, "Number of retry function calls that increased multiplier_count: %d",
                         data->sleep_multipler_changed_ct);
}


// typedef GFunc
void tsd_report_one_thread_data_hash_table_entry(
      gpointer key,
      gpointer value,
      gpointer user_data)
{
   // int thread_id = (int) key;
   Thread_Sleep_Data * settings = value;
   int depth = GPOINTER_TO_INT(user_data);
   report_thread_sleep_data(settings, depth);
   rpt_nl();
}


void report_all_thread_sleep_data(int depth) {
   if (!Thread_Sleep_Data_hash) {
      rpt_vstring(depth, "No per thread sleep data found");
   }
   else {
      rpt_vstring(depth, "Per thread sleep data:");
      g_hash_table_foreach(
            Thread_Sleep_Data_hash,
            tsd_report_one_thread_data_hash_table_entry,
            GINT_TO_POINTER(depth+1));
   }
}


// Registers a Thread_Sleep_Data instance in the master hash table for all threads
// static
void register_thread_sleep_data(Thread_Sleep_Data * per_thread_data) {
   g_mutex_lock(&thread_sleep_data_mutex);
   if (!Thread_Sleep_Data_hash) {
      Thread_Sleep_Data_hash = g_hash_table_new(g_direct_hash, NULL);
   }
   assert(!g_hash_table_contains(Thread_Sleep_Data_hash,
                                 GINT_TO_POINTER(per_thread_data->thread_id)));
   g_hash_table_insert(Thread_Sleep_Data_hash,
                       GINT_TO_POINTER(per_thread_data->thread_id),
                       per_thread_data);
   g_mutex_unlock(&thread_sleep_data_mutex);
}


// initialize a single instance
void init_thread_sleep_data(Thread_Sleep_Data * data) {
   data->sleep_multiplier_ct = 1;
   data->max_sleep_multiplier_ct = 1;

   data->current_sleep_adjustment_factor = 1.0;
   data->initialized = true;
   data->sleep_multiplier_factor = global_sleep_multiplier_factor;    // default
}


// Retrieves Thead_Sleep_Data for the current thread
// Creates and initializes a new instance if not found
// static
Thread_Sleep_Data *  get_thread_sleep_data(bool create_if_necessary) {
   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);

   Thread_Sleep_Data *data = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, data=%p\n", __func__, this_thread, data);

   if (!data && create_if_necessary) {
      data = g_new0(Thread_Sleep_Data, 1);
      data->thread_id = syscall(SYS_gettid);
      init_thread_sleep_data(data);
      g_private_set(&per_thread_key, data);
   }

   // printf("(%s) Returning: %p\n", __func__, settings);
   return data;
}




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
void   tsd_set_sleep_multiplier_ct(/* Sleep_Event_Type event_types,*/ int multiplier_ct) {
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   data->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > data->max_sleep_multiplier_ct)
      data->max_sleep_multiplier_ct = multiplier_ct;
   // DBGMSG("Setting sleep_multiplier_ct = %d", settings->sleep_multiplier_ct);
}

void tsd_bump_sleep_multiplier_changed_ct() {
   Thread_Sleep_Data * data = get_thread_sleep_data(true);
   data->sleep_multipler_changed_ct++;
}




