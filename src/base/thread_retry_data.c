/** \file thread_retry_data.c
 *
 *  Maintains retry counts and max try settings on a per thread basis.
 */

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/per_thread_data.h"
#include "base/thread_retry_data.h"


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

const char * retry_type_name(DDCA_Retry_Type type_id) {
   return retry_class_names[type_id];
}

const char * retry_type_description(DDCA_Retry_Type type_id) {
   return retry_class_descriptions[type_id];
}

// duplicate of default_maxtries = ddc_try_stats.c, unify
int default_maxtries[] = {
      INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES,
      INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES,
      INITIAL_MAX_MULTI_EXCHANGE_TRIES };


void init_thread_retry_data(Per_Thread_Data * data) {

   for (int ndx=0; ndx < DDCA_RETRY_TYPE_COUNT; ndx++) {
      DBGMSG("thread_id %d, retry type: %d, setting current, lowest, highest maxtries to %d ",
             data->thread_id, ndx, default_maxtries[ndx]);

      data->current_maxtries[ndx] = default_maxtries[ndx];
      data->highest_maxtries[ndx] = default_maxtries[ndx];
      data->lowest_maxtries[ndx]  = default_maxtries[ndx];
   }
   data->thread_retry_data_defined = true;
}


Per_Thread_Data * trd_get_thread_retry_data() {
   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   if (!ptd->thread_retry_data_defined) {
      // DBGMSG("thread_retry_data_defined false");
      init_thread_retry_data(ptd);
      // dbgrpt_per_thread_data(ptd, 2);
   }
   return ptd;
}




void trd_set_default_max_tries(DDCA_Retry_Type rcls, uint16_t new_maxtries) {
   bool debug = false;
   DBGMSF(debug, "Executing. rcls = %s, new_maxtries=%d", retry_type_name(rcls), new_maxtries);
   g_mutex_lock(&per_thread_data_mutex);
   default_maxtries[rcls] = new_maxtries;
   g_mutex_unlock(&per_thread_data_mutex);
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


void trd_set_initial_thread_max_tries(DDCA_Retry_Type retry_type, uint16_t new_maxtries) {
   Per_Thread_Data * data = trd_get_thread_retry_data();
   bool debug = true;
   DBGMSF(debug, "Executing. thread: %d, retry_class = %s, new_maxtries=%d",
                 data->thread_id, retry_type_name(retry_type), new_maxtries);
   data->current_maxtries[retry_type] = new_maxtries;
   data->highest_maxtries[retry_type] = new_maxtries;
   data->lowest_maxtries[retry_type]  = new_maxtries;
}

void trd_set_thread_max_tries(DDCA_Retry_Type retry_type, uint16_t new_maxtries) {
   bool debug = false;
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


uint16_t trd_get_thread_max_tries(DDCA_Retry_Type type_id) {
   bool debug = false;
   Per_Thread_Data * tsd = trd_get_thread_retry_data();
   uint16_t result = tsd->current_maxtries[type_id];
   DBGMSF(debug, "retry type=%s, returning %d", retry_type_name(type_id), result);
   return result;
}



void trd_minmax_visitor(Per_Thread_Data * data, void * accumulator) {
   bool debug = true;
   Global_Maxtries_Accumulator * acc = accumulator;
      DBGMSF(debug, "thread id: %d, retry data defined: %s, per thread data: lowest maxtries: %d, highest maxtries: %d",
            data->thread_id,
            sbool( data->thread_retry_data_defined ),
            data->lowest_maxtries[acc->retry_type],
            data->highest_maxtries[acc->retry_type]);
      DBGMSF(debug, "initial accumulator: lowest maxtries: %d, highest maxtries: %d",
            acc->min_lowest_maxtries,  acc->max_highest_maxtries);
   if (!data->thread_retry_data_defined) {
      DBGMSG("Delayed initialization:");
      init_thread_retry_data(data);
      dbgrpt_per_thread_data(data, 2);
   }
   if (data->highest_maxtries[acc->retry_type] > acc->max_highest_maxtries)
      acc->max_highest_maxtries = data->highest_maxtries[acc->retry_type];
   if (data->lowest_maxtries[acc->retry_type] < acc->min_lowest_maxtries) {
      DBGMSG("lowest maxtries = %d -> %d",
             acc->min_lowest_maxtries, data->lowest_maxtries[acc->retry_type]);
      acc->min_lowest_maxtries = data->lowest_maxtries[acc->retry_type];
   }
   DBGMSF(debug, "final accumulator: lowest maxtries: %d, highest maxtries: %d",
          acc->min_lowest_maxtries,  acc->max_highest_maxtries);
}


// void max_all_thread_highest_maxtries

// n returns value on stack, not pointer
Global_Maxtries_Accumulator trd_get_all_threads_maxtries_range(DDCA_Retry_Type typeid) {
   Global_Maxtries_Accumulator accumulator;
   accumulator.retry_type = typeid;
   accumulator.max_highest_maxtries = 0;               // less than any valid value
   accumulator.min_lowest_maxtries = MAX_MAX_TRIES+1;  // greater than any valid value

   ptd_apply_all(&trd_minmax_visitor, &accumulator);

   return accumulator;
}


/** Output a report of the retry data in a #Per_Thread_Data struct,
 *  intended as part of humanly readable program output.
 *
 *  \param  data   pointer to #Per_Thread_Data struct
 *  \param  depth  logical indentation level
 */
void report_thread_retry_data(Per_Thread_Data * data, int depth) {
   int d1 = depth+1;
   // int d2 = depth+2;
   rpt_vstring(depth, "Retry data for thread: %3d", data->thread_id);
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


void wrap_report_thread_retry_data(Per_Thread_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   rpt_vstring(depth, "Per_Thread_Data:");  // needed?
   report_thread_retry_data(data, depth);
}


/** Report all #Per_Thread_Data structs.  Note that this report includes
 *  structs for threads that have been closed.
 *
 *  \param depth  logical indentation depth
 */
void report_all_thread_retry_data(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   if (!per_thread_data_hash) {
      rpt_vstring(depth, "No thread retry data found");
   }
   else {
      ptd_apply_all_sorted(&wrap_report_thread_retry_data, GINT_TO_POINTER(depth) );
   }
   rpt_nl();
   DBGMSF(debug, "Done");
}





