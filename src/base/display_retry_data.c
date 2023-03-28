/** \file display_retry_data.c
 *
 *  Maintains retry counts and max try settings on a per thread basis.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "public/ddcutil_status_codes.h"

#include <assert.h>
#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/stats.h"

#include "base/display_retry_data.h"

//
// Maxtries
//

// Initial values are ddcutil default values, then can be changed
// to different user default values
// But distinction not maxtries values do not vary by thread.!
// duplicate of default_maxtries = ddc_try_stats.c, unify
static int default_maxtries[] = {
      INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES,
      INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES };


/** Initializes the retry data section of struct #Per_Display_Data
 *
 *  \param  data
 */
void drd_init_display_data(Per_Display_Data * data) {
   bool debug = false;

   for (int ndx=0; ndx < RETRY_OP_COUNT; ndx++) {
      DBGMSF(debug, "dpath =%s, retry type: %d, setting current, lowest, highest maxtries to %d ",
                    data->dpath, ndx, default_maxtries[ndx]);
#ifdef PER_DISPLAY_MAXTRIES
      data->current_maxtries[ndx] = default_maxtries[ndx];
      data->highest_maxtries[ndx] = default_maxtries[ndx];
      data->lowest_maxtries[ndx]  = default_maxtries[ndx];
#endif

      data->try_stats[0].retry_op = WRITE_ONLY_TRIES_OP;
      data->try_stats[1].retry_op = WRITE_READ_TRIES_OP;
      data->try_stats[2].retry_op = MULTI_PART_READ_OP;
      data->try_stats[3].retry_op = MULTI_PART_WRITE_OP;
   }
}


#ifdef UNUSED
// Just a pass through to pdd_get_display_data()
// Vestige of having separate struct for retry data alone
Per_Display_Data * drd_get_display_retry_data(DDCA_IO_Path dpath) {
   Per_Display_Data * pdd = pdd_get_per_display_data(dpath, true);
   return pdd;
}
#endif


/** Sets the maxtries value to be used for a given retry type when creating
 * new #Per_Display_Data instances.
 *
 * \param  retry_type
 * \param  maxtries   value to set
 */
void drd_set_default_max_tries(Retry_Operation rcls, uint16_t maxtries) {
   bool debug = false;
   DBGMSF(debug, "Executing. rcls = %s, new_maxtries=%d", retry_type_name(rcls), maxtries);

   default_maxtries[rcls] = maxtries;
}


#define GLOBAL_MAXTRIES_MARKER "GLMX"
typedef
struct {
   char marker[4];
   Retry_Operation rcls;
   uint16_t        maxtries;
} Global_Maxtries_Args;

#ifdef PER_DISPLAY_MAXTRIES
// satisfies GFunc
void display_global_maxtries_func(
      Per_Display_Data *   pdd,
      gpointer  user_data)
{
   bool debug = false;

   Global_Maxtries_Args * args = user_data;
   assert(memcmp(args->marker, GLOBAL_MAXTRIES_MARKER, 4) == 0);
   DBGMSF(debug, "thread = %d, rcls = %s, maxtries: %d",
                  pdd->dpath, retry_type_name(args->rcls), args->maxtries);

  if (pdd->lowest_maxtries[args->rcls] > args->maxtries)
      pdd->lowest_maxtries[args->rcls] = args->maxtries;
  if (pdd->highest_maxtries[args->rcls] < args->maxtries)
      pdd->highest_maxtries[args->rcls] = args->maxtries;
}
#endif

#ifdef PER_DISPLAY_MAXTRIES
/**
 *  For a given retry_type, sets
 *  - the default maxtries value for new threads
 *  - the maxtries value for each existing thread
 *
 *   \pararm      retry_type
 *   \param       new_maxtries
 */
void drd_set_all_maxtries(Retry_Operation rcls, uint16_t maxtries) {
   bool debug = false;
   DBGMSF(debug, "Executing. rcls = %s, new_maxtries=%d", retry_type_name(rcls), maxtries);

   default_maxtries[rcls] = maxtries;
   // done by caller
   // if  (rcls == MULTI_PART_READ_OP)
   //    default_maxtries[MULTI_PART_WRITE_OP] = maxtries;

   Global_Maxtries_Args args;
   memcpy(args.marker, GLOBAL_MAXTRIES_MARKER, 4);
   args.rcls = rcls;
   args.maxtries = maxtries;

   pdd_apply_all(display_global_maxtries_func, &args);
}
#endif


#ifdef UNFINISHED
void ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = false;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   g_mutex_lock(&thead_retry_data_mutex);
   Maxtries_Rec * mrec = get_display_maxtries_rec();
   for (Retry_Operation type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         mrec->maxtries[type_id] = new_max_tries[type_id];
   }
   g_mutex_unlock(&thead_retry_data_mutex);
}
#endif

#ifdef PTD
/** Sets the initial maxtries value for a specified retry type and the current thread.
 *  The highest_maxtries and lowest_maxtries values are set to the same value.
 *
 *  \param retry_type
 *  \param new_maxtries  value to set
 */
void drd_set_initial_display_max_tries(DDCA_IO_Path dpath, Retry_Operation retry_type, uint16_t new_maxtries) {
   bool debug = false;
   pdd_cross_display_operation_block();
   // bool this_function_locked = pdd_lock_if_unlocked();
   Per_Display_Data * data = drd_get_display_retry_data(dpath);
   DBGMSF(debug, "Executing. dpath: %s, retry_class = %s, new_maxtries=%d",
                 data->dpath, retry_type_name(retry_type), new_maxtries);
   data->current_maxtries[retry_type] = new_maxtries;
   data->highest_maxtries[retry_type] = new_maxtries;
   data->lowest_maxtries[retry_type]  = new_maxtries;
   // pdd_unlock_if_needed(this_function_locked);
}
#endif

#ifdef PER_DISPLAY_MAXTRIES
/** Sets the maxtries value for a specified retry type and display
 *
 *  \param dpath         device io path
 *  \param retry_type    retry type, WRITE_ONLY_TRIES_OP etc.
 *  \param new_maxtries  value to set
 */
void drd_set_display_max_tries(
      DDCA_IO_Path     dpath,
      Retry_Operation  retry_type,
      Retry_Op_Value   new_maxtries)
{
   bool debug = false;
   pdd_cross_display_operation_block(__func__);

   Per_Display_Data * drd = drd_get_display_retry_data(dpath);
   DBGMSF(debug, "Executing. display_id=%d, retry_class = %s, new_max_tries=%d",
                 drd->dpath, retry_type_name(retry_type), new_maxtries);

   drd->current_maxtries[retry_type] = new_maxtries;

   DBGMSF(debug, "thread id: %d, retry type: %d, per thread data: lowest maxtries: %d, highest maxtries: %d",
         drd->dpath,
         retry_type,
         drd->lowest_maxtries[retry_type],
         drd->highest_maxtries[retry_type]);

   if (new_maxtries > drd->highest_maxtries[retry_type])
      drd->highest_maxtries[retry_type] = new_maxtries;
   if (new_maxtries < drd->lowest_maxtries[retry_type])
      drd->lowest_maxtries[retry_type] = new_maxtries;

   DBGMSF(debug, "After adjustment, per thread data: lowest maxtries: %d, highest maxtries: %d",
         drd->dpath,
         drd->lowest_maxtries[retry_type],
         drd->highest_maxtries[retry_type]);
}


#ifdef UNFINISHED
// sets all at once
void drd_set_display_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]) {
   bool debug = false;
   DBGMSF(debug, "Executing. new_max_tries = [%d,%d,%d]",
                 new_max_tries[0], new_max_tries[1], new_max_tries[2] );
   Per_Display_Data * drd = drd_get_display_retry_data();
   drd->current_maxtries[retry_class] = new_max_tries;
   for (Retry_Operation type_id = 0; type_id < RETRY_TYPE_COUNT; type_id++) {
      if (new_max_tries[type_id] > 0)
         drd->current_maxtries[type_id] = new_max_tries[type_id];
   }
}
#endif


/** Returns the maxtries value for a given retry type and display.
 *
 *  @param  dpath    display io path
 *  @param  type_id  retry type id
 */
Retry_Op_Value drd_get_display_max_tries(DDCA_IO_Path dpath, Retry_Operation type_id) {
   bool debug = false;
   pdd_cross_display_operation_block(__func__);
   Per_Display_Data * pdd = drd_get_display_retry_data(dpath);
   Retry_Op_Value result = pdd->current_maxtries[type_id];
   DBGMSF(debug, "dpath=%s, retry type=%s, returning %d", dpath_repr_t(&dpath), retry_type_name(type_id), result);
   return result;
}


static void drd_minmax_visitor(Per_Display_Data * data, void * accumulator) {
   bool debug = false;
   Global_Maxtries_Accumulator * acc = accumulator;

   DBGMSF(debug, "device %s, retry data defined: %s, per thread data: lowest maxtries: %d, highest maxtries: %d",
         dpath_repr_t(&data->dpath),
         sbool( data->display_retry_data_defined ),
         data->lowest_maxtries[acc->retry_type],
         data->highest_maxtries[acc->retry_type]);
   DBGMSF(debug, "current accumulator: lowest maxtries: %d, highest maxtries: %d",
         acc->min_lowest_maxtries,  acc->max_highest_maxtries);

   assert(data->display_retry_data_defined);

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


/** for a given retry type, returns the greatest highest_maxtries and least lowest_maxtries
 *  values found in any #Per_Display_Data instance
 *
 *  \param  type_id  retry type
 *  \return #Global_Maxtries_Accumulator struct
 *
 *  \note
 *  Returns actual value on the stack, not a pointer to a value
 */
// Used only as a consistency check vs ddca_try_stats
// This is a multi-Per_Display_Data function, pdd_apply_all() performs locking
Global_Maxtries_Accumulator
drd_get_all_displays_maxtries_range(Retry_Operation typeid) {
   Global_Maxtries_Accumulator accumulator;
   accumulator.retry_type = typeid;
   accumulator.max_highest_maxtries = 0;               // less than any valid value
   accumulator.min_lowest_maxtries = MAX_MAX_TRIES+1;  // greater than any valid value
   pdd_apply_all(&drd_minmax_visitor, &accumulator);   // pdd_apply_all() will lock
   return accumulator;
}
#endif

#ifdef PER_DISPLAY_MAXTRIES
/** Output a report of the maxtries data in a #Per_Display_Data struct,
 *  intended as part of humanly readable program output.
 *
 *  \param  data   pointer to #Per_Display_Data struct
 *  \param  depth  logical indentation level
 */
static void report_display_maxtries_data(Per_Display_Data * data, int depth) {
   assert(data->display_retry_data_defined);

   pdd_cross_display_operation_block(__func__);
   //bool this_function_locked = pdd_lock_if_unlocked();

   int d1 = depth+1;
   // int d2 = depth+2;
   // rpt_vstring(depth, "Retry data for thread: %3d", data->thread_id);
   // if (data->dref)
   //    rpt_vstring(d1, "Display:                           %s", dref_repr_t(data->dref) );
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

   // pdd_unlock_if_needed(this_function_locked);
}
#endif


static void wrap_report_display_retry_data(Per_Display_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   // rpt_vstring(depth, "Per_Display_Data:");  // needed?
   rpt_vstring(depth, "Retry data for display on %s:", dpath_short_name_t(&data->dpath));
#ifdef PER_DISPLAY_MAXTRIES
   report_display_maxtries_data(data, depth);
#endif
   report_display_all_types_data_by_data(false,    // for_all_threads
                                        data,
                                        depth);
}

/** Report all #Per_Display_Data structs.  Note that this report includes
 *  structs for threads that have been closed.
 *
 *  \param depth  logical indentation depth
 */
void drd_report_all_display_retry_data(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   rpt_label(depth, "Per display retry data");
   assert(per_display_data_hash);
   pdd_cross_display_operation_block(__func__);
      // bool this_function_locked = pdd_lock_if_unlocked();
      pdd_apply_all_sorted(&wrap_report_display_retry_data, GINT_TO_POINTER(depth+1) );
      // pdd_unlock_if_needed(this_function_locked);
   // rpt_nl();
   // rpt_label(depth, "per thread data structure locks: ");
   // dbgrpt_per_display_data_locks(depth+1);
   rpt_nl();

   DBGMSF(debug, "Done");
}


//
// Try Stats
//

#ifdef UNUSED
// n. caller always locks
static void drd_reset_tries_by_data(Per_Display_Data * data) {
   // pdd_cross_display_operation_block();
   // bool this_function_locked = pdd_lock_if_unlocked();
   for (int ndx = 0; ndx < RETRY_OP_COUNT; ndx++) {
      for (int ctrndx = 0; ctrndx < MAX_MAX_TRIES+2; ctrndx++) {
         // counters[ctrndx] = 0;
         data->try_stats[ndx].counters[ctrndx] = 0;
      }
   }
   // pdd_unlock_if_needed(this_function_locked);
}

// reset counts for current display
void drd_reset_display_tries(Per_Display_Data * pdd) {
   // bool this_function_locked = pdd_lock_if_unlocked();
   // pdd_cross_display_operation_block();
   // Per_Display_Data * data = drd_get_display_retry_data();
   drd_reset_tries_by_data(pdd);
   // pdd_unlock_if_needed(this_function_locked);
}

// GFunc signature
static void drd_wrap_reset_tries_by_data(Per_Display_Data * data, void * arg) {
   drd_reset_tries_by_data(data);
}

void drd_reset_all_displays_tries() {
   // call tries_cur_display_reset_stats()
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. ");

   if (per_display_data_hash) {
      pdd_apply_all_sorted(&drd_wrap_reset_tries_by_data, NULL );   // handles locking
   }
   DBGMSF(debug, "Done");
}
#endif

// static
void drd_record_display_successful_tries(Per_Display_Data * data, Retry_Operation type_id, int tryct) {
   bool debug = false;
   // DBGMSF(debug, "type_id=%d - %s, tryct=%d",
   //               type_id, retry_type_name(type_id), tryct);
   // Per_Display_Data * data = drd_get_display_retry_data();

   // Per_Display_Try_Stats * typedata = &data->try_stats[type_id];
   DBGMSF(debug, "type_id=%d - %s, tryct=%d, Per_Display_Data: %p",
                 type_id, retry_type_name(type_id), tryct, data);
   // assert(0 < tryct && tryct <= typedata.max_tries);   // max_tries commented out
   // typedata->counters[tryct+1] += 1;
   data->try_stats[type_id].counters[tryct+1]++;
   DBGMSF(debug, "new counters value: %d", data->try_stats[type_id].counters[tryct+1]);
}

// static
void drd_record_display_failed_max_tries(Per_Display_Data * data, Retry_Operation type_id) {
   // Per_Display_Data * data = drd_get_display_retry_data();
   data->try_stats[type_id].counters[1]++;
}

// static
void drd_record_display_failed_fatally(Per_Display_Data * data, Retry_Operation type_id) {
   // Per_Display_Data * data = drd_get_display_retry_data();
   data->try_stats[type_id].counters[0]++;
}

void drd_record_display_tries(Per_Display_Data * pdd, Retry_Operation type_id, int rc, int tryct) {
   bool debug = false;
   DBGMSF(debug, "Executing. %s type_id=%d=%s, rc=%d, tryct=%d",
                  dpath_repr_t(&pdd->dpath),
                 type_id, retry_type_name(type_id), rc, tryct);

   // pdd_cross_display_operation_block();

   // bool this_function_locked = pdd_lock_if_unlocked();
   // Per_Display_Data * data = drd_get_display_retry_data();
   // Per_Display_Try_Stats* typedata = &data->try_stats[type_id];
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
   pdd->try_stats[type_id].counters[index]+= 1;
   //typedata->counters[index] +=1;
   // pdd_unlock_if_needed(this_function_locked);
}


int get_display_total_tries_for_one_type_by_data(Retry_Operation retry_type, Per_Display_Data  * data) {
   pdd_cross_display_operation_block(__func__);

   int total_attempts = 0;
   for (int counter_ndx = 0; counter_ndx < MAX_MAX_TRIES + 2; counter_ndx++) {
      total_attempts += data->try_stats[retry_type].counters[counter_ndx];
   }
   return total_attempts;
}


#ifdef UNUSED
/** Calculates the total number of tries for all exchange type on a single thread.
 *
 * \param  data  per-thread data record
 * \param  return total attempts for all exchange types
 */
int get_display_total_tries_for_all_types_by_data(Per_Display_Data  * data) {
   pdd_cross_display_operation_block();
   // bool this_function_locked = pdd_lock_if_unlocked();

   int total_attempts = 0;
   for (int typendx = 0; typendx < RETRY_OP_COUNT; typendx++) {
      // Per_Display_Try_Stats * typedata = &data->try_stats[typendx];
      for (int counter_ndx = 0; counter_ndx < MAX_MAX_TRIES + 2; counter_ndx++) {
         // total_attempts += typedata->counters[counter_ndx];
         total_attempts += data->try_stats[typendx].counters[counter_ndx];
      }
   }
   // pdd_unlock_if_needed(this_function_locked);

   return total_attempts;
}
#endif


/** Determines the index of the highest try counter for a given operation,
 *  i.e. other than 0 or 1, with a non-zero value, i.e. the highest try
 *  count needed to successfully perform the operation.
 *
 *  @param try count table
 *  @return highest try count for successful requests
 */
uint16_t display_index_of_highest_non_zero_counter(uint16_t* counters) {
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
 *  for a given display.
 *
 *  This function is also used to report summary data stored in a
 *  summary Per_Display instance.
 *
 *  \param  retry_type       type of transaction being reported
 *  \param  for_all_threads  indicates whether this call is for a real display,
 *                           or for a synthesized data record containing data that
 *                           summarizes all displays
 *  \param  data             pointer to per-thread data
 *  \param  depth            logical indentation depth
 */
void report_display_try_typed_data_by_data(
      Retry_Operation     retry_type,
      bool                for_all_threads_total,
      Per_Display_Data *  data,
      int                 depth)
{
   // bool debug = false;
   int d1 = depth+1;
   int d2 = depth+2;
   // rpt_nl();
   ASSERT_IFF( (retry_type == -1), for_all_threads_total );
   // bool this_function_locked = pdd_lock_if_unlocked();

   int total_attempts_for_one_type =  get_display_total_tries_for_one_type_by_data(retry_type, data);

   if (for_all_threads_total) {  // reporting a synthesized summary record
      rpt_vstring(depth, "Total %s retry statistics for all displays", retry_type_name(retry_type) );
   }
   else {      // normal case, reporting one thread
      if (total_attempts_for_one_type)
         rpt_vstring(depth, "Retry data for %s tries",
                         retry_type_description(retry_type));
      else
         rpt_vstring(depth, "Retry data for %s tries: No tries attempted",
               retry_type_description(retry_type));
   }

#ifdef OUT     // only use of highest_maxtries
   if (debug) {
      int upper_bound = data->highest_maxtries[retry_type] + 1;
      assert(upper_bound <= MAX_MAX_TRIES + 1);
      char * buf = int_array_to_string( data->try_stats[retry_type].counters, upper_bound);
      rpt_vstring(d1, "try_stats[%d=%-27s].counters = %s",
                      retry_type, retry_type_name(retry_type), buf);
      free(buf);
   }
#endif


   if ( total_attempts_for_one_type == 0) {
    //   rpt_vstring(d1, "No tries attempted");
   }
   else {     //          Per_Display_Try_Stats  try_stats[4];
      Per_Display_Try_Stats*  typedata =& data->try_stats[ retry_type ];

#ifdef PER_DISPLAY_MAXTRIES
     int maxtries_lower_bound =  data->lowest_maxtries[retry_type];
     int maxtries_upper_bound =  data->highest_maxtries[retry_type];

      if (maxtries_lower_bound == maxtries_upper_bound)
         rpt_vstring(d1, "Max tries allowed:  %d", maxtries_lower_bound);
      else
         rpt_vstring(d1, "Max tries allowed:  %d .. %d", maxtries_lower_bound, maxtries_upper_bound);

      int    last_index1 = MAX_MAX_TRIES + 1;
      int    last_index2 = maxtries_upper_bound + 1;
#endif
      int    last_index3 =  display_index_of_highest_non_zero_counter(data->try_stats[retry_type].counters);
#ifdef PER_DISPLAY_MAXTRIES
      // dbgrpt_per_display_data(data, 2);
      DBGMSF(debug, "MAX_MAX_TRIES+1:        %d", last_index1);
      DBGMSF(debug, "maxtries upper bound    %d", last_index2);
      DBGMSF(debug, "highest non-zero index: %d", last_index3);
      assert(last_index1 >= last_index2);
      assert(last_index2 >= last_index3);
#endif

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
         // pdd_unlock_if_needed(this_function_locked);

         rpt_vstring(d1, "Total successful:                 %3d", total_successful_attempts);
         rpt_vstring(d1, "Failed due to max tries exceeded: %3d", typedata->counters[1]);
         rpt_vstring(d1, "Failed due to fatal error:        %3d", typedata->counters[0]);
         rpt_vstring(d1, "Total attempts:                   %3d", total_attempts_for_one_type);
      }
   }
   rpt_nl();
}


/** Reports all try statistics for a single display
 *
 * \param  for_all_threads  indicates whether this call is for a real display,
 *                          or for a synthesized data record containing data that
 *                          summarizes all threads
 * \param   data            pointer to record for one thread
 * \param   depth           logical indentation depth
 */
void report_display_all_types_data_by_data(
      bool              for_all_displays,   // controls message
      Per_Display_Data* data,
      int               depth)
{
   // rpt_vstring(depth, "Tries data for thread %d", data->thread_id);
   // for DD_WRITE_ONLY, DD_WRITE_READ, etc.
   for (int try_type_ndx = 0; try_type_ndx < RETRY_OP_COUNT; try_type_ndx++) {
      Retry_Operation type_id = (Retry_Operation) try_type_ndx;
      report_display_try_typed_data_by_data( type_id, for_all_displays, data, depth+1);
      // rpt_nl();
   }
}
