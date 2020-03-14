/** \file thread_retry_data.c
 *
 *  Maintains retry counts and max try settings on a per thread basis.
 */

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "public/ddcutil_status_codes.h"

#include <assert.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/per_thread_data.h"
#include "base/thread_retry_data.h"


//
// Maxtries
//

static char * retry_type_descriptions[] = {
      "write only",
      "write-read",
      "multi-part read",
      "multi-part write"
};

static char * retry_type_names[] = {
      "DDCA_WRITE_ONLY_TRIES",
      "DDCA_WRITE_READ_TRIES",
      "DDCA_MULTI_PART_READ_TRIES",
      "DDCA_MULTI_PART_WRITE_TRIES"
};

const char * retry_type_name(DDCA_Retry_Type type_id) {
   return retry_type_names[type_id];
}

const char * retry_type_description(DDCA_Retry_Type type_id) {
   return retry_type_descriptions[type_id];
}


// Initial values are ddcutil default values, then can be changed
// to different user default values
// But distinction not maxtries values do not vary by thread.!
// duplicate of default_maxtries = ddc_try_stats.c, unify
int default_maxtries[] = {
      INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES,
      INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES };


/** Initializes the retry data section of struct #Per_Thread_Data
 *
 *  \param  data
 */
void init_thread_retry_data(Per_Thread_Data * data) {
   bool debug = false;

   for (int ndx=0; ndx < DDCA_RETRY_TYPE_COUNT; ndx++) {
      DBGMSF(debug, "thread_id %d, retry type: %d, setting current, lowest, highest maxtries to %d ",
                    data->thread_id, ndx, default_maxtries[ndx]);

      data->current_maxtries[ndx] = default_maxtries[ndx];
      data->highest_maxtries[ndx] = default_maxtries[ndx];
      data->lowest_maxtries[ndx]  = default_maxtries[ndx];
   }
   data->thread_retry_data_defined = true;
}


// Just a pass through to ptd_get_thread_retry_Data()
Per_Thread_Data * trd_get_thread_retry_data() {
   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   assert(ptd->thread_retry_data_defined);
   return ptd;
}


/** Sets the maxtries value to be used for a given retry type when creating
 * new #Per_Thread_Data instances.
 *
 * \param  retry_type
 * \param  maxtries   value to set
 */
void trd_set_default_max_tries(DDCA_Retry_Type rcls, uint16_t maxtries) {
   bool debug = false;
   DBGMSF(debug, "Executing. rcls = %s, new_maxtries=%d", retry_type_name(rcls), maxtries);

   default_maxtries[rcls] = maxtries;
}


#ifdef UNFINISHED
void ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = true;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   g_mutex_lock(&thead_retry_data_mutex);
   Maxtries_Rec * mrec = get_thread_maxtries_rec();
   for (DDCA_Retry_Type type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         mrec->maxtries[type_id] = new_max_tries[type_id];
   }
   g_mutex_unlock(&thead_retry_data_mutex);
}
#endif


/** Sets the maxtries value for a specified retry type and the current thread.
 *  The highest_maxtries and lowest_maxtries values are set the same value.
 *
 *  \param retry_type
 *  \param new_maxtries  value to set
 */
void trd_set_initial_thread_max_tries(DDCA_Retry_Type retry_type, uint16_t new_maxtries) {
   bool debug = false;
   ptd_cross_thread_operation_block();
   // bool this_function_locked = ptd_lock_if_unlocked();
   Per_Thread_Data * data = trd_get_thread_retry_data();
   DBGMSF(debug, "Executing. thread: %d, retry_class = %s, new_maxtries=%d",
                 data->thread_id, retry_type_name(retry_type), new_maxtries);
   data->current_maxtries[retry_type] = new_maxtries;
   data->highest_maxtries[retry_type] = new_maxtries;
   data->lowest_maxtries[retry_type]  = new_maxtries;
   // ptd_unlock_if_needed(this_function_locked);
}


/** Sets the maxtries value for a specified retry type and the current thread.
 *
 *  \param retry_type
 *  \param new_maxtries  value to set
 */
void trd_set_thread_max_tries(
      DDCA_Retry_Type       retry_type,
      DDCA_Retry_Count_Type new_maxtries)
{
   bool debug = false;
   ptd_cross_thread_operation_block();

   Per_Thread_Data * tsd = trd_get_thread_retry_data();
   DBGMSF(debug, "Executing. thread_id=%d, retry_class = %s, new_max_tries=%d",
                 tsd->thread_id, retry_type_name(retry_type), new_maxtries);

   tsd->current_maxtries[retry_type] = new_maxtries;

   DBGMSF(debug, "thread id: %d, retry type: %d, per thread data: lowest maxtries: %d, highest maxtries: %d",
         tsd->thread_id,
         retry_type,
         tsd->lowest_maxtries[retry_type],
         tsd->highest_maxtries[retry_type]);

   if (new_maxtries > tsd->highest_maxtries[retry_type])
      tsd->highest_maxtries[retry_type] = new_maxtries;
   if (new_maxtries < tsd->lowest_maxtries[retry_type])
      tsd->lowest_maxtries[retry_type] = new_maxtries;

   DBGMSF(debug, "After adjustment, per thread data: lowest maxtries: %d, highest maxtries: %d",
         tsd->thread_id,
         tsd->lowest_maxtries[retry_type],
         tsd->highest_maxtries[retry_type]);
}


#ifdef UNFINISHED
// sets all at once
void trd_set_thread_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = true;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   Per_Thread_Data * tsd = tsd_get_thread_retry_data();
   tsd->current_maxtries[retry_class] = new_max_tries;
   for (DDCA_Retry_Type type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         tsd->current_maxtries[type_id] = new_max_tries[type_id];
   }
}
#endif


/** Returns the maxtries value for a given retry type and the currently
 *  executing thread.
 *
 *  param  type_id  retry type id
 */
DDCA_Retry_Count_Type trd_get_thread_max_tries(DDCA_Retry_Type type_id) {
   bool debug = false;
   ptd_cross_thread_operation_block();
   Per_Thread_Data * trd = trd_get_thread_retry_data();
   DDCA_Retry_Count_Type result = trd->current_maxtries[type_id];
   DBGMSF(debug, "retry type=%s, returning %d", retry_type_name(type_id), result);
   return result;
}


static void trd_minmax_visitor(Per_Thread_Data * data, void * accumulator) {
   bool debug = false;
   Global_Maxtries_Accumulator * acc = accumulator;

   DBGMSF(debug, "thread id: %d, retry data defined: %s, per thread data: lowest maxtries: %d, highest maxtries: %d",
         data->thread_id,
         sbool( data->thread_retry_data_defined ),
         data->lowest_maxtries[acc->retry_type],
         data->highest_maxtries[acc->retry_type]);
   DBGMSF(debug, "current accumulator: lowest maxtries: %d, highest maxtries: %d",
         acc->min_lowest_maxtries,  acc->max_highest_maxtries);

   assert(data->thread_retry_data_defined);

   if (data->highest_maxtries[acc->retry_type] > acc->max_highest_maxtries)
      acc->max_highest_maxtries = data->highest_maxtries[acc->retry_type];
   if (data->lowest_maxtries[acc->retry_type] < acc->min_lowest_maxtries) {
      // DBGMSF(debug, "lowest maxtries = %d -> %d",
      //        acc->min_lowest_maxtries, data->lowest_maxtries[acc->retry_type]);
      acc->min_lowest_maxtries = data->lowest_maxtries[acc->retry_type];
   }
   DBGMSF(debug, "final accumulator: lowest maxtries: %d, highest maxtries: %d",
          acc->min_lowest_maxtries,  acc->max_highest_maxtries);
}


/** for a given retry type,returns the greatest highest_maxtries and least lowest_maxtries
 *  values found in any #Per_Thread_Data instance
 *
 *  \param  type_id  retry type
 *
 *  \note
 *  Returns actual value on the stack, not a pointer to a value
 */
// Used only as a consistency check vs ddca_try_stats
// This is a multi-Per_Thread_Data function, ptd_apply_all() performs locking
Global_Maxtries_Accumulator
trd_get_all_threads_maxtries_range(DDCA_Retry_Type typeid) {
   Global_Maxtries_Accumulator accumulator;
   accumulator.retry_type = typeid;
   accumulator.max_highest_maxtries = 0;               // less than any valid value
   accumulator.min_lowest_maxtries = MAX_MAX_TRIES+1;  // greater than any valid value
   ptd_apply_all(&trd_minmax_visitor, &accumulator);   // ptd_apply_all() will lock
   return accumulator;
}


/** Output a report of the maxtries data in a #Per_Thread_Data struct,
 *  intended as part of humanly readable program output.
 *
 *  \param  data   pointer to #Per_Thread_Data struct
 *  \param  depth  logical indentation level
 */
static void report_thread_maxtries_data(Per_Thread_Data * data, int depth) {
   assert(data->thread_retry_data_defined);
#ifdef OLD
   if (!data->thread_retry_data_defined) {
      bool debug = false;
       DBGMSF(debug, "==> thread_retry_data_defined not set.  Perform initialization:");
       init_thread_retry_data(data);
       if (debug)
          dbgrpt_per_thread_data(data, 2);
    }
#endif

   ptd_cross_thread_operation_block();
   //bool this_function_locked = ptd_lock_if_unlocked();

   int d1 = depth+1;
   // int d2 = depth+2;
   // rpt_vstring(depth, "Retry data for thread: %3d", data->thread_id);
   // if (data->dref)
   //    rpt_vstring(d1, "Display:                           %s", dref_repr_t(data->dref) );
   rpt_vstring(d1, "Thread Description:                %s", (data->description) ? data->description : "Not set");
   rpt_vstring(d1, "Current maxtries:                  %d,%d,%d,%d",
                    data->current_maxtries[0], data->current_maxtries[1],
                    data->current_maxtries[2], data->current_maxtries[3]);
   rpt_vstring(d1, "Highest maxtries:                  %d,%d,%d,%d",
                    data->highest_maxtries[0], data->highest_maxtries[1],
                    data->highest_maxtries[2], data->highest_maxtries[3]);
   rpt_vstring(d1, "Lowest maxtries:                   %d,%d,%d,%d",
                    data->lowest_maxtries[0], data->lowest_maxtries[1],
                    data->lowest_maxtries[2], data->lowest_maxtries[3]);
   rpt_nl();

   // ptd_unlock_if_needed(this_function_locked);
}


static void wrap_report_thread_maxtries_data(Per_Thread_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   // rpt_vstring(depth, "Per_Thread_Data:");  // needed?
   rpt_vstring(depth, "Thread %d retry data:", data->thread_id);
   report_thread_maxtries_data(data, depth);
   report_thread_all_types_data_by_data(false,    // for_all_threads
                                        data,
                                        depth);
}


/** Report all #Per_Thread_Data structs.  Note that this report includes
 *  structs for threads that have been closed.
 *
 *  \param depth  logical indentation depth
 */
void report_all_thread_maxtries_data(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   rpt_label(depth, "Retry data by thread:");
   assert(per_thread_data_hash);
   ptd_cross_thread_operation_block();
      // bool this_function_locked = ptd_lock_if_unlocked();
      ptd_apply_all_sorted(&wrap_report_thread_maxtries_data, GINT_TO_POINTER(depth+1) );
      // ptd_unlock_if_needed(this_function_locked);
   // rpt_nl();
   // rpt_label(depth, "per thread data structure locks: ");
   // dbgrpt_per_thread_data_locks(depth+1);
   // rpt_nl();

   DBGMSF(debug, "Done");
}

//
// Try Stats
//

// n. caller always locks
static void trd_reset_tries_by_data(Per_Thread_Data * data) {
   ptd_cross_thread_operation_block();
   // bool this_function_locked = ptd_lock_if_unlocked();
   for (int ndx = 0; ndx < DDCA_RETRY_TYPE_COUNT; ndx++) {
     for (int ctrndx = 0; ctrndx < MAX_MAX_TRIES+2; ctrndx++) {
        // counters[ctrndx] = 0;
        data->try_stats[ndx].counters[ctrndx] = 0;

     }
   }
   // ptd_unlock_if_needed(this_function_locked);
}

// reset counts for current thread
void trd_reset_cur_thread_tries() {
   // bool this_function_locked = ptd_lock_if_unlocked();
   ptd_cross_thread_operation_block();
   Per_Thread_Data * data = trd_get_thread_retry_data();
   trd_reset_tries_by_data(data);
   // ptd_unlock_if_needed(this_function_locked);
}


// GFunc signature
static void trd_wrap_reset_tries_by_data(Per_Thread_Data * data, void * arg) {
   trd_reset_tries_by_data(data);
}


void trd_reset_all_threads_tries() {
   // call tries_cur_thread_reset_stats()
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. ");

   if (per_thread_data_hash) {
      ptd_apply_all_sorted(&trd_wrap_reset_tries_by_data, NULL );   // handles locking
   }
   DBGMSF(debug, "Done");
}

#ifdef NO
static void trd_record_cur_thread_successful_tries(DDCA_Retry_Type type_id, int tryct) {
   bool debug = false;
   // DBGMSF(debug, "type_id=%d - %s, tryct=%d",
   //               type_id, retry_type_name(type_id), tryct);
   Per_Thread_Data * data = trd_get_thread_retry_data();

   // Per_Thread_Try_Stats * typedata = &data->try_stats[type_id];
   DBGMSF(debug, "type_id=%d - %s, tryct=%d, Per_Thread_Data: %p",
                 type_id, retry_type_name(type_id), tryct, data);
   // assert(0 < tryct && tryct <= typedata.max_tries);   // max_tries commented out
   // typedata->counters[tryct+1] += 1;
   data->try_stats[type_id].counters[tryct+1]++;
   DBGMSF(debug, "new counters value: %d", data->try_stats[type_id].counters[tryct+1]);
}

static void trd_record_cur_thread_failed_max_tries(DDCA_Retry_Type type_id) {
   Per_Thread_Data * data = trd_get_thread_retry_data();
   data->try_stats[type_id].counters[1]++;

}

static void trd_record_cur_thread_failed_fatally(DDCA_Retry_Type type_id) {
   Per_Thread_Data * data = trd_get_thread_retry_data();
   data->try_stats[type_id].counters[0]++;
}
#endif

void trd_record_cur_thread_tries(DDCA_Retry_Type type_id, int rc, int tryct) {
   bool debug = false;
   DBGMSF(debug, "Executing. type_id=%d=%s, rc=%d, tryct=%d",
                 type_id, retry_type_name(type_id), rc, tryct);

   ptd_cross_thread_operation_block();
   // bool this_function_locked = ptd_lock_if_unlocked();
   Per_Thread_Data * data = trd_get_thread_retry_data();
   // Per_Thread_Try_Stats* typedata = &data->try_stats[type_id];
   int index = 0;
   if (rc == 0) {
      index = tryct+1;
   }
   // fragile, but eliminates testing for max_tries:
   else if (rc == DDCRC_RETRIES || rc == DDCRC_ALL_TRIES_ZERO) {
      index = 1;
   }
   else {  // failed fatally
      index = 0;
   }
   data->try_stats[type_id].counters[index]+= 1;
   //typedata->counters[index] +=1;
   // ptd_unlock_if_needed(this_function_locked);
}


int get_thread_total_tries_for_one_type_by_data(DDCA_Retry_Type retry_type, Per_Thread_Data  * data) {
   ptd_cross_thread_operation_block();

   int total_attempts = 0;
   for (int counter_ndx = 0; counter_ndx < MAX_MAX_TRIES + 2; counter_ndx++) {
      total_attempts += data->try_stats[retry_type].counters[counter_ndx];
   }
   return total_attempts;
}


/** Calculates the total number of tries for all exchange type on a single thread.
 *
 * \param  data  per-thread data record
 * \param  return total attempts for all exchange types
 */
int get_thread_total_tries_for_all_types_by_data(Per_Thread_Data  * data) {
   ptd_cross_thread_operation_block();
   // bool this_function_locked = ptd_lock_if_unlocked();

   int total_attempts = 0;
   for (int typendx = 0; typendx < DDCA_RETRY_TYPE_COUNT; typendx++) {
      // Per_Thread_Try_Stats * typedata = &data->try_stats[typendx];
      for (int counter_ndx = 0; counter_ndx < MAX_MAX_TRIES + 2; counter_ndx++) {
         // total_attempts += typedata->counters[counter_ndx];
         total_attempts += data->try_stats[typendx].counters[counter_ndx];
      }
   }
   // ptd_unlock_if_needed(this_function_locked);

   return total_attempts;
}


/** Determines the index of the highest try counter, i.e. other than 0 or 1,  with a non-zero value.
 *
 *  \ param upper bound
 */
uint16_t index_of_highest_non_zero_counter(uint16_t* counters) {
   int result = 1;
   for (int kk = MAX_MAX_TRIES+1; kk > 1; kk--) {
      if (counters[kk] != 0) {
         result = kk;
         break;
      }
   }
   // DBGMSG("Returning: %d", result);
   return result;
}


/** Reports a single type of transaction (write-only, write-read, etc.
 *  for a given thread.
 *
 *  This method is also used to report summary data stored in a
 *  summary thread.
 *
 *  \param  retry_type       type of transaction being reported
 *  \param  for_all_threads  indicates whether this call is for a real thread,
 *                           or for a synthesized data record containing data that
 *                           summarizes all threads
 *  \param  data             pointer to per-thread data
 *  \param  depth            logical indentation depth
 */
void report_thread_try_typed_data_by_data(
      DDCA_Retry_Type     retry_type,
      bool                for_all_threads_total,
      Per_Thread_Data *   data,
      int                 depth)
{
   bool debug = false;
   int d1 = depth+1;
   int d2 = depth+2;
   // rpt_nl();
   assert( ( (retry_type == -1) &&  for_all_threads_total) ||
           ((retry_type != -1) && !for_all_threads_total) );
   // bool this_function_locked = ptd_lock_if_unlocked();
   if (for_all_threads_total) {  // reporting a synthesized summary record
      rpt_vstring(depth, "Total %s retry statistics for all threads", retry_type_name(retry_type) );
   }
   else {      // normal case, reporting one thread
      rpt_vstring(depth, "Thread %d %s retry statistics",
                         data->thread_id, retry_type_name(retry_type));
   }

   if (debug) {
      int upper_bound = data->highest_maxtries[retry_type] + 1;
      assert(upper_bound <= MAX_MAX_TRIES + 1);
      char * buf = int_array_to_string( data->try_stats[retry_type].counters, upper_bound);
      rpt_vstring(d1, "try_stats[%d=%-27s].counters = %s",
                      retry_type, retry_type_name(retry_type), buf);
      free(buf);
   }

   int total_attempts_for_one_type =  get_thread_total_tries_for_one_type_by_data(retry_type, data);
   if ( total_attempts_for_one_type == 0) {
      rpt_vstring(d1, "No tries attempted");
   }
   else {     //          Per_Thread_Try_Stats  try_stats[4];
      Per_Thread_Try_Stats*  typedata =& data->try_stats[ retry_type ];

     int maxtries_lower_bound =  data->lowest_maxtries[retry_type];
     int maxtries_upper_bound =  data->highest_maxtries[retry_type];

      if (maxtries_lower_bound == maxtries_upper_bound)
         rpt_vstring(d1, "Max tries allowed:  %d", maxtries_lower_bound);
      else
         rpt_vstring(d1, "Max tries allowed:  %d .. %d", maxtries_lower_bound, maxtries_upper_bound);

      int    last_index1 = MAX_MAX_TRIES + 1;
      int    last_index2 = maxtries_upper_bound + 1;
      int    last_index3 =  index_of_highest_non_zero_counter(data->try_stats[retry_type].counters);
      // dbgrpt_per_thread_data(data, 2);
      DBGMSF(debug, "MAX_MAX_TRIES+1:        %d", last_index1);
      DBGMSF(debug, "maxtries upper bound    %d", last_index2);
      DBGMSF(debug, "highest non-zero index: %d", last_index3);
      assert(last_index1 >= last_index2);
      assert(last_index2 >= last_index3);

      int total_successful_attempts = 0;
      for (int ndx = 2; ndx <= last_index3; ndx++)
         total_successful_attempts += typedata->counters[ndx];
      int all_attempts = total_successful_attempts + typedata->counters[0] + typedata->counters[1];

      assert(all_attempts == total_attempts_for_one_type);

      bool force_detail = false;     // true for debugging

      if (all_attempts == 0 && !force_detail) {
         rpt_vstring(d1, "Total attempts   %2d", all_attempts);
      }
      else {
         rpt_vstring(d1, "Successful attempts by number of tries required:");

         if (last_index3 <= 1)
            rpt_label(d2, " None");
         else {
            for (int ndx=2; ndx <= last_index3; ndx++) {
               rpt_vstring(d2, "%2d:  %3d", ndx-1, typedata->counters[ndx]);
            }
         }
         // ptd_unlock_if_needed(this_function_locked);

         rpt_vstring(d1, "Total successful:                 %3d", total_successful_attempts);
         rpt_vstring(d1, "Failed due to max tries exceeded: %3d", typedata->counters[1]);
         rpt_vstring(d1, "Failed due to fatal error:        %3d", typedata->counters[0]);
         rpt_vstring(d1, "Total attempts:                   %3d", total_attempts_for_one_type);
      }
   }
   rpt_nl();
}


/** Reports all try statistics for a single thread
 *
 * \param  for_all_threads  indicates whether this call is for a real thread,
 *                          of for a synthesized data record containing data that
 *                          summarizes all threads
 * \param   data            pointer to record for one thread
 * \param   depth           logical indentation depth
 */
void report_thread_all_types_data_by_data(
      bool                for_all_threads,   // controls message
      Per_Thread_Data   * data,
      int                 depth)
{
   // rpt_vstring(depth, "Tries data for thread %d", data->thread_id);
   // for WRITE_ONLY, WRITE_READ, etc.
   for (int try_type_ndx = 0; try_type_ndx < DDCA_RETRY_TYPE_COUNT; try_type_ndx++) {
      DDCA_Retry_Type type_id = (DDCA_Retry_Type) try_type_ndx;
      report_thread_try_typed_data_by_data( type_id, for_all_threads, data, depth+1);
      // rpt_nl();
   }
}


