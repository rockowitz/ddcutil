/** @file per_display_data.c
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <base/dsa0.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <sys/types.h>

#include "util/debug_util.h"
#include "util/glib_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/display_retry_data.h"    // temp circular
#include "base/displays.h"
#include "base/dsa0.h"
#include "base/dsa1.h"
#include "base/dsa2.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "base/per_display_data.h"

#define TRACE_GROUP  DDCA_TRC_NONE


// Master table of sleep data for all displays
GHashTable *    per_display_data_hash = NULL;

//
// Locking
//
static GPrivate pdd_this_thread_has_lock;
static GPrivate pdd_lock_depth; // GINT_TO_POINTER(0);
static bool     debug_mutex = false;
       int      pdd_lock_count = 0;
       int      pdd_unlock_count = 0;
       int      pdd_cross_thread_operation_blocked_count = 0;

double user_sleep_multiplier = 1.0; // may be changed by --sleep-multiplier option
bool   explicit_user_sleep_multiplier = false;

void dbgrpt_per_display_data_locks(int depth) {
   rpt_vstring(depth, "pdd_lock_count:                            %-4d", pdd_lock_count);
   rpt_vstring(depth, "pdd_unlock_count:                          %-4d", pdd_unlock_count);
   rpt_vstring(depth, "pdd_cross_thread_operation_blocked_count:  %-4d", pdd_cross_thread_operation_blocked_count);
}

static GMutex try_data_mutex;


/** If **try_data_mutex** is not already locked by the current thread,
 *  lock it.
 *
 *  \remark
 *  This function is necessary because the behavior if a GLib mutex is
 *  relocked by the current thread is undefined.
 */

// avoids locking if this thread already owns the lock, since behavior undefined
bool pdd_lock_if_unlocked() {
   bool debug = false;
   debug = debug || debug_mutex;

   bool lock_performed = false;
   bool thread_has_lock = GPOINTER_TO_INT(g_private_get(&pdd_this_thread_has_lock));
   DBGMSF(debug, "Already locked: %s", sbool(thread_has_lock));
   if (!thread_has_lock) {
      g_mutex_lock(&try_data_mutex);
      lock_performed = true;
      // should this be a depth counter rather than a boolean?
      g_private_set(&pdd_this_thread_has_lock, GINT_TO_POINTER(true));
      if (debug) {
         intmax_t cur_thread_id = get_thread_id();
         DBGMSG("Locked by thread %d", cur_thread_id);
      }
   }

   DBGMSF(debug, "Returning: %s", sbool(lock_performed) );
   return lock_performed;
}


/** Unlocks the **try_data_mutex** set by a call to #lock_if_unlocked
 *
 *  \param  unlock_requested perform unlock
 */
void pdd_unlock_if_needed(bool unlock_requested) {
   bool debug = false;
   debug = debug || debug_mutex;
   DBGMSF(debug, "unlock_requested=%s", sbool(unlock_requested));

   if (unlock_requested) {
      // is it actually locked?
      bool currently_locked = GPOINTER_TO_INT(g_private_get(&pdd_this_thread_has_lock));
      DBGMSF(debug, "currently_locked = %s", sbool(currently_locked));
      if (currently_locked) {
         g_private_set(&pdd_this_thread_has_lock, GINT_TO_POINTER(false));
         if (debug) {
            intmax_t cur_thread_id = get_thread_id();
            DBGMSG("Unlocked by thread %d", cur_thread_id);
         }
         g_mutex_unlock(&try_data_mutex);
      }
   }

   DBGMSF(debug, "Done");
}



static bool    cross_thread_operation_active = false;
static GMutex  cross_thread_operation_mutex;
static pid_t   cross_thread_operation_owner;

// The locking strategy relies on the fact that in practice conflicts
// will be rare, and critical sections short.
// Operations are blocked only using a spin-lock.

// The groups of operations:
// - Operations that operate on the single Per_Display_Data instance
//   associated with the currently executing thread.
// - Operations that operate on a single Per_Display_Data instance,
//   but possibly not from the thread associated with the Per_Display_Data instance.  ???
// - Operations that operate on multiple Per_Display_Data instances.
//   These are referred to as cross thread operations.
//   Alt, perhaps clearer, refer to them as multi-thread data instances.

/**
 */
bool pdd_cross_display_operation_start(const char * caller) {
   // Only 1 cross display action can be active at one time.
   // All per_display actions must wait

   bool debug = false;
   debug = debug || debug_mutex;

   bool lock_performed = false;

   int display_lock_depth = GPOINTER_TO_INT(g_private_get(&pdd_lock_depth));
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
      "Caller %s, lock depth: %d. pdd_lock_count=%d, pdd_unlock_count=%d",
      caller, display_lock_depth, pdd_lock_count, pdd_unlock_count);

   if (display_lock_depth == 0) {    // (A)
   // if (!thread_has_lock) {
      // display_lock_depth is per-thread, so must be unchanged from (A)
      g_mutex_lock(&cross_thread_operation_mutex);
      lock_performed = true;
      cross_thread_operation_active = true;
      pdd_lock_count++;
      Thread_Output_Settings * thread_settings = get_thread_settings();
      intmax_t cur_thread_id = thread_settings->tid;  // alt: get_thread_id()
      cross_thread_operation_owner = cur_thread_id;
      DBGMSF(debug, "          Locked performed by thread %d", cur_thread_id);
      sleep_millis(10);   // give all per-thread functions time to finish
   }
   display_lock_depth+=1;
   g_private_set(&pdd_lock_depth, GINT_TO_POINTER(display_lock_depth));
   DBGTRC_DONE(debug, DDCA_TRC_NONE,
         "Caller: %s, pdd_display_lock_depth=%d, pdd_lock_count=%d, pdd_unlock_cound=%d, Returning lock_performed: %s,",
         caller, display_lock_depth, pdd_lock_count, pdd_unlock_count, sbool(lock_performed));
   return lock_performed;
}


void pdd_cross_display_operation_end(const char * caller) {
   bool debug = false;
   int display_lock_depth = GPOINTER_TO_INT(g_private_get(&pdd_lock_depth));
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
         "Caller: %s, display_lock_depth=%d, pdd_lock_count=%d, pdd_unlock_count=%d",
         caller, display_lock_depth, pdd_lock_count, pdd_unlock_count);
   assert(display_lock_depth >= 1);
   g_private_set(&pdd_lock_depth, GINT_TO_POINTER(display_lock_depth-1));

   if (display_lock_depth == 1) {
      cross_thread_operation_active = false;
      cross_thread_operation_owner = 0;
      pdd_unlock_count++;
      assert(pdd_lock_count == pdd_unlock_count);
      g_mutex_unlock(&cross_thread_operation_mutex);
   }
   else {
      assert( pdd_lock_count > pdd_unlock_count );
   }
   display_lock_depth -= 1;
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Caller: %s, display_lock_depth=%d, pdd_lock_count=%d, pdd_unlock_count=%d",
         caller, display_lock_depth, pdd_lock_count, pdd_unlock_count);
}


/** Block execution of single Per_Display_Data operations when an operation
 *  involving multiple Per_Thead_Data instances is active.
 */

void pdd_cross_display_operation_block(const char * caller) {
   // intmax_t cur_displayid = get_display_id();
   Thread_Output_Settings * thread_settings = get_thread_settings();
   intmax_t cur_displayid = thread_settings->tid;
   if (cross_thread_operation_active && cur_displayid != cross_thread_operation_owner) {
      __sync_fetch_and_add(&pdd_cross_thread_operation_blocked_count, 1);
      do {
         sleep_millis(10);
      } while (cross_thread_operation_active);
   }
}


// GDestroyNotify: void (*GDestroyNotify) (gpointer data);

void per_display_data_destroy(void * data) {
   if (data) {
      Per_Display_Data * pdd = data;
      free(pdd);
   }
}


//
// Sleep Multiplier Factor
//

/** Sets the default sleep multiplier factor, used for the creation of any new displays.
 * This is a global value and is a floating point number.
 *
 *  @param multiplier
 *
 *  @remark Intended for use only during program initialization.  If used
 *          more generally, get and set of default sleep multiplier needs to
 *          be protected by a lock.
 *  @todo
 *  Add Sleep_Event_Type bitfield to make sleep factor dependent on event type?
 */
void pdd_set_default_sleep_multiplier_factor(double multiplier, bool explicit) {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_NONE,
                    "Executing. Setting default_sleep_multiplier_factor = %6.3f, explicit = %s",
                    multiplier, sbool(explicit));
   assert(multiplier >= 0);
   if (multiplier == 0.0f)
      multiplier = .01;
   user_sleep_multiplier = multiplier;
   explicit_user_sleep_multiplier = explicit;
}


/** Gets the default sleep multiplier factor.
 *
 *  @return sleep multiplier factor
 */
double pdd_get_default_sleep_multiplier_factor() {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_NONE,
          "Returning default_sleep_multiplier_factor = %6.3f", user_sleep_multiplier);
   return user_sleep_multiplier;
}


/** Sets the sleep multiplier factor for the current display.
 *
 *  @param factor  sleep multiplier factor
 */
void pdd_set_sleep_multiplier_factor(Per_Display_Data * data, double factor) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "factor = %6.3f", factor);
   assert(factor >= 0);
   data->user_sleep_multiplier = factor;
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


/** Gets the sleep multiplier factor for the current display.
 *
 *  @return sleep multiplier factor
 */
double pdd_get_sleep_multiplier_factor(Per_Display_Data * data) {
   bool debug = false;
   double result = data->user_sleep_multiplier;
   DBGTRC(debug, DDCA_TRC_NONE, "Returning %6.3f", result );
   return result;
}



#ifdef UNUSED
// apply the sleep-multiplier to any existing displays
// it will be set for new displays from global_sleep_multiplier_factor
void set_sleep_multiplier_factor_all(double factor) {
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. factor = %5.2f", factor);
   if (display_sleep_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,display_sleep_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Display_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->display_id);
         data->user_sleep_multiplier = factor;
      }
   }
}

default_dynamic_sleep_enabled

void set_global_sleep_multiplier_factor(double factor) {
   bool debug = false;
   DBGMSF(debug, "factor = %5.2f", factor);
   global_sleep_multiplier_factor = factor;
   // set_sleep_multiplier_factor_all(factor);   // only applies to new displays, do not change existing displays
}


double get_global_sleep_multiplier_factor() {
   return global_sleep_multiplier_factor;
}
#endif


//
// Locking
//

void pdd_init_pdd(Per_Display_Data * pdd) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
         "Initializing Per_Display_Data for %s", dpath_repr_t(&pdd->dpath));
   pdd->user_sleep_multiplier                      = user_sleep_multiplier;
   pdd->initial_adjusted_sleep_multiplier          = -1.0f;
   pdd->final_successful_adjusted_sleep_multiplier = -1.0f;
   pdd->total_sleep_time_millis = 0;
   if (dsa0_enabled)
      pdd->dsa0_data = dsa0_get_dsa0_data(pdd->dpath.path.i2c_busno);
   if (dsa1_enabled)
      pdd->dsa1_data = new_dsa1_data(pdd);
   drd_init_display_data(pdd);   // initialize the retry data section of Per_Display_Data
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Device = %s, user_sleep_multiplier=%4.2f", dpath_repr_t(&pdd->dpath), pdd->user_sleep_multiplier);
   if (debug)
      dbgrpt_per_display_data(pdd, 1);
}



/** Gets the #Per_Display_Data struct for a specified display.
 *  If the struct does not already exist, it is allocated and initialized.
 *
 *  @param  dpath  device access path
 *  @return pointer to #Per_Display_Data struct
 *
 *  @remark
 *  Historical comment from per-thread data.  Does this still matter?
 *  The structs are maintained centrally rather than using a thread-local pointer
 *  to a block on the heap because the of a problems when the thread is closed.
 *  Valgrind complains of access errors for closed threads, even though the
 *  struct is on the heap and still readable.
 */
Per_Display_Data * pdd_get_per_display_data(DDCA_IO_Path dpath, bool create_if_not_found) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Getting per display data for %s, create_if_not_found = %s",
         dpath_repr_t(&dpath), sbool(create_if_not_found));

   bool this_function_owns_lock = pdd_lock_if_unlocked();
   assert(per_display_data_hash);    // allocated by init_display_data_module()
   int hval = dpath_hash(dpath);
   Per_Display_Data * pdd = g_hash_table_lookup(per_display_data_hash, GINT_TO_POINTER(hval));
   if (!pdd && create_if_not_found) {
      DBGTRC(debug, TRACE_GROUP, "Per_Display_Data not found for %s", dpath_repr_t(&dpath));
      pdd = g_new0(Per_Display_Data, 1);
      pdd->dpath = dpath;
      g_private_set(&pdd_lock_depth, GINT_TO_POINTER(0));
      pdd_init_pdd(pdd);

      g_hash_table_insert(per_display_data_hash, GINT_TO_POINTER(hval), pdd);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Created Per_Display_Data struct for %s", dpath_repr_t(&pdd->dpath));
      DBGMSF(debug, "per_display_data_hash size=%d", g_hash_table_size(per_display_data_hash));
      if (debug)
         dbgrpt_per_display_data(pdd, 1);
   }
   pdd_unlock_if_needed(this_function_owns_lock);
   DBGTRC_NOPREFIX(  debug, TRACE_GROUP, "Device dpath:%s", dpath_repr_t(&dpath) );
   DBGTRC_RET_STRUCT(debug, TRACE_GROUP, Per_Display_Data, dbgrpt_per_display_data, pdd);
   return pdd;
}


void pdd_record_adjusted_sleep_multiplier_bounds(Per_Display_Data * pdd, bool successful) {
   assert(pdd);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bus=%d, initial_adjusted_sleep_multiplier = %4.2f",
         pdd->dpath.path.i2c_busno, pdd->initial_adjusted_sleep_multiplier);

   double cur_sleep_multiplier = pdd_get_adjusted_sleep_multiplier(pdd);
   if (cur_sleep_multiplier >= 0) {
      if (pdd->initial_adjusted_sleep_multiplier < 0.0f)
         pdd->initial_adjusted_sleep_multiplier = cur_sleep_multiplier;
      if (successful)
         pdd->final_successful_adjusted_sleep_multiplier = cur_sleep_multiplier;
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "cur_sleep_multiplier=%4.2f, initial_adjusted_sleep_multiplier = %4.2f, final_successful_adjusted_sleep_multiplier=%4.2f",
         cur_sleep_multiplier, pdd->initial_adjusted_sleep_multiplier, pdd->final_successful_adjusted_sleep_multiplier);
}



// Thread description operations always operate on the Per_Display_Data
// instance for the currently executing thread.

#ifdef OUT
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
#endif


/** Output a debug report of a #Per_Display_Data struct.
 *
 *  @param  data   pointer to #Per_Display_Data struct
 *  @param  depth  logical indentation level
 *
 *  // relies on caller for possible blocking
 */
void dbgrpt_per_display_data(Per_Display_Data * data, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Per_Display_Data",  data, depth);
 //rpt_int( "sizeof(Per_Display_Data)",   NULL, sizeof(Per_Display_Data),          d1);
   rpt_vstring(d1, "dpath                                                    : %s", dpath_repr_t(&data->dpath) );
   // Sleep multiplier adjustment:
   rpt_vstring(d1, "initial_sleep_multiplier                                 : %3.2f", data->initial_adjusted_sleep_multiplier);
   rpt_vstring(d1, "final_sleep_multiplier                                   : %3.2f", data->final_successful_adjusted_sleep_multiplier);


   // Maxtries history
   for (int retry_type = 0; retry_type < 4; retry_type++) {
      char * buf = int_array_to_string( data->try_stats[retry_type].counters, MAX_MAX_TRIES+1);
      rpt_vstring(d1, "try_stats[%d=%-27s].counters = %s",
                      retry_type, retry_type_name(retry_type), buf);
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
void pdd_apply_all(Pdd_Func func, void * arg) {
   pdd_cross_display_operation_start(__func__);
   bool debug = false;
   assert(per_display_data_hash);    // allocated by init_display_data_module()

      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,per_display_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Display_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->dpath);
         func(data, arg);
      }

   pdd_cross_display_operation_end(__func__);
}


/** Apply a given function to all #Per_Display_Data structs, ordered by display id.
 *  Note that this report includes structs for displays that have been closed.
 *
 *  @param func function to apply
 *  @param arg pointer or integer value
 *
 *  This is a multi-instance operation.
 */
void pdd_apply_all_sorted(Pdd_Func func, void * arg) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   pdd_cross_display_operation_start(__func__);
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

   pdd_cross_display_operation_end(__func__);
   DBGMSF(debug, "Done");
}


void pdd_reset_per_display_data(Per_Display_Data * data, void* arg ) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "data = %p, dpath=%s", data, dpath_repr_t(&data->dpath));

   Per_Display_Data * pdd = data;
   pdd->total_sleep_time_millis = 0.0;
   // what other fields to reset?

   if (pdd->dsa0_data)
      dsa0_reset(pdd->dsa0_data);
   if (pdd->dsa1_data)
      dsa1_reset_data(pdd->dsa1_data);
   if (dsa2_enabled)
      dsa2_reset_by_dpath(pdd->dpath);
   for (int retry_type = 0; retry_type < 4; retry_type++) {
      for (int ndx = 0; ndx < MAX_MAX_TRIES+2; ndx++) {
         pdd->try_stats[retry_type].counters[ndx] = 0;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


void pdd_reset_all() {
   pdd_cross_display_operation_block(__func__);
   pdd_apply_all(pdd_reset_per_display_data, NULL);
   pdd_cross_display_operation_end(__func__);
}


#ifdef UNUSED
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
   pdd_cross_display_operation_block(__func__);

   // simple but ugly
   // rpt_vstring(depth, "Thread: %d. Description:%s",
   //             pdd->display_id, pdd->description);

   // rpt_vstring(depth, "Thread: %d", pdd->display_id);
   // char * header = "Description: ";

   char header[100];
   g_snprintf(header, 100, "Display path %s: ", dpath_repr_t(&pdd->dpath));
   rpt_vstring(d1, "%s", header);
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
#endif


void pdd_report_all_display_status_counts(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   rpt_label(depth, "No per-display status code statistics are collected");
   rpt_nl();
   DBGMSF(debug, "Done");
}


void pdd_report_elapsed(Per_Display_Data * pdd, int depth) {
   // bool debug = false;
   rpt_vstring(depth, "Elapsed time report for display %s (pdd)", dpath_short_name_t(&pdd->dpath));
   int d1 = depth+1;

   char * s = (dsa2_enabled && dsa2_is_from_cache_by_dpath(pdd->dpath) ?   "  (from cache)" : "" );

   rpt_vstring(d1, "User sleep multiplier factor:   %7.2f",   pdd->user_sleep_multiplier);
   rpt_vstring(d1, "Initial adjusted multiplier:    %7.2f%s", pdd->initial_adjusted_sleep_multiplier, s);
   rpt_vstring(d1, "Final adjusted multiplier:      %7.2f",   pdd->final_successful_adjusted_sleep_multiplier);
   rpt_vstring(d1, "Total sleep time (milliseconds):  %5d",   pdd->total_sleep_time_millis);
   // if (debug || get_output_level() >= DDCA_OL_VV) {
   //    rpt_vstring(d1, "Internal data: (A)");
   // }
   rpt_nl();

   // report_display_all_types_data_by_data(false, pdd, depth);

   if (dsa0_enabled) {
      assert(pdd->dsa0_data);
      report_dsa0_data(pdd->dsa0_data, d1);
   }
   if (dsa1_enabled && pdd->dsa1_data && get_output_level() >= DDCA_OL_VERBOSE) {
      dsa1_report(pdd->dsa1_data, d1);
   }
   if (dsa2_enabled && get_output_level() >= DDCA_OL_VV) {
         dsa2_report_all(d1);  // detailed internal info
   }
   rpt_nl();
}


void pdd_report_all_elapsed(int depth) {
   // rpt_label(depth, "PER DISPLAY ELAPSED DETAIL");
   for (int ndx = 0; ndx <= I2C_BUS_MAX; ndx++) {
      DDCA_IO_Path dpath;
      dpath.io_mode = DDCA_IO_I2C;
      dpath.path.i2c_busno = ndx;
      Per_Display_Data * pdd = pdd_get_per_display_data(dpath, false);
      if (pdd)
         pdd_report_elapsed(pdd, depth);
   }
}


void pdd_reset_multiplier(Per_Display_Data * pdd, float multiplier) {
   pdd->user_sleep_multiplier = multiplier;
   if (dsa0_enabled) {
      dsa0_reset(pdd->dsa0_data);
   }
   else if (dsa1_enabled) {
      dsa1_reset_data(pdd->dsa1_data);
   }
   else if (dsa2_enabled) {
     // dsa2_reset(pdd->dpath);     // TO DO
   }
}


double pdd_get_adjusted_sleep_multiplier(Per_Display_Data * pdd) {
   float result = 1.0f;

   if (dsa0_enabled) {
      result = dsa0_get_adjusted_sleep_multiplier(pdd->dsa0_data);
   }
   else if (dsa1_enabled) {
      result = dsa1_get_adjusted_sleep_multiplier(pdd->dsa1_data);
   }
   else if (dsa2_enabled) {
      result = dsa2_get_adjusted_sleep_multiplier_by_dpath(pdd->dpath);
   }
   else {
      result = pdd->user_sleep_multiplier;
   }

   return result;
}


void pdd_note_retryable_failure(Per_Display_Data * pdd, int remaining_tries) {
   if (dsa0_enabled) {
      dsa0_note_retryable_failure(pdd->dsa0_data, remaining_tries);
   }
   else if (dsa1_enabled) {
      dsa1_note_retryable_failure_by_pdd(pdd, remaining_tries);
   }
   else if (dsa2_enabled) {
      dsa2_note_retryable_failure_by_dpath(pdd->dpath, remaining_tries);
   }
   pdd_record_adjusted_sleep_multiplier_bounds(pdd, false);
}


void  pdd_record_final(Per_Display_Data * pdd, DDCA_Status ddcrc, int retries) {
   if (dsa0_enabled) {
      dsa0_record_final_by_pdd(pdd, ddcrc, retries);
   }
   else if (dsa1_enabled) {
      dsa1_record_final_by_pdd(pdd, ddcrc, retries);
   }
   else if (dsa2_enabled) {
      dsa2_record_final_by_dpath(pdd->dpath, ddcrc, retries);
   }
   if (ddcrc == 0)
      pdd_record_adjusted_sleep_multiplier_bounds(pdd, true);
}


void pdd_reset_multiplier_by_dh(Display_Handle * dh, float multiplier) {
   Per_Display_Data * pdd = dh->dref->pdd;
   pdd_reset_multiplier(pdd, multiplier);
}

float pdd_get_sleep_multiplier_by_dh(Display_Handle * dh)
{
   Per_Display_Data * pdd = dh->dref->pdd;
   return pdd_get_adjusted_sleep_multiplier(pdd);
}

void pdd_note_retryable_failure_by_dh(Display_Handle * dh, int remaining_tries)
{
   Per_Display_Data * pdd = dh->dref->pdd;
   pdd_note_retryable_failure(pdd, remaining_tries);
}
void pdd_record_final_by_dh(Display_Handle * dh, DDCA_Status ddcrc, int retries) {
   Per_Display_Data * pdd = dh->dref->pdd;
   pdd_record_final(pdd, ddcrc, retries);
}


//
// Initialization and Termination
//

/** Initialize per_display_data.c at program startup */
static void pdd_init_per_display_data_module() {
   per_display_data_hash = g_hash_table_new_full(g_direct_hash, NULL, NULL, per_display_data_destroy);
   // DBGMSG("per_display_data_hash = %p", per_display_data_hash);
}

static void pdd_release_per_display_data_module() {
   if (per_display_data_hash) {
      g_hash_table_destroy(per_display_data_hash);
   }
}


void init_per_display_data() {
   RTTI_ADD_FUNC(pdd_get_per_display_data);
   RTTI_ADD_FUNC(pdd_cross_display_operation_start);
   RTTI_ADD_FUNC(pdd_cross_display_operation_end);
   pdd_init_per_display_data_module();
}

void terminate_per_display_data() {
   pdd_release_per_display_data_module();
}

