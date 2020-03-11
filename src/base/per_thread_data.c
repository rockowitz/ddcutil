// per_thread_data.c

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <sys/types.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/thread_retry_data.h"    // temp circular

#include "thread_sleep_data.h"
#include "thread_retry_data.h"
#include "per_thread_data.h"

// Master table of sleep data for all threads
GHashTable *    per_thread_data_hash = NULL;
GMutex          per_thread_data_mutex;
static GPrivate this_thread_has_lock;
static GPrivate lock_depth; // GINT_TO_POINTER(0);
static bool     debug_mutex = false;
       int      ptd_lock_count = 0;
       int      ptd_unlock_count = 0;
       int cross_thread_operation_blocked_count = 0;

void dbgrpt_per_thread_data_locks(int depth) {
   rpt_vstring(depth, "ptd_lock_count:                        %-4d", ptd_lock_count);
   rpt_vstring(depth, "ptd_unlock_count:                      %-4d", ptd_unlock_count);
   rpt_vstring(depth, " cross_thread_operation_blocked_count: %-4d", cross_thread_operation_blocked_count);
}

static bool    cross_thread_operation_active = false;
static GMutex  cross_thread_operation_mutex;
static pid_t   cross_thread_operation_owner;

bool cross_thread_operation_start() {
   // Only 1 cross thread action can be active at one time,
   // and all per_thread actions should wait

   // TODO copy and modify ptd_lock_if_unlocked,
   // do not want to acquire lock if the thread already has it

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

          // unnecessary - already in mutex
         // __sync_fetch_and_add(&ptd_lock_count, 1);
      ptd_lock_count++;

      // should this be a depth counter rather than a boolean?
      g_private_set(&this_thread_has_lock, GINT_TO_POINTER(true));


      pid_t cur_thread_id = syscall(SYS_gettid);
      cross_thread_operation_owner = cur_thread_id;
      DBGMSF(debug, "Locked by thread %d", cur_thread_id);
      sleep_millis(10);   // give all per-thread functions time to finish
   }
   g_private_set(&lock_depth, GINT_TO_POINTER(thread_lock_depth+1));
   DBGMSF(debug, "Returning: %s", sbool(lock_performed) );
   return lock_performed;
}


void cross_thread_operation_end() {
   int thread_lock_depth = GPOINTER_TO_INT(g_private_get(&lock_depth));
   g_private_set(&lock_depth, GINT_TO_POINTER(thread_lock_depth-1));

   if (thread_lock_depth == 1) {
   // if (unlock_requested) {
      cross_thread_operation_active = false;
      cross_thread_operation_owner = 0;
      g_private_set(&this_thread_has_lock, false);
      ptd_unlock_count++;
      g_mutex_unlock(&cross_thread_operation_mutex);
   }
}


void cross_thread_operation_block() {
   if (cross_thread_operation_active && syscall(SYS_gettid) != cross_thread_operation_owner) {
      __sync_fetch_and_add(&cross_thread_operation_blocked_count, 1);
      do {
         sleep_millis(10);
      } while (cross_thread_operation_active);
   }
}


//
// Locking
//

#ifdef OLD

/** If **per_thread_data_mutex** is not already locked by the current thread,
 *  lock it.
 *
 *  \remark
 *  This function is necessary because if a GLib mutex is
 *  re-locked by the current thread then the behavior is undefined.
 */
bool ptd_lock_if_unlocked() {
   bool debug = false;
   debug = debug || debug_mutex;
   bool lock_performed = false;
   bool thread_has_lock = GPOINTER_TO_INT(g_private_get(&this_thread_has_lock));
   DBGMSF(debug, "Already locked: %s", sbool(thread_has_lock));
   if (!thread_has_lock) {
      g_mutex_lock(&per_thread_data_mutex);
      lock_performed = true;
      // should this be a depth counter rather than a boolean?
      __sync_fetch_and_add(&ptd_lock_count, 1);
      // ptd_lock_count++;
      g_private_set(&this_thread_has_lock, GINT_TO_POINTER(true));
      if (debug) {
         pid_t cur_thread_id = syscall(SYS_gettid);
         DBGMSG("Locked by thread %d", cur_thread_id);
      }
   }
   DBGMSF(debug, "Returning: %s", sbool(lock_performed) );
   return lock_performed;
}


/** Unlocks the **per_thread_data_mutex** set by a call to #lock_if_unlocked
 *
 *  \param  unlock_requested perform unlock
 */
void ptd_unlock_if_needed(bool unlock_requested) {
   bool debug = false;
   debug = debug || debug_mutex;
   DBGMSF(debug, "unlock_requested=%s", sbool(unlock_requested));

   if (unlock_requested) {
      // is it actually locked?
      bool currently_locked = GPOINTER_TO_INT(g_private_get(&this_thread_has_lock));
      DBGMSF(debug, "currently_locked = %s", sbool(currently_locked));
      if (currently_locked) {
         g_private_set(&this_thread_has_lock, GINT_TO_POINTER(false));
         ptd_unlock_count++;
         if (debug) {
            pid_t cur_thread_id = syscall(SYS_gettid);
            DBGMSF(debug, "Unlocked by thread %d", cur_thread_id);
         }
         g_mutex_unlock(&per_thread_data_mutex);
      }
   }
}


/** Requests a lock on the per-thread data structure.
 *  A lock is not performed if the current thread already holds the lock
 *
 *  \return  true if a lock was actually performed
 */
bool ptd_lock() {
   return ptd_lock_if_unlocked();
}


/** Requests that the currently held lock on the **try_data** data structure
 *  be released
 *
 *  \param release_requested  if true, attempt to unlock
 */
void ptd_unlock(bool release_requested) {
   ptd_unlock_if_needed(release_requested);
}
#endif

static void init_per_thread_data(Per_Thread_Data * ptd) {
   init_thread_sleep_data(ptd);
   init_thread_retry_data(ptd);
}


/** Gets the #Per_Thread_Data struct for the current thread, using the
 *  current thread's id number. If the struct does not already exist, it
 *  is allocated and initialized.
 *
 *  \return pointer to #Per_Thread_Data struct
 *
 *  \remark
 *  The structs are maintained centrally rather than using a thread-local pointer
 *  to a block on the heap because the of a problems when the thread is closed.
 *  Valgrind complains of access errors for closed threads, even though the
 *  struct is on the heap and still readable.
 */
Per_Thread_Data * ptd_get_per_thread_data() {
   bool debug = true;
   pid_t cur_thread_id = syscall(SYS_gettid);
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
      g_private_set(&lock_depth, GINT_TO_POINTER(0));
      init_per_thread_data(data);
      DBGMSF(debug, "Initialized: %s. thread_sleep_data_defined: %s. thread_retry_data_defined; %s",
           sbool(data->initialized),
           sbool( data->thread_sleep_data_defined), sbool( data->thread_retry_data_defined));

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


#ifdef UNUSED
void ptd_register_thread_dref(Display_Ref * dref) {
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   DBGMSG("thread: %d, display ref: %s", ptd->thread_id, dref_repr_t(dref));
   ptd->dref = dref;
   dbgrpt_per_thread_data(ptd, 4);
}
#endif


void ptd_set_thread_description(const char * description) {
   cross_thread_operation_block();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();

   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   // DBGMSG("thread: %d, description: %s", ptd->thread_id, description);
   if (ptd->description)
      free(ptd->description);
   ptd->description = strdup(description);
   // dbgrpt_per_thread_data(ptd, 4);

   // ptd_unlock_if_needed(this_function_owns_lock);
}


void ptd_append_thread_description(const char * addl_description) {
   cross_thread_operation_block();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();

   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   if (ptd->description)
      ptd->description = g_strdup_printf("%s; %s", ptd->description, addl_description);
   else
      ptd->description = strdup(addl_description);

   // ptd_unlock_if_needed(this_function_owns_lock);
}


const char * ptd_get_thread_description_t() {
   static GPrivate  x_key = G_PRIVATE_INIT(g_free);
   static GPrivate  x_len_key = G_PRIVATE_INIT(g_free);

   cross_thread_operation_block();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   // ptd_unlock_if_needed(this_function_owns_lock);
   char * buf = get_thread_dynamic_buffer(&x_key, &x_len_key, strlen(ptd->description)+1);
   strcpy(buf,ptd->description);
   return buf;
}


char * int_array_to_string(uint16_t * start, int ct) {
   int bufsz = ct*10;
   char * buf = calloc(1, bufsz);
   int next = 0;
   for (int ctr =0; ctr < ct && next < bufsz; ctr++) {
      g_snprintf(buf+next, bufsz-next,"%s%d", (next > 0) ? ", " : "", *(start+ctr) );
      next = strlen(buf);
   }
   // DBGMSG("start=%p, ct=%d, returning %s", start, ct, buf);
   return buf;
}


/** Output a debug report of a #Per_Thread_Data struct.
 *
 *  \param  data   pointer to #Per_Thread_Data struct
 *  \param  depth  logical indentation level
 */
void dbgrpt_per_thread_data(Per_Thread_Data * data, int depth) {
   cross_thread_operation_block();  // or cross_thread_operation_start()?
   // bool this_function_owns_lock = ptd_lock_if_unlocked();

   int d1 = depth+1;
   rpt_structure_loc("Per_Thread_Data", data, depth);
 //rpt_int( "sizeof(Per_Thread_Data)",  NULL, sizeof(Per_Thread_Data),   d1);
   rpt_int( "thread_id",                  NULL, data->thread_id,             d1);
   rpt_bool("initialized",                NULL, data->initialized,           d1);
//    rpt_vstring(d1,"dref:                  %s", dref_repr_t(data->dref) );
   rpt_vstring(d1,"description                %s", data->description);
   rpt_bool("dynamic_sleep_enabled",      NULL, data->dynamic_sleep_enabled, d1);

   rpt_bool("sleep data initialized" ,    NULL, data->thread_sleep_data_defined, d1);

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

   rpt_bool("retry data initialized"    , NULL, data->thread_retry_data_defined, d1);

   rpt_vstring(d1, "Highest maxtries:                  %d,%d,%d,%d",
                    data->highest_maxtries[0], data->highest_maxtries[1],
                    data->highest_maxtries[2], data->highest_maxtries[3]);
   rpt_vstring(d1, "Current maxtries:                  %d,%d,%d,%d",
                    data->current_maxtries[0], data->current_maxtries[1],
                    data->current_maxtries[2], data->current_maxtries[3]);
   rpt_vstring(d1, "Lowest maxtries:                   %d,%d,%d,%d",
                    data->lowest_maxtries[0], data->lowest_maxtries[1],
                    data->lowest_maxtries[2], data->lowest_maxtries[3]);

   for (int retry_type = 0; retry_type < 4; retry_type++) {
      int upper_bound = data->highest_maxtries[retry_type] + 1;
      assert(upper_bound <= MAX_MAX_TRIES + 1);
      char * buf = int_array_to_string( data->try_stats[retry_type].counters, upper_bound);
      rpt_vstring(d1, "try_stats[%d=%-27s].counters = %s",
                      retry_type, retry_type_name(retry_type), buf);
      free(buf);
   }
   // ptd_unlock_if_needed(this_function_owns_lock);
}


void ptd_apply_all(Ptd_Func func, void * arg) {
   cross_thread_operation_start();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();
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

   cross_thread_operation_end();
   // ptd_unlock_if_needed(this_function_owns_lock);
}


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


/** Report all #Per_Thread_Data structs.  Note that this report includes
 *  structs for threads that have been closed.
 *
 *  \param depth  logical indentation depth
 */
void ptd_apply_all_sorted(Ptd_Func func, void * arg) {
   bool debug = true;
   DBGMSF(debug, "Starting");
   cross_thread_operation_start();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();
   assert(per_thread_data_hash);

      DBGMSF(debug, "hash table size = %d", g_hash_table_size(per_thread_data_hash));
      GList * keys = g_hash_table_get_keys (per_thread_data_hash);
      GList * new_head = g_list_sort(keys, compare_int_list_entries); // not working
      GList * l;
      for (l = new_head; l != NULL; l = l->next) {
         int key = GPOINTER_TO_INT(l->data);
         DBGMSF(debug, "Key: %d", key);
         Per_Thread_Data * data = g_hash_table_lookup(per_thread_data_hash, l->data);
         assert(data);

         func(data, arg);
      }
      g_list_free(new_head);   // would keys also work?

   cross_thread_operation_end();
   // ptd_unlock_if_needed(this_function_owns_lock);
   DBGMSF(debug, "Done");
}


void ptd_thread_summary(Per_Thread_Data * ptd, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   int d1 = depth+1;
   cross_thread_operation_block();
   // bool this_function_owns_lock = ptd_lock_if_unlocked();

   // rpt_vstring(depth, "Thread: %d. Description:%s",
   //             ptd->thread_id, ptd->description);

   rpt_vstring(depth, "Thread: %d", ptd->thread_id);
   char * header = "Description: ";
   int hdrlen = strlen(header);
   if (!ptd->description)
      rpt_vstring(d1, "%s", header);
   else {
      Null_Terminated_String_Array pieces =
            strsplit_maxlength(ptd->description, 70, " ");
      int ntsa_ndx = 0;
      while (true) {
         char * s = pieces[ntsa_ndx++];
         if (!s)
            break;
         rpt_vstring(d1, "%-*s%s", hdrlen, header, s);
         // printf("(%s) s = %p\n", __func__, s);
         if (strlen(header) > 0)
            header = "";
      }
   }

   // ptd_unlock_if_needed(this_function_owns_lock);
}


void ptd_list_threads(int depth) {
   // bool this_function_owns_lock = ptd_lock_if_unlocked();

   int d1 = depth +1;
   // rpt_label(depth, "Have data for threads:");
   rpt_label(depth, "Report has per-thread data for threads:");
   ptd_apply_all_sorted(ptd_thread_summary, GINT_TO_POINTER(d1));

}

// nothing to change
// void per_thread_data_reset() {
// }



/** Initialize per-thread data at program startup */
void init_thread_data_module() {
   per_thread_data_hash = g_hash_table_new(g_direct_hash, NULL);
   DBGMSG("per_thead_data_hash = %p", per_thread_data_hash);
}


