/** @file ddc_try_stats.c
 *
 *  Maintains statistics on DDC retries, along with maxtries settings.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>


#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/parms.h"
#include "base/per_thread_data.h"    // for retry_type_name()
#include "base/thread_retry_data.h"
#include "base/thread_sleep_data.h"

#include "ddc/ddc_try_stats.h"

//
// Locking
//

static GMutex try_data_mutex;
static GPrivate this_thread_has_lock;
static bool debug_mutex = false;


/** If **try_data_mutex** is not already locked by the current thread,
 *  lock it.
 *
 *  \remark
 *  This function is necessary because the behavior if a GLib mutex is
 *  relocked by the curren thread is undefined.
 */

// avoids locking if this thread already owns the lock, since behavior undefined
bool lock_if_unlocked() {
   bool debug = false;
   debug = debug || debug_mutex;
   bool lock_performed = false;
   bool thread_has_lock = GPOINTER_TO_INT(g_private_get(&this_thread_has_lock));
   DBGMSF(debug, "Already locked: %s", sbool(thread_has_lock));
   if (!thread_has_lock) {
      g_mutex_lock(&try_data_mutex);
      lock_performed = true;
      // should this be a depth counter rather than a boolean?
      g_private_set(&this_thread_has_lock, GINT_TO_POINTER(true));
      if (debug) {
         pid_t cur_thread_id = syscall(SYS_gettid);
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
void unlock_if_needed(bool unlock_requested) {
   bool debug = false;
   debug = debug || debug_mutex;
   DBGMSF(debug, "unlock_requested=%s", sbool(unlock_requested));

   if (unlock_requested) {
      // is it actually locked?
      bool currently_locked = GPOINTER_TO_INT(g_private_get(&this_thread_has_lock));
      DBGMSF(debug, "currently_locked = %s", sbool(currently_locked));
      if (currently_locked) {
         g_private_set(&this_thread_has_lock, GINT_TO_POINTER(false));
         if (debug) {
            pid_t cur_thread_id = syscall(SYS_gettid);
            DBGMSF(debug, "Unlocked by thread %d", cur_thread_id);
         }
         g_mutex_unlock(&try_data_mutex);
      }
   }
}


/** Requests a lock on the **try data** data structure.
 *  A lock is not performed if the current thread already holds the lock
 *
 *  \return  true if a lock was actually performed
 */
bool try_data_lock() {
   return lock_if_unlocked();
}


/** Requests that the currently held lock on the **try_data** data structure
 *  be released
 *
 *  \param release_requested  if true, attempt to unlock
 */
void try_data_unlock(bool release_requested) {
   unlock_if_needed(release_requested);
}


// counters usage:
//  0  number of failures because of fatal errors
//  1  number of failures because retry exceeded
//  n>1 number of successes after n-1 tries,
//      e.g. if succeed after 1 try, recorded in counter 2

// 1 instance for each retry type
typedef
struct {
   Retry_Operation retry_type;
   Retry_Op_Value    maxtries;
   Retry_Op_Value    counters[MAX_MAX_TRIES+2];
   Retry_Op_Value    highest_maxtries;
   Retry_Op_Value    lowest_maxtries;
} Try_Data2;


static Retry_Op_Value default_maxtries[] = {
      INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES,
      INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES };


static Try_Data2 try_data[RETRY_OP_COUNT];


/* Initializes a Try_Data data structure
 *
 * @param  retry_type  Retry_Operation type
 * #param  maxtries    maximum number of tries
 */
void try_data_init_retry_type(Retry_Operation retry_type, Retry_Op_Value maxtries) {
   try_data[retry_type].retry_type       = retry_type;
   try_data[retry_type].maxtries         = maxtries;
   try_data[retry_type].highest_maxtries = maxtries;
   try_data[retry_type].lowest_maxtries  = maxtries;
}


/** Performs initialization at time of program startup.
 */
void try_data_init() {
   for (int retry_type = 0; retry_type < RETRY_OP_COUNT; retry_type++) {
      try_data_init_retry_type(retry_type, default_maxtries[retry_type]);
   }
}


//
// Maxtries
//

/** Gets the current maximum number of tries allowed for an operation
 *
 *  \param  retry_type
 *  \return maxtries value
 */
Retry_Op_Value try_data_get_maxtries2(Retry_Operation retry_type) {
   bool debug = false;
   int result = try_data[retry_type].maxtries;
   DBGMSF(debug, "retry type=%s, returning %d", retry_type_name(retry_type), result);
   return result;
}


/** Sets the maxtries value for an operation
 *
 * \param retry_type
 * \param new_maxtries
 */
void try_data_set_maxtries2(Retry_Operation retry_type, Retry_Op_Value new_maxtries) {
   bool debug = false;

   Try_Data2 * stats_rec = &try_data[retry_type];
   DBGMSF(debug, "Starting. stats type: %s for %s, new_maxtries: %d",
                 retry_type_name(stats_rec->retry_type),
                 retry_type_description(stats_rec->retry_type),
                 new_maxtries);

   assert(new_maxtries >= 1 && new_maxtries <= MAX_MAX_TRIES);

   bool this_function_performed_lock = lock_if_unlocked();

   stats_rec->maxtries = new_maxtries;
   if (new_maxtries < stats_rec->lowest_maxtries)
      stats_rec->lowest_maxtries = new_maxtries;
   if (new_maxtries > stats_rec->highest_maxtries)
      stats_rec->highest_maxtries = new_maxtries;

   trd_set_all_maxtries(retry_type, new_maxtries);

   unlock_if_needed(this_function_performed_lock);

   DBGMSF(debug, "Done");
}


//
// Reset counters
//

/** Resets the counters to 0 for the specified #Retry_Operation, and resets
 *  the highest and lowest maxtries value seen to the current maxtries value.
 *
 *  @param retry type
 */
void try_data_reset2(Retry_Operation retry_type) {
   bool debug = false;
   DBGMSF(debug, "Starting, retry type: %s", retry_type_name(retry_type));

   bool this_function_performed_lock = lock_if_unlocked();

   DBGMSF(debug, "Setting highest_maxtries, lowest_maxtries = current maxtries: %d",
                 try_data[retry_type].maxtries);
   // Reset does not change the current maxtries value, but it does reset the
   // highest and lowest values seen to the current value.
   Retry_Op_Value current_maxtries = try_data[retry_type].maxtries;
   try_data[retry_type].highest_maxtries = current_maxtries;
   try_data[retry_type].lowest_maxtries =  current_maxtries;

   for (int ndx=0; ndx < MAX_MAX_TRIES+1; ndx++)
      try_data[retry_type].counters[ndx] = 0;

   unlock_if_needed(this_function_performed_lock);

   DBGMSF(debug, "Done");
}


/** Resets the counters for all retry types
 */
void try_data_reset2_all() {
   bool this_function_performed_lock = lock_if_unlocked();

   for (int retry_type = 0; retry_type < RETRY_OP_COUNT; retry_type++) {
      try_data_reset2(retry_type);
   }

   unlock_if_needed(this_function_performed_lock);
}


#ifdef OLD
static void record_successful_tries2(Retry_Operation retry_type, int tryct){
   bool debug = false || debug_mutex;
   // DBGMSG("=============================================");
   DBGMSF(debug, "Starting. retry_type=%d - %s, tryct=%d",
                 retry_type, retry_type_name(retry_type), tryct);
   Try_Data2 * stats_rec = try_data[retry_type];
   DBGMSF(debug, "Current stats_rec->maxtries=%d", stats_rec->maxtries);
   assert(0 < tryct && tryct <= stats_rec->maxtries);

   //g_mutex_lock(&try_data_mutex);
   stats_rec->counters[tryct+1] += 1;
   // g_mutex_unlock(&try_data_mutex);

   DBGMSF(debug, "Done");
}


static void record_failed_max_tries2(Retry_Operation retry_type) {
   bool debug = false || debug_mutex;
   DBGMSF(debug, "Starting");

   Try_Data2 * stats_rec = try_data[retry_type];
   // g_mutex_lock(&try_data_mutex);
   stats_rec->counters[1] += 1;
   // g_mutex_unlock(&try_data_mutex);

   DBGMSF(debug, "Done");
}


static void record_failed_fatally2(Retry_Operation retry_type) {
   bool debug = false || debug_mutex;
    DBGMSF(debug, "Starting");

   Try_Data2 * stats_rec = try_data[retry_type];
   // g_mutex_lock(&try_data_mutex);
   stats_rec->counters[0] += 1;
   // g_mutex_unlock(&try_data_mutex);

   DBGMSF(debug, "Done");
}
#endif


/** Records the status and retry count for a retryable transaction
 *
 *  @param  retry_type
 *  @param  ddcrc status code
 *  @param  tryct number of tries required for success, when rc == 0
 *
 *  @remark
 *  Also calls #trd_record_cur_thread_ties() to record the transaction status
 *  in the per-thread data structure.
 */
void try_data_record_tries2(Retry_Operation retry_type, DDCA_Status ddcrc, int tryct) {
   bool debug = false;
   DBGMSF(debug, "retry_type = %d - %s,  ddcrc=%d, tryct=%d",
                 retry_type, retry_type_name(retry_type), ddcrc, tryct);

   trd_record_cur_thread_tries(retry_type, ddcrc, tryct);

   Try_Data2 * stats_rec = &try_data[retry_type];
   bool locked_by_this_func = lock_if_unlocked();
   if (ddcrc == 0) {
      DBGMSF(debug, "Current stats_rec->maxtries=%d", stats_rec->maxtries);
      assert(0 < tryct && tryct <= stats_rec->maxtries);

      stats_rec->counters[tryct+1] += 1;
   }
   // fragile, but eliminates testing for max_tries:
   else if (ddcrc == DDCRC_RETRIES || ddcrc == DDCRC_ALL_TRIES_ZERO) {
      // failed for max tries exceeded
      stats_rec->counters[1] += 1;
   }
   else {
      // failed fatally
      stats_rec->counters[0] += 1;
   }

   unlock_if_needed(locked_by_this_func);
}

//
// Reporting
//

// used to test whether there's anything to report
static int try_data_get_total_attempts2(Retry_Operation retry_type) {
   int total_attempts = 0;
   int ndx;
   for (ndx=0; ndx <= MAX_MAX_TRIES+1; ndx++) {
      total_attempts += try_data[retry_type].counters[ndx];
   }
   return total_attempts;
}


/** Reports try statistics for a specified #Retry_Operation
 *
 *  Output is written to the current FOUT destination.
 *
 *  \param retry_type
 *  \param depth        logical indentation depth
 *
 */
void try_data_report2(Retry_Operation retry_type, int depth) {
   bool debug = false;
   int d1 = depth+1;
   rpt_nl();
   Try_Data2 * stats_rec = &try_data[retry_type];
   rpt_vstring(depth, "Retry statistics for %s", retry_type_description(retry_type));

   bool this_function_performed_lock = lock_if_unlocked();

   // doesn't distinguish write vs read
   // rpt_vstring(depth, "Retry statistics for ddc %s exchange", ddc_retry_type_description(stats_rec->retry_type));
   int total_attempts = try_data_get_total_attempts2(retry_type);

   if (total_attempts == 0) {
      rpt_vstring(d1, "No tries attempted");
   }
   else {
      int total_successful_attempts = 0;
      int max1 = stats_rec->maxtries;

      // Temporary for consistency check:
      Global_Maxtries_Accumulator acc =
             trd_get_all_threads_maxtries_range(retry_type);
      DBGMSF(debug, "acc.max_highest_maxtries=%d, stats_rec->highest_maxtries = %d",
                    acc.max_highest_maxtries, stats_rec->highest_maxtries);
      // assert (acc.max_highest_maxtries == stats_rec->highest_maxtries);
      // assert (acc.min_lowest_maxtries  == stats_rec->lowest_maxtries);
      if (acc.max_highest_maxtries != stats_rec->highest_maxtries) {
         DBGMSG("acc.max_highest_maxtries(%d) != stats_rec->highest_retries(%d)",
                acc.max_highest_maxtries,stats_rec->highest_maxtries);
      }
      if (acc.min_lowest_maxtries != stats_rec->lowest_maxtries) {
            DBGMSG("acc.max_lowest_maxtries(%d) != stats_rec->lowest_restries(%d)",
                   acc.min_lowest_maxtries,stats_rec->lowest_maxtries);
      }

      rpt_vstring(d1, "Max tries allowed: %d", max1);
      if (acc.min_lowest_maxtries == acc.max_highest_maxtries)
         rpt_vstring(d1, "Max tries allowed: %d", acc.min_lowest_maxtries);

      rpt_vstring(d1, "Max tries allowed range: %d..%d",
                      acc.min_lowest_maxtries, acc.max_highest_maxtries);

      int upper_bound = MAX_MAX_TRIES+1;
      while (upper_bound > 1) {
         if (stats_rec->counters[upper_bound] != 0)
            break;
         upper_bound--;
      }

      // n upper_bound = 1 if no successful attempts
      char * s = (upper_bound == 1) ? " None" : "";
      rpt_vstring(d1, "Successful attempts by number of tries required:%s", s);
      if (upper_bound > 1) {
         for (int ndx=2; ndx <= upper_bound; ndx++) {
            total_successful_attempts += stats_rec->counters[ndx];
            // DBGMSG("ndx=%d", ndx);
            rpt_vstring(d1, "   %2d:  %3d", ndx-1, stats_rec->counters[ndx]);
         }
      }
      assert( ( (upper_bound == 1) && (total_successful_attempts == 0) ) ||
              ( (upper_bound > 1 ) && (total_successful_attempts >  0) )
            );
      rpt_vstring(d1, "Total successful attempts:        %3d", total_successful_attempts);
      rpt_vstring(d1, "Failed due to max tries exceeded: %3d", stats_rec->counters[1]);
      rpt_vstring(d1, "Failed due to fatal error:        %3d", stats_rec->counters[0]);
      rpt_vstring(d1, "Total attempts:                   %3d", total_attempts);
   }

   unlock_if_needed(this_function_performed_lock);
}


#ifdef OLD
void ddc_report_write_read_stats(int depth) {
   try_data_report2(WRITE_READ_TRIES_OP, depth);
}

void ddc_report_write_only_stats(int depth) {
   try_data_report2(WRITE_ONLY_TRIES_OP, depth);
}

/** Reports the statistics for multi-part reads */
void ddc_report_multi_part_read_stats(int depth) {
   try_data_report2(MULTI_PART_READ_OP, depth);
}

/** Reports the statistics for multi-part writes */
void ddc_report_multi_part_write_stats(int depth) {
   try_data_report2(MULTI_PART_WRITE_OP, depth);
}
#endif

/** Reports the current maxtries settings.
 *
 *  \param depth logical indentation depth
 */
void ddc_report_max_tries(int depth) {
   rpt_vstring(depth, "Maximum Try Settings:");
   rpt_vstring(depth, "Operation Type                    Current  Default");
   rpt_vstring(depth, "Write only exchange tries:       %8d %8d",
               try_data_get_maxtries2(WRITE_ONLY_TRIES_OP),
               INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES);
   rpt_vstring(depth, "Write read exchange tries:       %8d %8d",
               try_data_get_maxtries2(WRITE_READ_TRIES_OP),
               INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES);
   rpt_vstring(depth, "Multi-part read exchange tries:  %8d %8d",
               try_data_get_maxtries2(MULTI_PART_READ_OP),
               INITIAL_MAX_MULTI_EXCHANGE_TRIES);
   rpt_vstring(depth, "Multi-part write exchange tries: %8d %8d",
               try_data_get_maxtries2(MULTI_PART_WRITE_OP),
               INITIAL_MAX_MULTI_EXCHANGE_TRIES);
   rpt_nl();
}


/** Reports all DDC level statistics
 * \param depth logical indentation depth
 */
void ddc_report_ddc_stats(int depth) {
   // rpt_nl();
   // retry related stats
   ddc_report_max_tries(depth);
   try_data_report2(WRITE_ONLY_TRIES_OP, depth);   //   ddc_report_write_only_stats(depth);
   try_data_report2(WRITE_READ_TRIES_OP, depth);   //   ddc_report_write_read_stats(depth);
   try_data_report2(MULTI_PART_READ_OP,  depth);   //   ddc_report_multi_part_read_stats(depth);
   try_data_report2(MULTI_PART_WRITE_OP, depth);   //   ddc_report_multi_part_write_stats(depth);
}

