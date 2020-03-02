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

// Defaults for new threads.  Default sleep multiplier factor can be adjusted,
// Default sleep multiplier count cannot.
static       double default_sleep_multiplier_factor = 1.0;
static const int    default_sleep_multiplier_count  = 1;
static       bool   default_dynamic_sleep_enabled   = false;

// Master table of sleep data for all threads
static GHashTable *  thread_sleep_data_hash = NULL;
static GMutex        thread_sleep_data_mutex;
static double        global_sleep_multiplier_factor = 1.0;   // as set by --sleep-multiplier option

static uint16_t default_maxtries[] = {
      INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES,
      INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES
     };



// Do not call while already holding lock.  Behavior undefined
void tsd_lock_all_thread_data() {
   g_mutex_lock(&thread_sleep_data_mutex);
}

// Do not call if not holding lock.  Behavior undefined.
void tsd_unlock_all_thread_data() {
   g_mutex_unlock(&thread_sleep_data_mutex);
}


#ifdef UNUSED
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


/** Enable or disable dynamic sleep adjustment on the current thread
 *
 *  \param enabled   true/false i.e. enabled, disabled
 */
void tsd_enable_dynamic_sleep(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "enabled = %s", sbool(enabled));
   Thread_Sleep_Data * data = tsd_get_thread_sleep_data();
   data->dynamic_sleep_enabled = enabled;
}


/** Output a debug report of a #Thread_Sleep_Data struct.
 *
 *  \param  data   pointer to #Thread_Sleep_Data struct
 *  \param  depth  logical indentation level
 */
void dbgrpt_thread_sleep_data(Thread_Sleep_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Thread_Sleep_Data", data, depth);
 //rpt_int( "sizeof(Thread_Sleep_Data)",  NULL, sizeof(Thread_Sleep_Data),   d1);
   rpt_int( "thread_id",                  NULL, data->thread_id,             d1);
   rpt_bool("initialized",                NULL, data->initialized,           d1);
   rpt_bool("dynamic_sleep_enabled",      NULL, data->dynamic_sleep_enabled, d1);
   rpt_vstring(d1, "sleep-multiplier value:           %15.2f", data->sleep_multiplier_factor);

   // Sleep multiplier adjustment:
#ifdef REF

   int    sleep_multiplier_ct    ;         // can be changed by retry logic
   int    highest_sleep_multiplier_value;  // high water mark
   int    sleep_multipler_changer_ct;      // number of function calls that adjusted multiplier ct

#endif
   rpt_int("sleep_multiplier_ct",         NULL, data->sleep_multiplier_ct,        d1);
   rpt_vstring(d1, "sleep_multiplier_changer_ct:      %15d",   data->sleep_multipler_changer_ct);
   rpt_vstring(d1, "highest_sleep_multiplier_ct:      %15d",   data->highest_sleep_multiplier_value);

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
   rpt_vstring(d1, "current_sleep_adjustmet_factor     %15.2f", data->current_sleep_adjustment_factor);
   rpt_vstring(d1, "thread_adjustment_increment        %15.2f", data->thread_adjustment_increment);
   rpt_int("adjustment_check_interval",   NULL, data->adjustment_check_interval, d1);

   // TODO: report maxtries
}


/** Output a report of a #Thread_Sleep_Data struct, intended for use in program output.
 *
 *  \param  data   pointer to #Thread_Sleep_Data struct
 *  \param  depth  logical indentation level
 */
void report_thread_sleep_data(Thread_Sleep_Data * data, int depth) {
   int d1 = depth+1;
   // int d2 = depth+2;
   rpt_vstring(depth, "Sleep data for thread: %3d", data->thread_id);
   rpt_label(depth,   "General:");
   rpt_vstring(d1,    "Current sleep-multiplier factor:  %5.2f", data->sleep_multiplier_factor);
   rpt_vstring(d1,    "Dynamic sleep enabled:             %s",  sbool(data->dynamic_sleep_enabled));

   rpt_label(depth,   "Sleep multiplier adjustment:");
   rpt_vstring(d1,    "Current adjustment:                %d", data->sleep_multiplier_ct);
   rpt_vstring(d1,    "Highest adjustment:                %d", data->highest_sleep_multiplier_value);
   rpt_label(  d1,    "Number of function calls");
   rpt_vstring(d1,    "   that performed adjustment:      %d", data->sleep_multipler_changer_ct);

   if ( data->dynamic_sleep_enabled ) {
      rpt_title("Dynamic Sleep Adjustment:  ", depth);
      rpt_vstring(d1, "Total successful reads:          %5d",   data->total_ok_status_count);
      rpt_vstring(d1, "Total reads with DDC error:      %5d",   data->total_error_status_count);
      rpt_vstring(d1, "Total ignored status codes:      %5d",   data->total_other_status_ct);
      rpt_vstring(d1, "Current sleep adjustment factor: %5.2f", data->current_sleep_adjustment_factor);
      rpt_vstring(d1, "Thread adjustment increment:     %5.2f", data->thread_adjustment_increment);
      rpt_vstring(d1, "Adjustment check interval        %5d",   data->adjustment_check_interval);

      rpt_vstring(d1, "Calls since last check:          %5d",   data->calls_since_last_check);
      rpt_vstring(d1, "Total adjustment checks:         %5d",   data->total_adjustment_checks);
      rpt_vstring(d1, "Number of adjustments:           %5d",   data->adjustment_ct);
      rpt_vstring(d1, "Number of excess adjustments:    %5d",   data->max_adjustment_ct);
      rpt_vstring(d1, "Final sleep adjustment:          %5.2f", data->current_sleep_adjustment_factor);
   }

   rpt_label(depth,"Retry settings:");
   rpt_vstring(d1, "Current maxtries:                  %d,%d,%d,%d",
                    data->current_maxtries[0], data->current_maxtries[1],
                    data->current_maxtries[2], data->current_maxtries[3]);
   rpt_vstring(d1, "Highest maxtries:                  %d,%d,%d,%d",
                    data->highest_maxtries[0], data->highest_maxtries[1],
                    data->highest_maxtries[2], data->highest_maxtries[3]);
   rpt_vstring(d1, "Lowest maxtries:                   %d,%d,%d,%d",
                    data->lowest_maxtries[0], data->lowest_maxtries[1],
                    data->lowest_maxtries[2], data->lowest_maxtries[3]);
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
   Thread_Sleep_Data * data = value;
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


// GCompareFunc function signature
static gint compare_int_list_entries(
      gconstpointer a,
      gconstpointer b)
{
   int ia = GPOINTER_TO_INT(a);
   int ib = GPOINTER_TO_INT(b);
   gint result = 0;
   if (ia < ib)
      result = -1;
   else if (ia > ib)
      result = 1;
   // DBGMSG("a=%p, ia=%d, b=%p, ib=%d, returning %d", a, ia, b, ib, result);
   return result;
}


/** Report all #Thread_Sleep_Data structs.  Note that this report includes
 *  structs for threads that have been closed.
 *
 *  \param depth  logical indentation depth
 */
void report_all_thread_sleep_data(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   if (!thread_sleep_data_hash) {
      rpt_vstring(depth, "No thread sleep data found");
      rpt_nl();
   }
   else {
      DBGMSF(debug, "hash table size = %d", g_hash_table_size(thread_sleep_data_hash));
      GList * keys = g_hash_table_get_keys (thread_sleep_data_hash);
      GList * new_head = g_list_sort(keys, compare_int_list_entries); // not working
      GList * l;
#ifdef OLD
      for (l = new_head; l != NULL; l = l->next) {
         int key = GPOINTER_TO_INT(l->data);
         DBGMSG("Key: %d", key);
      }
#endif

      rpt_vstring(depth, "Thread_Sleep_Data:");
      for (l = new_head; l != NULL; l = l->next) {
         int key = GPOINTER_TO_INT(l->data);
         DBGMSF(debug, "Key: %d", key);
         Thread_Sleep_Data * data = g_hash_table_lookup(thread_sleep_data_hash, l->data);
         assert(data);
         report_thread_sleep_data(data, depth+1);
         rpt_nl();
      }

      g_list_free(new_head);   // would keys also work?

#ifdef OLD
      rpt_vstring(depth, "Thread Sleep Data:");
      g_hash_table_foreach(
            thread_sleep_data_hash,
            tsd_report_one_thread_data_hash_table_entry,
            GINT_TO_POINTER(depth+1));
#endif
   }
   DBGMSF(debug, "Done");
}


#ifdef OLD
// Registers a Thread_Sleep_Data instance in the master hash table for all threads
static
void register_thread_sleep_data(Thread_Sleep_Data * per_thread_data) {
   bool debug = true;
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
   dbgrpt_thread_sleep_data(per_thread_data, 1);
   g_mutex_unlock(&thread_sleep_data_mutex);
}
#endif


// initialize a single instance
static void init_thread_sleep_data(Thread_Sleep_Data * data) {
   data->dynamic_sleep_enabled = default_dynamic_sleep_enabled;
   data->sleep_multiplier_ct = default_sleep_multiplier_count;
   data->highest_sleep_multiplier_value = 1;

   data->current_sleep_adjustment_factor = 1.0;
   data->initialized = true;
   data->sleep_multiplier_factor = global_sleep_multiplier_factor;    // default
   data->thread_adjustment_increment = global_sleep_multiplier_factor;
   data->adjustment_check_interval = 2;

   for (int ndx=0; ndx < DDCA_RETRY_TYPE_COUNT; ndx++) {
      data->current_maxtries[ndx] = default_maxtries[ndx];
      data->highest_maxtries[ndx] = default_maxtries[ndx];
      data->lowest_maxtries[ndx]  = default_maxtries[ndx];
   }
}

#ifdef OLD
// Retrieves Thead_Sleep_Data for the current thread
// Creates and initializes a new instance if not found
// static
Thread_Sleep_Data * get_thread_sleep_data0(bool create_if_necessary) {
   bool debug = false;
   pid_t cur_thread_id = syscall(SYS_gettid);
   // DBGMSF(debug, "Starting. create_if_necessary = %s", sbool(create_if_necessary));

   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);
   // gchar * buf =
   Thread_Sleep_Data * data =
   get_thread_fixed_buffer(
         &per_thread_key,
         sizeof(Thread_Sleep_Data));

   // Thread_Sleep_Data *data = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, data=%p\n", __func__, this_thread, data);

   // DBGMSF(debug, "data=%p, create_if_necessary=%s", data, sbool(create_if_necessary));
   assert  ( ((data->thread_id == 0) && !data->initialized)  ||
             ((data->thread_id != 0) && data->initialized) );
   if (data->thread_id == 0) {
  //  if (!data && create_if_necessary) {
      // DBGMSF(debug, "Creating Thread_Sleep_Data for thread %d", cur_thread_id);
     //  data = g_new0(Thread_Sleep_Data, 1);
      data->thread_id = cur_thread_id;
      init_thread_sleep_data(data);
      //  g_private_set(&per_thread_key, data);
      // dbgrpt_thread_sleep_data(data,1);
      register_thread_sleep_data(data);
   }
   DBGMSF(debug, "Returning: %p, Thread_Sleep_Data for thread %d", data, data->thread_id);
   return data;
}
#endif


/** Gets the #Thread_Sleep_Data struct for the current thread, using the
 *  current thread's id number. If the struct does not already exist, it
 *  is allocated and initialized.
 *
 *  \return pointer to #Thread_Sleep_Data struct
 *
 *  \remark
 *  The structs are maintained centrally rather than using a thread-local pointer
 *  to a block on the heap because the of a problems when the thread is closed.
 *  Valgrind complains of access errors for closed threads, even though the
 *  struct is on the heap and still readable.
 */
Thread_Sleep_Data * tsd_get_thread_sleep_data() {
   bool debug = false;
   pid_t cur_thread_id = syscall(SYS_gettid);
   // DBGMSF(debug, "Getting thread sleep data for thread %d", cur_thread_id);
   g_mutex_lock(&thread_sleep_data_mutex);
   if (!thread_sleep_data_hash) {
      thread_sleep_data_hash = g_hash_table_new(g_direct_hash, NULL);
   }
   Thread_Sleep_Data * data = g_hash_table_lookup(thread_sleep_data_hash,
                                            GINT_TO_POINTER(cur_thread_id));
   if (!data) {
      // DBGMSG("Thread_Sleep_Data not found for thread %d", cur_thread_id);
      data = g_new0(Thread_Sleep_Data, 1);
      data->thread_id = cur_thread_id;
      init_thread_sleep_data(data);

      g_hash_table_insert(thread_sleep_data_hash,
                          GINT_TO_POINTER(cur_thread_id),
                          data);
      DBGMSF(debug, "Created Thead_Sleep_Data struct for thread id = %d", data->thread_id);
      if (debug)
        dbgrpt_thread_sleep_data(data, 1);
   }
   g_mutex_unlock(&thread_sleep_data_mutex);
   return data;
}


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
   Thread_Sleep_Data * data = tsd_get_thread_sleep_data();
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
   Thread_Sleep_Data * data = tsd_get_thread_sleep_data();
   data->sleep_multiplier_factor = factor;
   data->thread_adjustment_increment = factor;
   DBGMSF(debug, "Done");
}


//
// Sleep Multiplier Count
//

/** Gets the multiplier count for the current thread.
 *
 *  \return multiplier count
 */
int tsd_get_sleep_multiplier_ct() {
   Thread_Sleep_Data * data = tsd_get_thread_sleep_data();
   return data->sleep_multiplier_ct;
}


/** Sets the multiplier count for the current thread.
 *
 *  \parsm multipler_ct  value to set
 */
void tsd_set_sleep_multiplier_ct(int multiplier_ct) {
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   Thread_Sleep_Data * data = tsd_get_thread_sleep_data();
   data->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > data->highest_sleep_multiplier_value)
      data->highest_sleep_multiplier_value = multiplier_ct;
   // DBGMSG("Setting sleep_multiplier_ct = %d", settings->sleep_multiplier_ct);
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
         Thread_Sleep_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         data->sleep_multiplier_factor = factor;
      }
   }
}
#endif


// Enable dynamic sleep on all existing threads
void tsd_enable_dsa_all(bool enable) {
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. enable = %s", sbool(enable) );
   default_dynamic_sleep_enabled = enable;  // for initializing new threads
   if (thread_sleep_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,thread_sleep_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Thread_Sleep_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         data->dynamic_sleep_enabled = enable;
      }
   }
}





void tsd_dsa_enable(bool enabled) {
   Thread_Sleep_Data * tsd = tsd_get_thread_sleep_data();
   tsd->dynamic_sleep_enabled = enabled;
}


// Enable or disable dynamic sleep for all current threads and new threads
void tsd_dsa_enable_globally(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "Executing.  enabled = %s", sbool(enabled));
   default_dynamic_sleep_enabled = enabled;
   tsd_enable_dsa_all(enabled) ;
}


// Is dynamic sleep enabled on the current thread?
bool tsd_dsa_is_enabled() {
   Thread_Sleep_Data * tsd = tsd_get_thread_sleep_data();
   return tsd->dynamic_sleep_enabled;
}

void tsd_set_dsa_enabled_default(bool enabled) {
   default_dynamic_sleep_enabled = enabled;
}



// Number of function executions that changed the multiplier
void tsd_bump_sleep_multiplier_changer_ct() {
   Thread_Sleep_Data * data = tsd_get_thread_sleep_data();
   data->sleep_multipler_changer_ct++;
}


void minmax_visitor(Thread_Sleep_Data * data, void * accumulator) {
   Global_Maxtries_Accumulator * acc = accumulator;
   if (data->highest_maxtries[acc->retry_type] > acc->max_highest_maxtries)
      acc->max_highest_maxtries = data->highest_maxtries[acc->retry_type];
   if (data->lowest_maxtries[acc->retry_type] < acc->min_lowest_maxtries) {
      DBGMSG("lowest maxtries = %d -> %d",
             acc->min_lowest_maxtries, data->lowest_maxtries[acc->retry_type]);
      acc->min_lowest_maxtries = data->lowest_maxtries[acc->retry_type];
   }
}

void tsd_apply_all(Tsd_Func func, void * arg) {
   bool debug = false;
   if (thread_sleep_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,thread_sleep_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Thread_Sleep_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         func(data, arg);
      }
   }
}


//
// Maxtries
//

static char * retry_class_descriptions[] = {
      "write only",
      "write-read",
      "multi-part read",
      "multi-part write"
};


char * retry_class_names[] = {
      "DDCA_WRITE_ONLY_TRIES",
      "DDCA_WRITE_READ_TRIES",
      "DDCA_MULTI_PART_READ_TRIES",
      "DDCA_MULTI_PART_WRITE_TRIES"
};

const char * ddc_retry_type_name(DDCA_Retry_Type type_id) {
   return retry_class_names[type_id];
}

const char * ddc_retry_type_description(DDCA_Retry_Type type_id) {
   return retry_class_descriptions[type_id];
}



void ddc_set_default_max_tries(DDCA_Retry_Type rcls, uint16_t new_maxtries) {
   bool debug = false;
   DBGMSF(debug, "Executing. rcls = %s, new_maxtries=%d", ddc_retry_type_name(rcls), new_maxtries);
   g_mutex_lock(&thread_sleep_data_mutex);
   default_maxtries[rcls] = new_maxtries;
   g_mutex_unlock(&thread_sleep_data_mutex);
}


#ifdef UNFINISHED
void ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = true;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   g_mutex_lock(&thread_sleep_data_mutex);
   Maxtries_Rec * mrec = get_thread_maxtries_rec();
   for (DDCA_Retry_Type type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         mrec->maxtries[type_id] = new_max_tries[type_id];
   }
   g_mutex_unlock(&thread_sleep_data_mutex);
}
#endif


void ddc_set_initial_thread_max_tries(DDCA_Retry_Type retry_type, uint16_t new_maxtries) {
   bool debug = true;
   DBGMSF(debug, "Executing. retry_class = %s, new_maxtries=%d",
                 ddc_retry_type_name(retry_type), new_maxtries);
   Thread_Sleep_Data * tsd = tsd_get_thread_sleep_data();
   tsd->current_maxtries[retry_type] = new_maxtries;
   tsd->highest_maxtries[retry_type] = new_maxtries;
   tsd->lowest_maxtries[retry_type] = new_maxtries;
}

void ddc_set_thread_max_tries(DDCA_Retry_Type retry_type, uint16_t new_maxtries) {
   bool debug = true;
   DBGMSF(debug, "Executing. retry_class = %s, new_max_tries=%d",
                 ddc_retry_type_name(retry_type), new_maxtries);
   Thread_Sleep_Data * tsd = tsd_get_thread_sleep_data();
   tsd->current_maxtries[retry_type] = new_maxtries;

   if (new_maxtries > tsd->highest_maxtries[retry_type])
      tsd->highest_maxtries[retry_type] = new_maxtries;
   if (new_maxtries < tsd->lowest_maxtries[retry_type])
      tsd->lowest_maxtries[retry_type] = new_maxtries;
}

#ifdef UNFINISHED
// sets all at once
void ddc_set_thread_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = true;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   Thread_Sleep_Data * tsd = tsd_get_thread_sleep_data();
   tsd->current_maxtries[retry_class] = new_max_tries;
   for (DDCA_Retry_Type type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         tsd->current_maxtries[type_id] = new_max_tries[type_id];
   }
}
#endif


uint16_t ddc_get_thread_max_tries(DDCA_Retry_Type type_id) {
   bool debug = true;
   Thread_Sleep_Data * tsd = tsd_get_thread_sleep_data();
   uint16_t result = tsd->current_maxtries[type_id];
   DBGMSF(debug, "retry type=%s, returning %d", ddc_retry_type_name(type_id), result);
   return result;
}


// void max_all_thread_highest_maxtries

// n returns value on stack, not pointer
Global_Maxtries_Accumulator tsd_get_all_threads_maxtries_range(DDCA_Retry_Type typeid) {

   Global_Maxtries_Accumulator accumulator;
   accumulator.retry_type = typeid;
   accumulator.max_highest_maxtries = 0;
   accumulator.min_lowest_maxtries = MAX_MAX_TRIES;

   tsd_apply_all(&minmax_visitor, &accumulator);

   return accumulator;
}

