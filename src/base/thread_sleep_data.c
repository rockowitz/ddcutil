/** @file thread_sleep_data.c
 *
 *  Struct Thread_Sleep_Data maintains all per-thread sleep data.
 *
 *  This file contains the usual access and report functions, along with
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

#include "base/per_thread_data.h"
#include "base/thread_sleep_data.h"

//
// Sleep time adjustment
//

/* Two multipliers are applied to the sleep time determined from the
 * IO mode and event type.
 *
 * sleep_multiplier_factor: set globally, e.g. from arg passed on
 * command line.  Consider making thread specific.
 *
 * sleep_multiplier_ct: Per thread adjustment,initiated by IO retries.
 */

// Defaults for new threads.  Default sleep multiplier factor can be adjusted,
// Default sleep multiplier count cannot.
static       double default_sleep_multiplier_factor = 1.0;
static const int    default_sleep_multiplier_count  = 1;
static       bool   default_dynamic_sleep_enabled   = false;
static       double global_sleep_multiplier_factor = 1.0;   // as set by --sleep-multiplier option


//
// Reporting
//

/** Output a report of the sleep data in a #Per_Thread_Data struct in a form
 * intended to be incorporated in program output.
 *
 *  \param  data   pointer to #Per_Thread_Data struct
 *  \param  depth  logical indentation level
 */
void report_thread_sleep_data(Per_Thread_Data * data, int depth) {
   ptd_cross_thread_operation_block();
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(depth, "Thread %d sleep data:", data->thread_id);
   rpt_label(  d1,   "General:");
   // if (data->dref)
   //    rpt_vstring(d2,    "Display:                           %s", dref_repr_t(data->dref) );
   rpt_vstring(d2,    "Description:                       %s", (data->description) ? data->description : "Not set");
   rpt_vstring(d2,    "Current sleep-multiplier factor:  %5.2f", data->sleep_multiplier_factor);
   rpt_vstring(d2,    "Dynamic sleep enabled:             %s",  sbool(data->dynamic_sleep_enabled));

   rpt_label(  d1,   "Sleep multiplier adjustment:");
   rpt_vstring(d2,    "Current adjustment:                %d", data->sleep_multiplier_ct);
   rpt_vstring(d2,    "Highest adjustment:                %d", data->highest_sleep_multiplier_value);
   rpt_label(  d2,    "Number of function calls");
   rpt_vstring(d2,    "   that performed adjustment:      %d", data->sleep_multipler_changer_ct);

   if ( data->dynamic_sleep_enabled ) {
      rpt_label(  d1, "Dynamic Sleep Adjustment:  ");
      rpt_vstring(d2, "Total successful reads:          %5d",   data->total_ok_status_count);
      rpt_vstring(d2, "Total reads with DDC error:      %5d",   data->total_error_status_count);
      rpt_vstring(d2, "Total ignored status codes:      %5d",   data->total_other_status_ct);
      rpt_vstring(d2, "Current sleep adjustment factor: %5.2f", data->current_sleep_adjustment_factor);
      rpt_vstring(d2, "Thread adjustment increment:     %5.2f", data->thread_adjustment_increment);
      rpt_vstring(d2, "Adjustment check interval        %5d",   data->adjustment_check_interval);

      rpt_vstring(d2, "Calls since last check:          %5d",   data->calls_since_last_check);
      rpt_vstring(d2, "Total adjustment checks:         %5d",   data->total_adjustment_checks);
      rpt_vstring(d2, "Number of adjustments:           %5d",   data->adjustment_ct);
      rpt_vstring(d2, "Number of excess adjustments:    %5d",   data->max_adjustment_ct);
      rpt_vstring(d2, "Final sleep adjustment:          %5.2f", data->current_sleep_adjustment_factor);
   }
}



#ifdef OLD
// typedef GFunc, invoked by g_hash_table_foreach
static void
tsd_report_one_thread_data_hash_table_entry(
      gpointer key,
      gpointer value,
      gpointer user_data)
{
   bool debug = true;
   DBGMSF(debug, "key (thread_id) = %d", GPOINTER_TO_INT(key));
   Per_Thread_Data * data = value;
   // This pointer is valid even after a thread goes away, since it
   // points to a block on the heap.  However, if a copy of the
   // pointer is stored in thread local memory, Valgrind complains
   // of an access error when the thread goes away.
   // DBGMSG("data=%p");
   // assert(data);
   // DBGMSG("key in data: %d", (int) data->thread_id);
   int depth = GPOINTER_TO_INT(user_data);
   // DBGMSG("depth=%d", depth);
   // dbgrpt_thread_sleep_data(data, 4);
   report_thread_sleep_data(data, depth);
   rpt_nl();
}
#endif


void wrap_report_thread_sleep_data(Per_Thread_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   // rpt_vstring(depth, "Per_Thread_Data:");  // needed?
   report_thread_sleep_data(data, depth);
}


/** Report all #Per_Thread_Data structs.  Note that this report includes
 *  structs for threads that have been closed.
 *
 *  \param depth  logical indentation depth
 */
void report_all_thread_sleep_data(int depth) {
   // int d1 = depth+1;
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(per_thread_data_hash);
   rpt_label(depth, "Per thread sleep data");
   // For debugging per-thread locks
   // rpt_vstring(d1, "ptd lock count:                  %d", ptd_lock_count);
   // rpt_vstring(d1, "ptd_unlock_count:                %d", ptd_unlock_count);
   // rpt_vstring(d1, "cross thread operations blocked: %d", cross_thread_operation_blocked_count);

   ptd_apply_all_sorted(&wrap_report_thread_sleep_data, GINT_TO_POINTER(depth+1) );
   DBGMSF(debug, "Done");
   rpt_nl();
}


#ifdef OLD
// Registers a Per_Thread_Data instance in the master hash table for all threads
static
void register_thread_sleep_data(Per_Thread_Data * per_thread_data) {
   bool debug = true;
   DBGMSF(debug, "per_thread_data=%p", per_thread_data);
   g_mutex_lock(&thread_sleep_data_mutex);
   if (!per_thread_data_hash) {
      per_thread_data_hash = g_hash_table_new(g_direct_hash, NULL);
   }
   assert(!g_hash_table_contains(per_thread_data_hash,
                                 GINT_TO_POINTER(per_thread_data->thread_id)));
   g_hash_table_insert(per_thread_data_hash,
                       GINT_TO_POINTER(per_thread_data->thread_id),
                       per_thread_data);
   DBGMSF(debug, "Inserted Thead_Sleep_Data for thread id = %d", per_thread_data->thread_id);
   dbgrpt_thread_sleep_data(per_thread_data, 1);
   g_mutex_unlock(&thread_sleep_data_mutex);
}
#endif


//
// Obtain, initialize, and reset sleep data for current thread


#ifdef OLD
// Retrieves Thead_Sleep_Data for the current thread
// Creates and initializes a new instance if not found
// static
Per_Thread_Data * get_thread_sleep_data0(bool create_if_necessary) {
   bool debug = false;
   pid_t cur_thread_id = syscall(SYS_gettid);
   // DBGMSF(debug, "Starting. create_if_necessary = %s", sbool(create_if_necessary));

   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);
   // gchar * buf =
   Per_Thread_Data * data =
   get_thread_fixed_buffer(
         &per_thread_key,
         sizeof(Per_Thread_Data));

   // Per_Thread_Data *data = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, data=%p\n", __func__, this_thread, data);

   // DBGMSF(debug, "data=%p, create_if_necessary=%s", data, sbool(create_if_necessary));
   assert  ( ((data->thread_id == 0) && !data->initialized)  ||
             ((data->thread_id != 0) && data->initialized) );
   if (data->thread_id == 0) {
  //  if (!data && create_if_necessary) {
      // DBGMSF(debug, "Creating Per_Thread_Data for thread %d", cur_thread_id);
     //  data = g_new0(Per_Thread_Data, 1);
      data->thread_id = cur_thread_id;
      init_thread_sleep_data(data);
      //  g_private_set(&per_thread_key, data);
      // dbgrpt_thread_sleep_data(data,1);
      register_thread_sleep_data(data);
   }
   DBGMSF(debug, "Returning: %p, Per_Thread_Data for thread %d", data, data->thread_id);
   return data;
}
#endif


Per_Thread_Data * tsd_get_thread_sleep_data() {
   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   assert(ptd->thread_sleep_data_defined);
#ifdef OLD  // init_thread_sleep_data() now called from per-thread data initializer
   if (!ptd->thread_sleep_data_defined) {
      // DBGMSG("thread_sleep_data_defined = false");
      init_thread_sleep_data(ptd);
      // DBGMSG("After initialization: ");
      // dbgrpt_per_thread_data(ptd, 4);
   }
#endif
   return ptd;
}


// initialize a single instance
void init_thread_sleep_data(Per_Thread_Data * data) {
   data->dynamic_sleep_enabled = default_dynamic_sleep_enabled;
   data->sleep_multiplier_ct = default_sleep_multiplier_count;
   data->highest_sleep_multiplier_value = 1;

   data->current_sleep_adjustment_factor = 1.0;
   data->initialized = true;
   data->sleep_multiplier_factor = global_sleep_multiplier_factor;    // default
   data->thread_adjustment_increment = global_sleep_multiplier_factor;
   data->adjustment_check_interval = 2;

   data->thread_sleep_data_defined = true;   // vs data->initialized
}


void reset_thread_sleep_data(Per_Thread_Data * data) {
   ptd_cross_thread_operation_block();
   data->highest_sleep_multiplier_value = data->sleep_multiplier_ct;
   data->sleep_multipler_changer_ct = 0;
   data->total_ok_status_count = 0;
   data->total_error_status_count = 0;
   data->total_other_status_ct = 0;
   data->total_adjustment_checks = 0;
   data->adjustment_ct = 0;
   data->max_adjustment_ct = 0;
}


void wrap_reset_thread_sleep_data(Per_Thread_Data * data, void * arg) {
   reset_thread_sleep_data(data);
}


void reset_all_thread_sleep_data() {
   if (per_thread_data_hash) {
      ptd_apply_all_sorted(&wrap_reset_thread_sleep_data, NULL );
   }
}




//
// Operations on all instances
//



//
// Sleep time adjustment
//

/* Two multipliers are applied to the sleep time determined from the
 * io mode and event type.
 *
 * A default sleep_multiplier_factor: is set globally,
 * e.g. from the --sleep-multiplier option passed on command line.
 * It can be adjusted on a per thread basis..
 *
 * The sleep multiplier count is intended for short-term dynamic
 * adjustment, typically be retry mechanisms within a single operation.
 * It is normally 1.
 */

//
// Sleep Multiplier Factor
//

/** Sets the default sleep multiplier factor, used for the creation of any new threads.
 * This is a global value and is a floating point number.
 *
 *  \param multiplier
 *
 *  \remark Intended for use only during program initialization.  If used
 *          more generally, get and set of default sleep multiplier needs to
 *          be protected by a lock.
 *  \todo
 *  Add Sleep_Event_Type bitfield to make sleep factor dependent on event type?
 */
void tsd_set_default_sleep_multiplier_factor(double multiplier) {
   assert(multiplier > 0 && multiplier < 100);
   default_sleep_multiplier_factor = multiplier;
   // DBGMSG("Setting sleep_multiplier_factor = %6.1f",set_sleep_multiplier_ct sleep_multiplier_factor);
}

/** Gets the default sleep multiplier factor.
 *
 *  \return sleep multiplier factor
 */
double tsd_get_default_sleep_multiplier_factor() {
   return default_sleep_multiplier_factor;
}


/** Gets the sleep multiplier factor for the current thread.
 *
 *  \return sleep mulitiplier factor
 */
double tsd_get_sleep_multiplier_factor() {
   bool debug = false;
   Per_Thread_Data * data = tsd_get_thread_sleep_data();
   double result = data->sleep_multiplier_factor;
   DBGMSF(debug, "Returning %5.2f", result );
   return result;
}


/** Sets the sleep multiplier factor for the current thread.
 *
 *  \parsm factor  sleep multiplier factor
 */
void tsd_set_sleep_multiplier_factor(double factor) {
   bool debug = false;

   // Need to guard with mutex!

   DBGMSF(debug, "Executing. factor = %5.2f", factor);
   ptd_cross_thread_operation_block();
   Per_Thread_Data * data = tsd_get_thread_sleep_data();
   data->sleep_multiplier_factor = factor;
   data->thread_adjustment_increment = factor;
   DBGMSF(debug, "Done");
}


#ifdef UNUSED
default_dynamic_sleep_enabled

void set_global_sleep_multiplier_factor(double factor) {
   bool debug = false;
   DBGMSF(debug, "factor = %5.2f", factor);
   global_sleep_multiplier_factor = factor;
   // set_sleep_multiplier_factor_all(factor);   // only applies to new threads, do not change existing threads
}


double get_global_sleep_multiplier_factor() {
   return global_sleep_multiplier_factor;
}
#endif




//
// Sleep Multiplier Count
//

/** Gets the multiplier count for the current thread.
 *
 *  \return multiplier count
 */
int tsd_get_sleep_multiplier_ct() {
   Per_Thread_Data * data = tsd_get_thread_sleep_data();
   return data->sleep_multiplier_ct;
}


/** Sets the multiplier count for the current thread.
 *
 *  \parsm multipler_ct  value to set
 */
void tsd_set_sleep_multiplier_ct(int multiplier_ct) {
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   ptd_cross_thread_operation_start();
   Per_Thread_Data * data = tsd_get_thread_sleep_data();
   data->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > data->highest_sleep_multiplier_value)
      data->highest_sleep_multiplier_value = multiplier_ct;
   ptd_cross_thread_operation_end();
   // DBGMSG("Setting sleep_multiplier_ct = %d", settings->sleep_multiplier_ct);
}


// Number of function executions that changed the multiplier
void tsd_bump_sleep_multiplier_changer_ct() {
   ptd_cross_thread_operation_block();
   Per_Thread_Data * data = tsd_get_thread_sleep_data();
   data->sleep_multipler_changer_ct++;
}


#ifdef UNUSED
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
         Per_Thread_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         data->sleep_multiplier_factor = factor;
      }
   }
}
#endif


//
// Dynamic Sleep
//

/** Enable or disable dynamic sleep adjustment on the current thread
 *
 *  \param enabled   true/false i.e. enabled, disabled
 */
void tsd_enable_dynamic_sleep(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "enabled = %s", sbool(enabled));
   ptd_cross_thread_operation_start();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();
   Per_Thread_Data * data = tsd_get_thread_sleep_data();
   data->dynamic_sleep_enabled = enabled;
   ptd_cross_thread_operation_end();
   // ptd_unlock_if_needed(this_function_owns_lock);
}


// Enable dynamic sleep on all existing threads
void tsd_enable_dsa_all(bool enable) {
   // needs mutex
   ptd_cross_thread_operation_start();
   bool debug = false;
   DBGMSF(debug, "Starting. enable = %s", sbool(enable) );
   default_dynamic_sleep_enabled = enable;  // for initializing new threads
   if (per_thread_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,per_thread_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Thread_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         data->dynamic_sleep_enabled = enable;
      }
   }
   ptd_cross_thread_operation_end();
}


void tsd_dsa_enable(bool enabled) {
   ptd_cross_thread_operation_block();
   Per_Thread_Data * tsd = tsd_get_thread_sleep_data();
   tsd->dynamic_sleep_enabled = enabled;
}


// Enable or disable dynamic sleep for all current threads and new threads
void tsd_dsa_enable_globally(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "Executing.  enabled = %s", sbool(enabled));
   ptd_cross_thread_operation_start();
   default_dynamic_sleep_enabled = enabled;
   tsd_enable_dsa_all(enabled) ;
   ptd_cross_thread_operation_end();
}


// Is dynamic sleep enabled on the current thread?
bool tsd_dsa_is_enabled() {
   ptd_cross_thread_operation_start();     // needed
   Per_Thread_Data * tsd = tsd_get_thread_sleep_data();
   bool result = tsd->dynamic_sleep_enabled;
   ptd_cross_thread_operation_end();
   return result;
}

void tsd_set_dsa_enabled_default(bool enabled) {
   default_dynamic_sleep_enabled = enabled;
}

bool tsd_get_dsa_enabled_default() {
   return default_dynamic_sleep_enabled;
}

