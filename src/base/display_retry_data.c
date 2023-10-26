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


static void wrap_report_display_retry_data(Per_Display_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   rpt_vstring(depth, "Retry data for display on %s:", dpath_short_name_t(&data->dpath));
   report_display_all_types_data_by_data(false,    // for_all_displays
                                        data,
                                        depth);
}


/** Report all #Per_Display_Data structs.  Note that this report includes
 *  structs for displays that may have been disconnected.
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

#ifdef UNUSED
// static
void drd_record_display_successful_tries(Per_Display_Data * data, Retry_Operation type_id, int tryct) {
   bool debug = false;
   DBGMSF(debug, "type_id=%d - %s, tryct=%d, Per_Display_Data: %p",
                 type_id, retry_type_name(type_id), tryct, data);
   data->try_stats[type_id].counters[tryct+1]++;
   DBGMSF(debug, "new counters value: %d", data->try_stats[type_id].counters[tryct+1]);
}


// static
void drd_record_display_failed_max_tries(Per_Display_Data * data, Retry_Operation type_id) {
   data->try_stats[type_id].counters[1]++;
}


// static
void drd_record_display_failed_fatally(Per_Display_Data * data, Retry_Operation type_id) {
   data->try_stats[type_id].counters[0]++;
}
#endif


void drd_record_display_tries(Per_Display_Data * pdd, Retry_Operation type_id, int rc, int tryct) {
   bool debug = false;
   DBGMSF(debug, "Executing. %s type_id=%d=%s, rc=%d, tryct=%d",
                  dpath_repr_t(&pdd->dpath),
                 type_id, retry_type_name(type_id), rc, tryct);

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
 *  summary Per_Display_Data instance.
 *
 *  \param  retry_type       type of transaction being reported
 *  \param  for_all_displays indicates whether this call is for a real display,
 *                           or for a synthesized data record containing data that
 *                           summarizes all displays
 *  \param  data             pointer to per-thread data
 *  \param  depth            logical indentation depth
 */
void report_display_try_typed_data_by_data(
      Retry_Operation     retry_type,
      bool                for_all_displays_total,
      Per_Display_Data *  data,
      int                 depth)
{
   // bool debug = false;
   int d1 = depth+1;
   int d2 = depth+2;
   ASSERT_IFF( (retry_type == -1), for_all_displays_total );
   // bool this_function_locked = pdd_lock_if_unlocked();

   int total_attempts_for_one_type =  get_display_total_tries_for_one_type_by_data(retry_type, data);

   if (for_all_displays_total) {  // reporting a synthesized summary record
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
      int    last_index3 =  display_index_of_highest_non_zero_counter(data->try_stats[retry_type].counters);
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
 * \param  for_all_displays  indicates whether this call is for a real display,
 *                           or for a synthesized data record containing data that
 *                           summarizes all displays
 * \param   data             pointer to record for one display
 * \param   depth            logical indentation depth
 */
void report_display_all_types_data_by_data(
      bool              for_all_displays,   // controls message
      Per_Display_Data* data,
      int               depth)
{
   for (int try_type_ndx = 0; try_type_ndx < RETRY_OP_COUNT; try_type_ndx++) {
      Retry_Operation type_id = (Retry_Operation) try_type_ndx;
      report_display_try_typed_data_by_data( type_id, for_all_displays, data, depth+1);
   }
}
