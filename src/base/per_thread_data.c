/** @file per_thread_data.c
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <sys/types.h>

#include "util/debug_util.h"
#include "util/glib_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/timestamp.h"

#include "base/core.h"
#include "base/core_per_thread_settings.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "base/per_thread_data.h"

void ptd_profile_function_report(Per_Thread_Data * ptd, gpointer depth);

// Master table of sleep data for all threads
GHashTable *    per_thread_data_hash = NULL;   // key is thread id
static GPrivate this_thread_has_lock;
static GPrivate lock_depth; // GINT_TO_POINTER(0);
static bool     debug_mutex = false;
       int      ptd_lock_count = 0;
       int      ptd_unlock_count = 0;
       int      cross_thread_operation_blocked_count = 0;

void dbgrpt_per_thread_data_locks(int depth) {
   rpt_vstring(depth, "ptd_lock_count:                        %-4d", ptd_lock_count);
   rpt_vstring(depth, "ptd_unlock_count:                      %-4d", ptd_unlock_count);
   rpt_vstring(depth, "cross_thread_operation_blocked_count:  %-4d", cross_thread_operation_blocked_count);
}

static bool    cross_thread_operation_active = false;
static GMutex  cross_thread_operation_mutex;
static pid_t   cross_thread_operation_owner;

// The locking strategy relies on the fact that in practice conflicts
// will be rare, and critical sections short.
// Operations that occur on the
// are blocked only using a spin-lock.

// The groups of operations:
// - Operations that operate on the single Per_Thread_Data instance
//   associated with the currently executing thread.
// - Operations that operate on a single Per_Thread_Data instance,
//   but possibly not from the thread associated with the Per_Thread_Data instance.
// - Operations that operate on multiple Per_Thread_Data instances.
//   These are referred to as cross thread operations.
//   Alt, perhaps clearer, refer to them as multi-thread data instances.

/**
 */
bool ptd_cross_thread_operation_start() {
   // Only 1 cross thread action can be active at one time.
   // All per_thread actions must wait

   bool debug = false;
   debug = debug || debug_mutex;

   bool lock_performed = false;

   // which way is better?
   bool thread_has_lock  = GPOINTER_TO_INT(g_private_get(&this_thread_has_lock));
   int thread_lock_depth = GPOINTER_TO_INT(g_private_get(&lock_depth));
   assert ( ( thread_has_lock && thread_lock_depth  > 0) ||
            (!thread_has_lock && thread_lock_depth == 0) );
   DBGMSF(debug, "Already locked: %s", sbool(thread_has_lock));

   if (thread_lock_depth == 0) {    // (A)
   // if (!thread_has_lock) {
      // thread_lock_depth is per-thread, so must be unchanged from (A)
      g_mutex_lock(&cross_thread_operation_mutex);
      lock_performed = true;
      cross_thread_operation_active = true;

      ptd_lock_count++;

      // should this be a depth counter rather than a boolean?
      g_private_set(&this_thread_has_lock, GINT_TO_POINTER(true));

      Thread_Output_Settings * thread_settings = get_thread_settings();
      // intmax_t cur_thread_id = get_thread_id();
      intmax_t cur_thread_id = thread_settings->tid;
      cross_thread_operation_owner = cur_thread_id;
      DBGMSF(debug, "Locked by thread %d", cur_thread_id);
      sleep_millis(10);   // give all per-thread functions time to finish
   }
   g_private_set(&lock_depth, GINT_TO_POINTER(thread_lock_depth+1));
   DBGMSF(debug, "Returning: %s", sbool(lock_performed) );
   return lock_performed;
}


void ptd_cross_thread_operation_end() {
   int thread_lock_depth = GPOINTER_TO_INT(g_private_get(&lock_depth));
   g_private_set(&lock_depth, GINT_TO_POINTER(thread_lock_depth-1));
   assert(thread_lock_depth >= 1);

   if (thread_lock_depth == 1) {
   // if (unlock_requested) {
      cross_thread_operation_active = false;
      cross_thread_operation_owner = 0;
      g_private_set(&this_thread_has_lock, false);
      ptd_unlock_count++;
      assert(ptd_lock_count == ptd_unlock_count);
      g_mutex_unlock(&cross_thread_operation_mutex);
   }
   else {
      assert( ptd_lock_count > ptd_unlock_count );
   }
}


/** Block execution of single Per_Thread_Data operations when an operation
 *  involving multiple Per_Thead_Data instances is active.
 */

void ptd_cross_thread_operation_block() {
   // intmax_t cur_threadid = get_thread_id();
   Thread_Output_Settings * thread_settings = get_thread_settings();
   intmax_t cur_threadid = thread_settings->tid;
   if (cross_thread_operation_active && cur_threadid != cross_thread_operation_owner) {
      __sync_fetch_and_add(&cross_thread_operation_blocked_count, 1);
      do {
         sleep_millis(10);
      } while (cross_thread_operation_active);
   }
}


// satisfies type GDestroyNotify: void (*GDestroyNotify) (gpointer data);
void per_thread_data_destroy(void * data) {
   if (data) {
      Per_Thread_Data * ptd = data;
#ifdef REMOVED_20
      free(ptd->description);
#endif
      free(ptd->cur_func);
      if (ptd->function_stats)
         g_hash_table_destroy(ptd->function_stats);
      free(ptd);
   }
}


void terminate_per_thread_data() {
   if (per_thread_data_hash) {
      g_hash_table_destroy(per_thread_data_hash);
   }
}

//
// Locking
//

#ifdef PTD
static void ptd_init(Per_Thread_Data * ptd) {
}
#endif


/** Gets the #Per_Thread_Data struct for the current thread, using the
 *  current thread's id number. If the struct does not already exist, it
 *  is allocated and initialized.
 *
 *  @return pointer to #Per_Thread_Data struct
 *
 *  @remark
 *  The structs are maintained centrally rather than using a thread-local pointer
 *  to a block on the heap because the of a problem when the thread is closed.
 *  Valgrind complains of access errors for closed threads, even though the
 *  struct is on the heap and still readable.
 */
Per_Thread_Data * ptd_get_per_thread_data() {
   bool debug = false;
   // intmax_t cur_thread_id = get_thread_id();
   Thread_Output_Settings * thread_settings = get_thread_settings();
   intmax_t cur_thread_id = thread_settings->tid;
   // DBGMSF(debug, "Getting thread sleep data for thread %d", cur_thread_id);
   // bool this_function_owns_lock = ptd_lock_if_unlocked();
   assert(per_thread_data_hash);    // allocated by init_thread_data_module()
   // DBGMSG("per_thread_data_hash = %p", per_thread_data_hash);
   // n. data hash for current thread can only be looked up from current thread,
   // so there's nothing can happen to per_thread_data_hash before g_hash_table_insert()
   Per_Thread_Data * data = g_hash_table_lookup(per_thread_data_hash,
                                            GINT_TO_POINTER(cur_thread_id));
   if (!data) {
      DBGMSF(debug, "==> Per_Thread_Data not found for thread %d", cur_thread_id);
      data = g_new0(Per_Thread_Data, 1);
      data->thread_id = cur_thread_id;
      // data->sleep_multiplier = -1.0f;
      g_private_set(&lock_depth, GINT_TO_POINTER(0));
#ifdef PTD
      ptd_init(data);
      DBGMSF(debug, "Initialized: %s. thread_sleep_data_defined: %s. thread_retry_data_defined; %s",
           sbool(data->initialized),
           sbool( data->thread_sleep_data_defined), sbool( data->thread_retry_data_defined));
#endif

      g_hash_table_insert(per_thread_data_hash,
                          GINT_TO_POINTER(cur_thread_id),
                          data);
      DBGMSF(debug, "Created Per_Thread_Data struct for thread id = %d", data->thread_id);
      DBGMSF(debug, "per_thread_data_hash size=%d", g_hash_table_size(per_thread_data_hash));
      if (debug)
        dbgrpt_per_thread_data(data, 1);
   }
   // ptd_unlock_if_needed(this_function_owns_lock);
   return data;
}


#ifdef REMOVED_20
// Thread description operations always operate on the Per_Thread_Data
// instance for the currently executing thread.

void ptd_set_thread_description(const char * description) {
   ptd_cross_thread_operation_block();
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   // DBGMSG("thread: %d, description: %s", ptd->thread_id, description);
   if (ptd->description)
      free(ptd->description);
   ptd->description = g_strdup(description);
}


void ptd_append_thread_description(const char * addl_description) {
   ptd_cross_thread_operation_block();
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   // DBGMSG("ptd->description = %s, addl_descripton = %s", ptd->description, addl_description);
   if (!ptd->description)
      ptd->description = g_strdup(addl_description);
   else if (str_contains(ptd->description, addl_description) < 0) {
      char * s = ptd->description;
      ptd->description = g_strdup_printf("%s; %s", ptd->description, addl_description);
      free(s);
   }
}


const char * ptd_get_thread_description_t() {
   static GPrivate  x_key = G_PRIVATE_INIT(g_free);
   static GPrivate  x_len_key = G_PRIVATE_INIT(g_free);

   ptd_cross_thread_operation_block();
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   char * buf = NULL;
   if (ptd->description) {
      char * buf = get_thread_dynamic_buffer(&x_key, &x_len_key, strlen(ptd->description)+1);
      strcpy(buf,ptd->description);
   }
   return buf;
}
#endif


/** Output a debug report of a #Per_Thread_Data struct.
 *
 *  @param  data   pointer to #Per_Thread_Data struct
 *  @param  depth  logical indentation level
 *
 *  // relies on caller for possible blocking
 */
void dbgrpt_per_thread_data(Per_Thread_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Per_Thread_Data", data, depth);
   rpt_vstring(d1,"initialized                %s", sbool(data->initialized));
   rpt_vstring(d1,"thread_id                  %d", data->thread_id);
#ifdef REMOVED_20
   rpt_vstring(d1,"description                %s", data->description);
#endif
   rpt_vstring(d1,"cur_dh:                    %s", dh_repr(data->cur_dh) );
   rpt_vstring(d1,"cur_func                   %s", data->cur_func);
   rpt_vstring(d1,"cur_start                  %"PRIu64, data->cur_start);
   rpt_vstring(d1,"function profile stats: ");
   ptd_profile_function_report(data, GINT_TO_POINTER(depth+1));
}


/** Applies a specified function with signature GFunc to all
 *  #Per_Thread_Data instances.
 *
 *  @param  func  function to apply
 *  \parm   arg   an arbitrary argument passed as a pointer
 *
 *  This is a multi-instance operation.
 */
void ptd_apply_all(Ptd_Func func, void * arg) {
   ptd_cross_thread_operation_start();
   bool debug = false;
   assert(per_thread_data_hash);    // allocated by init_thread_data_module()

      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,per_thread_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Thread_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         func(data, arg);
      }

   ptd_cross_thread_operation_end();
}


/** Apply a given function to all #Per_Thread_Data structs, ordered by thread id.
 *  Note that this report includes structs for threads that have been closed.
 *
 *  @param func function to apply
 *  @param arg pointer or integer value
 *
 *  This is a multi-instance operation.
 */
void ptd_apply_all_sorted(Ptd_Func func, void * arg) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   ptd_cross_thread_operation_start();
   assert(per_thread_data_hash);

   DBGMSF(debug, "hash table size = %d", g_hash_table_size(per_thread_data_hash));
   GList * keys = g_hash_table_get_keys (per_thread_data_hash);
   GList * new_head = g_list_sort(keys, gaux_ptr_intcomp);
   GList * l;
   for (l = new_head; l != NULL; l = l->next) {
      int key = GPOINTER_TO_INT(l->data);
      DBGMSF(debug, "Key: %d", key);
      Per_Thread_Data * data = g_hash_table_lookup(per_thread_data_hash, l->data);
      assert(data);

      func(data, arg);
   }
   g_list_free(new_head);   // would keys also work?

   ptd_cross_thread_operation_end();
   DBGMSF(debug, "Done");
}


/** Emits a brief summary of a #Per_Thread_Data instance,
 *  showing the thread id number and description.
 *
 *  ptd   pointer to #Per_Thread_Data instance
 *  arg   logical indentation
 *
 *  @note
 *  This function has a GFunc signature
 *  @note
 *  Called only by multi-thread-data functions that hold lock
 */
void ptd_thread_summary(Per_Thread_Data * ptd, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   int d1 = depth+1;
   ptd_cross_thread_operation_block();

   // simple but ugly
   // rpt_vstring(depth, "Thread: %d. Description:%s",
   //             ptd->thread_id, ptd->description);

   // rpt_vstring(depth, "Thread: %d", ptd->thread_id);
   // char * header = "Description: ";

   char header[100];
   g_snprintf(header, 100, "Thread %d: ", ptd->thread_id);

   rpt_vstring(d1, "%s", header);
#ifdef REMOVED_20
   int hdrlen = strlen(header);
   if (!ptd->description)
      rpt_vstring(d1, "%s", header);
   else {
      Null_Terminated_String_Array pieces =
            strsplit_maxlength(ptd->description, 60, " ");
      int ntsa_ndx = 0;
      while (true) {
         char * s = pieces[ntsa_ndx++];
         if (!s)
            break;
         rpt_vstring(d1, "%-*s%s", hdrlen, header, s);
         if (strlen(header) > 0) {
            // *header = '\0';    // better, test it next time working with this function
            strcpy(header, "");
            // header = "";
         }
      }
      ntsa_free(pieces, true);
   }
#endif
}

/** Emits a brief summary (thread id and description) for each
 * #Per_Thread_Data instance.
 *
 *  @param  depth   logical indentation depth
 */
void ptd_list_threads(int depth) {
   // bool this_function_owns_lock = ptd_lock_if_unlocked();

   int d1 = depth +1;
   // rpt_label(depth, "Have data for threads:");
   rpt_label(depth, "Report has per-thread data for threads:");
   ptd_apply_all_sorted(ptd_thread_summary, GINT_TO_POINTER(d1));
   rpt_nl();
}

#ifdef PTD
void report_all_thread_status_counts(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   rpt_label(depth, "No per-thread status code statistics are collected");
   rpt_nl();
   DBGMSF(debug, "Done");
}
#endif


#ifdef TIMING_TEST
// test timing for getting thread id
// conclusion:
//   at   10 calls using cached value is   2 times as fast
//   at  100 calls using cached value is  11 times as fast
//   at 1000 calls using cached value is  24 times as fast
void test_get_thread_id() {
   // int loopct = 10 * 1000 * 1000; // 10 million
   int loopct = 1000;
   intmax_t threadid;
   intmax_t start = cur_realtime_nanosec();
   for (int ndx = 0; ndx < loopct; ndx++) {
      threadid = get_thread_id();
   }
   intmax_t end = cur_realtime_nanosec();
   intmax_t elapsed = (end-start)/1000;
   printf("threadid = %ld,  direct:         %10"PRIu64"\n", threadid, elapsed);

   start = cur_realtime_nanosec();
   Thread_Output_Settings* ts = get_thread_settings();
   ts->tid = get_thread_id();
   for (int ndx = 0; ndx < loopct; ndx++) {
      Thread_Output_Settings* ts = get_thread_settings();
      threadid = ts->tid;
   }
   end = cur_realtime_nanosec();
   intmax_t elapsed2 = (end-start)/1000;
   printf("threadid = %ld,  thread_setings: %10"PRIu64"\n", threadid, elapsed2);
   int ratio = elapsed/elapsed2;
   printf("ratio: %d\n", ratio);
}
#endif


//
// Collect Performance Stats
//

bool ptd_api_profiling_enabled = false;


void ptd_profile_function_stats_key_destroy(void * data) {   // GDestroyNotify
   // key is function name, type char*
   g_free(data);
}


void ptd_profile_function_stats_value_destroy(void * data) {  // GDestroyNofify
   // value is Per_Thread_Function_Stats*
   Per_Thread_Function_Stats * stats = (Per_Thread_Function_Stats *) data;
   free(stats->function);
   free(stats);
}


void ptd_profile_reset_thread_stats(Per_Thread_Data * ptd, void * data) {
   bool debug = false;
   DBGMSF(debug, "Starting. ptd=%p, data=%p", ptd, data);
   ptd_cross_thread_operation_block();    // wait for any cross-thread operation to complete
   if (ptd->function_stats)
      g_hash_table_remove_all(ptd->function_stats);
   DBGMSF(debug, "Done");
}


void ptd_profile_reset_all_stats()  {
   ptd_cross_thread_operation_start();
   ptd_apply_all(ptd_profile_reset_thread_stats, NULL);
   ptd_cross_thread_operation_end();
}


// gets the Function_Stats_Hash table for the current thread
static inline Function_Stats_Hash * ptd_profile_get_stats() {
   // does this function need to block?
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   if (!ptd->function_stats) {
      ptd->function_stats = g_hash_table_new_full(g_str_hash, g_str_equal,
            ptd_profile_function_stats_key_destroy, ptd_profile_function_stats_value_destroy);
   }
   return ptd->function_stats;
}


void ptd_profile_function_start(const char * func) {
   // bool debug = false;
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   if (!ptd->cur_func) {
      ptd->cur_func = strdup(func);
      ptd->cur_start = cur_realtime_nanosec();
   }
}


void ptd_profile_function_end(const char * func) {
   bool debug = false;
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   DBGF(debug, "Starting. func=%s, cur_func=%s", func, ptd->cur_func);
   if (streq(ptd->cur_func, func)) {
      Function_Stats_Hash * stats_table = ptd_profile_get_stats();
      Per_Thread_Function_Stats * function_stats = g_hash_table_lookup(stats_table, func);
      DBGF(debug, "       stats_table=%p, function_stats=%p", stats_table, function_stats);
      if (!function_stats) {
         function_stats = calloc(1, sizeof(Per_Thread_Function_Stats));
         function_stats->function = strdup(func);
         g_hash_table_insert(stats_table, strdup(func), function_stats);
      }
      function_stats->total_calls++;
      function_stats->total_nanosec = (cur_realtime_nanosec() - ptd->cur_start);
      free(ptd->cur_func);
      ptd->cur_func = NULL;
   }
}


//
// Stats Utility Functions
//
#ifdef NOT_NEEDED
const char * func_addr_to_name(gpointer addr) {
#define ALT
#ifdef ALT
   const char * name = "Unknown";
   Dl_info info;
   int rc = dladdr(addr, &info);
   if (rc) {
      name = info.dli_sname;
   }
#else
   char * name =  rtti_get_func_name_by_addr(addr);
   if (!name)
      name = "Unknown";
#endif
   return name;
}
#endif


//
// Create summary table
//

/** Adds the stats for one function on a thread to the summary record for all
 *  threads for that function.
 *
 *  Satisfies type GHFunc
 *
 *  @oaram  key   function name (char*)
 *  @param  value pointer to Per_Thread_Function_Stats struct for the function on a thread
 *  @data   pointer to summary hash table of function names -> Per_Thread_Function_Stats struct
 */
void add_one_func_to_summary(gpointer key, gpointer cur_func_stats, gpointer data) {
   bool debug = false;
   DBGMSF(debug, "Starting. key=%p -> %s, cur_func_stats=%p, data=%p", key, key, cur_func_stats, data);
   Per_Thread_Function_Stats * cfs = (Per_Thread_Function_Stats*) cur_func_stats;
   assert(streq(key,cfs->function));
   Function_Stats_Hash * summary_table = (GHashTable *) data;
   Per_Thread_Function_Stats * cur_summary_entry =
         g_hash_table_lookup(summary_table, cfs->function);
   if (!cur_summary_entry) {
      DBGMSF(debug, "      Per_Thread_Function_Stats not found for %s", cfs->function);
      cur_summary_entry = calloc(1, sizeof(Per_Thread_Function_Stats));
      cur_summary_entry->function = strdup(cfs->function);
      g_hash_table_insert(summary_table, strdup(cfs->function), cur_summary_entry);
   }
   cur_summary_entry->total_calls += cfs->total_calls;
   cur_summary_entry->total_nanosec += cfs->total_nanosec;
   DBGMSF(debug, "Done.   cur_summary_entry=%p, total_calls=%d, total_nanosec=%d, function=%p->%s",
         cur_summary_entry, cur_summary_entry->total_calls, cur_summary_entry->total_nanosec,
         cur_summary_entry->function, cur_summary_entry->function);
}


/** Adds the stats for all functions on single thread to the summary record
 *  for all functions on all threads
 *
 *  @param ptd  pointer to Per_Thread_Data struct for a thread
 *  @param data pointer to summary
 */
void ptd_add_stats(Per_Thread_Data * ptd, void * data) {
   bool debug = false;
   DBGMSF(debug, "Starting. ptd=%p, data=%p", ptd, data);
   g_hash_table_foreach(ptd->function_stats, add_one_func_to_summary, data);
   DBGMSF(debug, "Done");
}


/** Creates a hash table with the total stats for each function across all threads
 *
 *  @return hash table with key = function name, value = Per_Thread_Function_Stats*
 */
Function_Stats_Hash * summarize_per_thread_stats() {
   Function_Stats_Hash * summary = g_hash_table_new(g_str_hash, g_str_equal);
   ptd_apply_all(ptd_add_stats, summary);   // ptd_apply_all manages locking
   return summary;
}


/** Reports stats for one function on one thread, or reports the
 *  summary record for one function on all threads
 *
 *  Satisfies typedef GHFunc
 *
 *  @oaram  key   function name (char*)
 *  @param  value pointer to Per_Thread_Function_Stats struct for the function
 *  @data   depth logical indentation depth
 */
static void ptd_report_one_func0(Per_Thread_Function_Stats * pts, void * arg) {
   // bool debug = false;
   int depth = GPOINTER_TO_INT(arg);
   // DBGMSF(debug, "pts=%p, pts->total_calls=%d", pts, pts->total_calls);
   // DBGMSF(debug, "pts=%p, pts->total_millisec=%d", pts, pts->total_millisec);
   // DBGMSF(debug, "pts=%p, pts->function=%p", pts, pts->function);
   rpt_vstring(depth, "%5d  %8"PRIu64"  %s",
         pts->total_calls, (pts->total_nanosec+500)/1000, pts->function);
}

static void ptd_report_one_func(gpointer key, gpointer value, gpointer user_data) {
   bool debug = false;
   DBGMSF(debug, "gpointer=%p->%s, value=%p, user_data=%p", key, key, value, user_data);
   Per_Thread_Function_Stats * pts = (Per_Thread_Function_Stats *) value;
   //const char * func_name = func_addr_to_name(pts->function);
   // DBGMSF(debug, "pts=%p, pts->total_calls=%d", pts, pts->total_calls);
   // DBGMSF(debug, "pts=%p, pts->total_millisec=%d", pts, pts->total_millisec);
   // DBGMSF(debug, "pts=%p, pts->function=%p", pts, pts->function);
   ptd_report_one_func0(pts, user_data);
}


// Reports function stats for a single thread
void ptd_profile_function_report(Per_Thread_Data * ptd, gpointer depth) {
   int d0 = GPOINTER_TO_INT(depth);
   // int d1 = d0 + 1;
   rpt_vstring(d0, "Per-Thread Function Profile Report for thread %d:", ptd->thread_id);
   if (ptd->function_stats) {
      rpt_label(d0, "Count  Microsec  Function Name");
      g_hash_table_foreach(ptd->function_stats, ptd_report_one_func, depth);
   }
   else
      rpt_label(d0, "No function stats");
   rpt_nl();
}


// Apply a function to all Per_Thread_Data records
// typedef void (*Ptd_Func)(Per_Thread_Data * data, void * arg);   // Template for function to apply

void ptd_profile_report_all_threads(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   ptd_apply_all_sorted(ptd_profile_function_report, GINT_TO_POINTER(depth));
   // rpt_nl();
   DBGMSF(debug, "Done");
}


gint
gaux_scomp(

      gconstpointer a,
      gconstpointer b)
{
   // DBGMSG("a=%p -> %s", a,a);
   // DBGMSG("b=%p -> %s", b,b);
   return g_ascii_strcasecmp(a,b);
}



typedef void (*Ptd_Stats_Func)(Per_Thread_Function_Stats * data, void * arg);   // Template for function to apply

/** Apply a given function to all #Per_Thread_Data structs, ordered by thread id.
 *  Note that this report includes structs for threads that have been closed.
 *
 *  @param func function to apply
 *  @param arg pointer or integer value
 *
 *  This is a multi-instance operation.
 */
void ptd_profile_apply_all_sorted(Function_Stats_Hash * function_stats_hash,
                            Ptd_Stats_Func func, void * arg) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(function_stats_hash);

   DBGMSF(debug, "hash table size = %d", g_hash_table_size(function_stats_hash));
   GList * keys = g_hash_table_get_keys (function_stats_hash);
   GList * new_head = g_list_sort(keys, gaux_scomp);
   GList * l;
   for (l = new_head; l != NULL; l = l->next) {
      char * key = (char*) l->data;
      DBGMSF(debug, "Key: %s", key);
      Per_Thread_Function_Stats * data = g_hash_table_lookup(function_stats_hash, l->data);
      assert(data);
      func(data, arg);
   }
   g_list_free(new_head);   // would keys also work?

   DBGMSF(debug, "Done");
}


void ptd_profile_report_stats_summary(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   // int d1 = depth+1;
   rpt_label(depth, "Summary Function Profile Report for all Threads");
   rpt_label(depth, "Count  Microsec  Function Name");
   Function_Stats_Hash * summary_stats = summarize_per_thread_stats();
   DBGMSF(debug, "    summary_stats=%p", summary_stats);
   // g_hash_table_foreach(summary_stats, ptd_report_one_func, GINT_TO_POINTER(depth));
   ptd_profile_apply_all_sorted(summary_stats, ptd_report_one_func0, GINT_TO_POINTER(depth));
   DBGMSF(debug, "Done");
}


/** Initialize per_thread_data.c at program startup */
void init_per_thread_data() {
   per_thread_data_hash = g_hash_table_new_full(g_direct_hash, NULL, NULL, per_thread_data_destroy);
   // DBGMSG("per_thead_data_hash = %p", per_thread_data_hash);
   // test_get_thread_id();
}
