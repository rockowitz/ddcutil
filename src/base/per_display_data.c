/** @file per_display_data.c
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <sys/types.h>

#include "util/debug_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/display_retry_data.h"    // temp circular
#include "base/display_sleep_data.h"

#include "base/per_display_data.h"

// Master table of sleep data for all threads
GHashTable *    per_display_data_hash = NULL;
static GPrivate this_display_has_lock;
static GPrivate lock_depth; // GINT_TO_POINTER(0);
static bool     debug_mutex = false;
       int      pdd_lock_count = 0;
       int      pdd_unlock_count = 0;
       int      cross_display_operation_blocked_count = 0;

void dbgrpt_per_display_data_locks(int depth) {
   rpt_vstring(depth, "pdd_lock_count:                        %-4d", pdd_lock_count);
   rpt_vstring(depth, "pdd_unlock_count:                      %-4d", pdd_unlock_count);
   rpt_vstring(depth, "cross_display_operation_blocked_count:  %-4d", cross_display_operation_blocked_count);
}

static bool    cross_display_operation_active = false;
static GMutex  cross_display_operation_mutex;
static pid_t   cross_display_operation_owner;

// The locking strategy relies on the fact that in practice conflicts
// will be rare, and critical sections short.
// Operations that occur on the
// are blocked only using a spin-lock.

// The groups of operations:
// - Operations that operate on the single Per_Display_Data instance
//   associated with the currently executing thread.
// - Operations that operate on a single Per_Display_Data instance,
//   but possibly not from the thread associated with the Per_Display_Data instance.
// - Operations that operate on multiple Per_Display_Data instances.
//   These are referred to as cross thread operations.
//   Alt, perhaps clearer, refer to them as multi-thread data instances.

/**
 */
bool pdd_cross_display_operation_start() {
   // Only 1 cross display action can be active at one time.
   // All per_display actions must wait

   bool debug = false;
   debug = debug || debug_mutex;

   bool lock_performed = false;

   // which way is better?
   bool display_has_lock  = GPOINTER_TO_INT(g_private_get(&this_display_has_lock));
   int display_lock_depth = GPOINTER_TO_INT(g_private_get(&lock_depth));
   assert ( ( display_has_lock && display_lock_depth  > 0) ||
            (!display_has_lock && display_lock_depth == 0) );
   DBGMSF(debug, "Already locked: %s", sbool(display_has_lock));

   if (display_lock_depth == 0) {    // (A)
   // if (!display_has_lock) {
      // display_lock_depth is per-display, so must be unchanged from (A)
      g_mutex_lock(&cross_display_operation_mutex);
      lock_performed = true;
      cross_display_operation_active = true;

      pdd_lock_count++;

      // should this be a depth counter rather than a boolean?
      g_private_set(&this_display_has_lock, GINT_TO_POINTER(true));

      Thread_Output_Settings * thread_settings = get_thread_settings();
      // intmax_t cur_display_id = get_display_id();
      intmax_t cur_display_id = thread_settings->tid;
      cross_display_operation_owner = cur_display_id;
      DBGMSF(debug, "Locked by thread %d", cur_display_id);
      sleep_millis(10);   // give all per-thread functions time to finish
   }
   g_private_set(&lock_depth, GINT_TO_POINTER(display_lock_depth+1));
   DBGMSF(debug, "Returning: %s", sbool(lock_performed) );
   return lock_performed;
}


void pdd_cross_display_operation_end() {
   int display_lock_depth = GPOINTER_TO_INT(g_private_get(&lock_depth));
   g_private_set(&lock_depth, GINT_TO_POINTER(display_lock_depth-1));
   assert(display_lock_depth >= 1);

   if (display_lock_depth == 1) {
   // if (unlock_requested) {
      cross_display_operation_active = false;
      cross_display_operation_owner = 0;
      g_private_set(&this_display_has_lock, false);
      pdd_unlock_count++;
      assert(pdd_lock_count == pdd_unlock_count);
      g_mutex_unlock(&cross_display_operation_mutex);
   }
   else {
      assert( pdd_lock_count > pdd_unlock_count );
   }
}


/** Block execution of single Per_Display_Data operations when an operation
 *  involving multiple Per_Thead_Data instances is active.
 */

void pdd_cross_display_operation_block() {
   // intmax_t cur_displayid = get_display_id();
   Thread_Output_Settings * thread_settings = get_thread_settings();
   intmax_t cur_displayid = thread_settings->tid;
   if (cross_display_operation_active && cur_displayid != cross_display_operation_owner) {
      __sync_fetch_and_add(&cross_display_operation_blocked_count, 1);
      do {
         sleep_millis(10);
      } while (cross_display_operation_active);
   }
}


// GDestroyNotify: void (*GDestroyNotify) (gpointer data);

void per_display_data_destroy(void * data) {
   if (data) {
      Per_Display_Data * pdd = data;
      free(pdd->description);
      free(pdd);
   }
}

/** Initialize per_display_data.c at program startup */
void init_display_data_module() {
   per_display_data_hash = g_hash_table_new_full(g_direct_hash, NULL, NULL, per_display_data_destroy);
   // DBGMSG("per_thead_data_hash = %p", per_display_data_hash);
}

void release_display_data_module() {
   if (per_display_data_hash) {
      g_hash_table_destroy(per_display_data_hash);
   }
}

//
// Locking
//

static void pdd_init(Per_Display_Data * pdd) {
   init_display_sleep_data(pdd);
   drd_init(pdd);
}


/** Gets the #Per_Display_Data struct for the current thread, using the
 *  current thread's id number. If the struct does not already exist, it
 *  is allocated and initialized.
 *
 *  @return pointer to #Per_Display_Data struct
 *
 *  @remark
 *  The structs are maintained centrally rather than using a thread-local pointer
 *  to a block on the heap because the of a problems when the thread is closed.
 *  Valgrind complains of access errors for closed threads, even though the
 *  struct is on the heap and still readable.
 */
Per_Display_Data * pdd_get_per_display_data() {
   bool debug = false;
   // intmax_t cur_display_id = get_display_id();
   Thread_Output_Settings * thread_settings = get_thread_settings();
   intmax_t cur_display_id = thread_settings->tid;
   // DBGMSF(debug, "Getting thread sleep data for thread %d", cur_display_id);
   // bool this_function_owns_lock = pdd_lock_if_unlocked();
   assert(per_display_data_hash);    // allocated by init_display_data_module()
   // DBGMSG("per_display_data_hash = %p", per_display_data_hash);
   // n. data hash for current thread can only be looked up from current thread,
   // so there's nothing can happen to per_display_data_hash before g_hash_table_insert()
   Per_Display_Data * data = g_hash_table_lookup(per_display_data_hash,
                                            GINT_TO_POINTER(cur_display_id));
   if (!data) {
      DBGMSF(debug, "==> Per_Display_Data not found for thread %d", cur_display_id);
      data = g_new0(Per_Display_Data, 1);
      data->display_id = cur_display_id;
      g_private_set(&lock_depth, GINT_TO_POINTER(0));
      pdd_init(data);
      DBGMSF(debug, "Initialized: %s. display_sleep_data_defined: %s.display_retry_data_defined; %s",
           sbool(data->initialized),
           sbool( data->display_sleep_data_defined), sbool( data->display_retry_data_defined));

      g_hash_table_insert(per_display_data_hash,
                          GINT_TO_POINTER(cur_display_id),
                          data);
      DBGMSF(debug, "Created Per_Display_Data struct for thread id = %d", data->display_id);
      DBGMSF(debug, "per_display_data_hash size=%d", g_hash_table_size(per_display_data_hash));
      // if (debug)
      //   dbgrpt_per_display_data(data, 1);
   }
   // pdd_unlock_if_needed(this_function_owns_lock);
   return data;
}


// Thread description operations always operate on the Per_Display_Data
// instance for the currently executing thread.

void pdd_set_display_description(const char * description) {
   pdd_cross_display_operation_block();
   Per_Display_Data *  pdd = pdd_get_per_display_data();
   // DBGMSG("thread: %d, description: %s", pdd->display_id, description);
   if (pdd->description)
      free(pdd->description);
   pdd->description = g_strdup(description);
}


void pdd_append_display_description(const char * addl_description) {
   pdd_cross_display_operation_block();
   Per_Display_Data *  pdd = pdd_get_per_display_data();
   // DBGMSG("pdd->description = %s, addl_descripton = %s", pdd->description, addl_description);
   if (!pdd->description)
      pdd->description = g_strdup(addl_description);
   else if (str_contains(pdd->description, addl_description) < 0) {
      char * s = pdd->description;
      pdd->description = g_strdup_printf("%s; %s", pdd->description, addl_description);
      free(s);
   }

   // DBGMSG("Final pdd->description = %s", pdd->description);
}


const char * pdd_get_display_description_t() {
   static GPrivate  x_key = G_PRIVATE_INIT(g_free);
   static GPrivate  x_len_key = G_PRIVATE_INIT(g_free);

   pdd_cross_display_operation_block();
   Per_Display_Data *  pdd = pdd_get_per_display_data();
   char * buf = NULL;
   if (pdd->description) {
      char * buf = get_thread_dynamic_buffer(&x_key, &x_len_key, strlen(pdd->description)+1);
      strcpy(buf,pdd->description);
   }
   return buf;
}


char * dd_int_array_to_string(uint16_t * start, int ct) {
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


/** Output a debug report of a #Per_Display_Data struct.
 *
 *  @param  data   pointer to #Per_Display_Data struct
 *  @param  depth  logical indentation level
 *
 *  // relies on caller for possible blocking
 */
void dbgrpt_per_display_data(Per_Display_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Per_Display_Data", data, depth);
 //rpt_int( "sizeof(Per_Display_Data)",  NULL, sizeof(Per_Display_Data),   d1);
   rpt_bool("initialized",                NULL, data->initialized,           d1);
   rpt_int( "display_id",                  NULL, data->display_id,             d1);
   rpt_vstring(d1,"description                %s", data->description);
   rpt_bool("sleep data initialized" ,    NULL, data->display_sleep_data_defined, d1);

   // Sleep multiplier adjustment:
   rpt_vstring(d1, "sleep-multiplier value:           %15.2f", data->sleep_multiplier_factor);
   rpt_int("sleep_multiplier_ct",         NULL, data->sleep_multiplier_ct,        d1);
   rpt_vstring(d1, "sleep_multiplier_changer_ct:      %15d",   data->sleep_multipler_changer_ct);
   rpt_vstring(d1, "highest_sleep_multiplier_ct:      %15d",   data->highest_sleep_multiplier_ct);

   rpt_vstring(d1,"cur_dh:                 %s", dh_repr(data->cur_dh) );

   // Dynamic sleep adjustment:
   rpt_bool("dynamic_sleep_enabled",      NULL, data->dynamic_sleep_enabled,     d1);
   rpt_int("cur_ok_status_count",         NULL, data->cur_ok_status_count,       d1);
   rpt_int("cur_error_status_count",      NULL, data->cur_error_status_count,    d1);
   rpt_int("total_ok_status_count",       NULL, data->total_ok_status_count,     d1);
   rpt_int("total_error",                 NULL, data->total_error_status_count,  d1);
   rpt_int("other_status_ct",             NULL, data->total_other_status_ct,     d1);

   rpt_int("calls_since_last_check",      NULL, data->calls_since_last_check,    d1);
   rpt_int("total_adjustment_checks",     NULL, data->total_adjustment_checks,   d1);
   rpt_int("adjustment_ct",               NULL, data->total_adjustment_ct,       d1);
// rpt_int("max_adjustment_ct",           NULL, data->total_max_adjustment_ct,   d1);
// rpt_int("non_adjustment_ct",           NULL, data->total_non_adjustment_ct,   d1);

   rpt_vstring(d1, "cur_sleep_adjustmet_factor     %15.2f", data->cur_sleep_adjustment_factor);
// rpt_vstring(d1, "display_adjustment_increment        %15.2f", data->display_adjustment_increment);

   // Maxtries history
   rpt_bool("retry data initialized"    , NULL, data->display_retry_data_defined, d1);

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
      char * buf = dd_int_array_to_string( data->try_stats[retry_type].counters, upper_bound);
      rpt_vstring(d1, "try_stats[%d=%-27s].counters = %s",
                      retry_type, display_retry_type_name(retry_type), buf);
      free(buf);
   }
}


/** Applies a specified function with signature GFunc to all
 *  #Per_Display_Data instances.
 *
 *  @param  func  function to apply
 *  \parm   arg   an arbitrary argument passed as a pointer
 *
 *  This is a multi-instance operation.
 */
void pdd_apply_all(Dtd_Func func, void * arg) {
   pdd_cross_display_operation_start();
   bool debug = false;
   assert(per_display_data_hash);    // allocated by init_display_data_module()

      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,per_display_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Display_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->display_id);
         func(data, arg);
      }

   pdd_cross_display_operation_end();
}


/** Apply a given function to all #Per_Display_Data structs, ordered by display id.
 *  Note that this report includes structs for displays that have been closed.
 *
 *  @param func function to apply
 *  @param arg pointer or integer value
 *
 *  This is a multi-instance operation.
 */
void pdd_apply_all_sorted(Dtd_Func func, void * arg) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   pdd_cross_display_operation_start();
   assert(per_display_data_hash);

   DBGMSF(debug, "hash table size = %d", g_hash_table_size(per_display_data_hash));
   GList * keys = g_hash_table_get_keys (per_display_data_hash);
   GList * new_head = g_list_sort(keys, gaux_ptr_intcomp);
   GList * l;
   for (l = new_head; l != NULL; l = l->next) {
      int key = GPOINTER_TO_INT(l->data);
      DBGMSF(debug, "Key: %d", key);
      Per_Display_Data * data = g_hash_table_lookup(per_display_data_hash, l->data);
      assert(data);

      func(data, arg);
   }
   g_list_free(new_head);   // would keys also work?

   pdd_cross_display_operation_end();
   DBGMSF(debug, "Done");
}


/** Emits a brief summary of a #Per_Display_Data instance,
 *  showing the display id number and description.
 *
 *  pdd   pointer to #Per_Display_Data instance
 *  arg   logical indentation
 *
 *  @note
 *  This function has a GFunc signature
 *  @note
 *  Called only by multi-display-data functions that hold lock
 */
void pdd_display_summary(Per_Display_Data * pdd, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   int d1 = depth+1;
   pdd_cross_display_operation_block();

   // simple but ugly
   // rpt_vstring(depth, "Thread: %d. Description:%s",
   //             pdd->display_id, pdd->description);

   // rpt_vstring(depth, "Thread: %d", pdd->display_id);
   // char * header = "Description: ";

   char header[100];
   g_snprintf(header, 100, "Thread %d: ", pdd->display_id);

   int hdrlen = strlen(header);
   if (!pdd->description)
      rpt_vstring(d1, "%s", header);
   else {
      Null_Terminated_String_Array pieces =
            strsplit_maxlength(pdd->description, 60, " ");
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
}


/** Emits a brief summary (display id and description) for each
 * #Per_Display_Data instance.
 *
 *  @param  depth   logical indentation depth
 */
void pdd_list_displays(int depth) {
   // bool this_function_owns_lock = pdd_lock_if_unlocked();

   int d1 = depth +1;
   // rpt_label(depth, "Have data for displays:");
   rpt_label(depth, "Report has per-display data for displays:");
   pdd_apply_all_sorted(pdd_display_summary, GINT_TO_POINTER(d1));
   rpt_nl();
}


void report_all_display_status_counts(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   rpt_label(depth, "No per-display status code statistics are collected");
   rpt_nl();
   DBGMSF(debug, "Done");
}

