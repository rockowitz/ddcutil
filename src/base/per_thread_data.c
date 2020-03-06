// per_thread_data.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "per_thread_data.h"

// Master table of sleep data for all threads
GHashTable *  per_thread_data_hash = NULL;
GMutex        per_thread_data_mutex;

// Do not call while already holding lock.  Behavior undefined
void ptd_lock_all_thread_data() {
   g_mutex_lock(&per_thread_data_mutex);
}

// Do not call if not holding lock.  Behavior undefined.
void ptd_unlock_all_thread_data() {
   g_mutex_unlock(&per_thread_data_mutex);
}


void init_per_thread_data(Per_Thread_Data * ptd) {
   // just a placeholder
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
   bool debug = false;
   pid_t cur_thread_id = syscall(SYS_gettid);
   // DBGMSF(debug, "Getting thread sleep data for thread %d", cur_thread_id);
   g_mutex_lock(&per_thread_data_mutex);
   if (!per_thread_data_hash) {
      per_thread_data_hash = g_hash_table_new(g_direct_hash, NULL);
   }
   Per_Thread_Data * data = g_hash_table_lookup(per_thread_data_hash,
                                            GINT_TO_POINTER(cur_thread_id));
   if (!data) {
      DBGMSF(debug, "==> Per_Thread_Data not found for thread %d", cur_thread_id);
      data = g_new0(Per_Thread_Data, 1);
      data->thread_id = cur_thread_id;
      init_per_thread_data(data);
      DBGMSF(debug, "Initialized: %s. thread_sleep_data_defined: %s. thread_retry_data_defined; %s",
           sbool(data->initialized),
           sbool( data->thread_sleep_data_defined), sbool( data->thread_retry_data_defined));

      g_hash_table_insert(per_thread_data_hash,
                          GINT_TO_POINTER(cur_thread_id),
                          data);
      DBGMSF(debug, "Created Per_Thread_Data struct for thread id = %d", data->thread_id);
      if (debug)
        dbgrpt_per_thread_data(data, 1);
   }
   g_mutex_unlock(&per_thread_data_mutex);
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
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   // DBGMSG("thread: %d, description: %s", ptd->thread_id, description);
   if (ptd->description)
      free(ptd->description);
   ptd->description = strdup(description);
   // dbgrpt_per_thread_data(ptd, 4);
}

void ptd_append_thread_description(const char * addl_description) {
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   if (ptd->description)
      ptd->description = g_strdup_printf("%s; %s", ptd->description, addl_description);
   else
      ptd->description = strdup(addl_description);
}


const char * ptd_get_thread_description() {
   Per_Thread_Data *  ptd = ptd_get_per_thread_data();
   return ptd->description;
}




/** Output a debug report of a #Per_Thread_Data struct.
 *
 *  \param  data   pointer to #Per_Thread_Data struct
 *  \param  depth  logical indentation level
 */
void dbgrpt_per_thread_data(Per_Thread_Data * data, int depth) {
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


void ptd_apply_all(Ptd_Func func, void * arg) {
   bool debug = false;
   if (per_thread_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,per_thread_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Thread_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->thread_id);
         func(data, arg);
      }
   }
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
   bool debug = false;
   DBGMSF(debug, "Starting");
   if (per_thread_data_hash) {
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
   }
   DBGMSF(debug, "Done");
}



