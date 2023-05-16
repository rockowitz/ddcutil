/** @file dsa2.c Dynamic sleep algorithm 2
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE    // for localtime_r()
#define __ISOC99_SOURCE

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <time.h>
 
#include "util/coredefs.h"
#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/timestamp.h"
#include "util/xdg_util.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/status_code_mgt.h"
#include "base/rtti.h"

#include "dsa2.h"


const bool  Default_DSA2_Enabled = DEFAULT_ENABLE_DSA2;
const int   Default_Look_Back    = 5;
const int   Default_Initial_Step = 7;  // 1.0
const int   Max_Recent_Values    = 20;
const int   Default_Interval     = 3;
const int   Default_Greatest_Tries_Upper_Bound = 3;
const Sleep_Multiplier
            Default_Average_Tries_Upper_Bound = 1.4;
const int   Default_Greatest_Tries_Lower_Bound = 2;
const Sleep_Multiplier
            Default_Average_Tries_Lower_Bound = 1.1;

bool  dsa2_enabled           = Default_DSA2_Enabled;
int   initial_step           = Default_Initial_Step;
int   adjustment_interval    = Default_Interval;
int   target_greatest_tries_upper_bound  = Default_Greatest_Tries_Upper_Bound;
int   target_avg_tries_upper_bound_10    = Default_Average_Tries_Upper_Bound * 10; // multiply by 10 for integer arithmetic
int   target_greatest_tries_lower_bound  = Default_Greatest_Tries_Lower_Bound;
int   target_avg_tries_lower_bound_10    = Default_Average_Tries_Lower_Bound * 10;
int   min_decrement_lookback = 5;
int   global_lookback = Default_Look_Back;


bool
dsa2_set_greatest_tries_upper_bound(int tries) {
   bool result = false;
   if ( 1 <= tries && tries <= MAX_MAX_TRIES) {    // should get actual write/read maxtries
      target_greatest_tries_upper_bound = tries;
      result = true;
   }
   return result;
}


bool
dsa2_set_average_tries_upper_bound(Sleep_Multiplier avg_tries) {
   bool result = false;
   if (1.0 <= avg_tries && avg_tries <= MAX_MAX_TRIES) {
      target_avg_tries_upper_bound_10 = avg_tries * 10;
      result = true;
   }
   return result;
}

//
// Utility Functions
//

#ifdef UNUSED
int dpath_busno(DDCA_IO_Path dpath) {
   assert(dpath.io_mode == DDCA_IO_I2C);
   return        dpath.path.i2c_busno;
}
#endif


//
// Successful Invocation Struct
//

typedef struct {
   time_t epoch_seconds;    // timestamp to aid in development
   int    tryct;            // how many tries
   int    required_step;    // step level of successful invocation
} Successful_Invocation;


/** Returns a string representation of #Successful_Invocation instance.
 *  The value is valid until the next call to this function in the
 *  current thread.
 *
 *  @param  value of #Successful_Invocation (not a pointer)
 *  @return string representation
 */
char * si_repr_t(Successful_Invocation si) {
   static GPrivate  buf_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&buf_key, 40);

   g_snprintf(buf,  40,  "{%2d,%2d,%s}",
             si.tryct, si.required_step, formatted_epoch_time_t(si.epoch_seconds));

   return buf;
}


//
// Circular_Invocation_Result_Buffer
//

typedef struct {
   Successful_Invocation *    values;
   int      size;     // size of values[]
   int      ct;       // number of values used: 0..size
   int      nextpos;  // index to next write to
} Circular_Invocation_Result_Buffer;


/** Allocates a new #Circular_Invocation_Result_Buffer of
 *  #Successful_Invocation structs
 *
 *  @param  size  buffer size (number of entries)
 *  @return newly allocated #Circular_Integer_Buffer
 */
static Circular_Invocation_Result_Buffer *
cirb_new(int size) {
   Circular_Invocation_Result_Buffer * cirb = calloc(1, sizeof(Circular_Invocation_Result_Buffer));
   cirb->values = calloc(size, sizeof(Successful_Invocation));
   cirb->size = size;
   cirb->ct = 0;
   cirb->nextpos = 0;
   return cirb;
}


/** Frees a #Circular_Invocation_Result_Buffer
 *
 *  @param  cirb  pointer to buffer
 */
static void
cirb_free(Circular_Invocation_Result_Buffer * cirb) {
   free(cirb->values);
   free(cirb);
}


/** Appends a #Successful_Invocation struct to a #Circular_Invocation_Result_Buffer.
 *
 *  @param   cirb  pointer to #Circular_Integer_Buffer
 *  @param   value value to append
 */
static void
cirb_add(Circular_Invocation_Result_Buffer* cirb, Successful_Invocation value) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "cirb=%p, cirb->nextpos=%2d, cirb->ct=%2d, value=%s",
         cirb, cirb->nextpos, cirb->ct, si_repr_t(value));
    cirb->values[cirb->nextpos] = value;
    cirb->nextpos = (cirb->nextpos+1) % cirb->size;
    if (cirb->ct < cirb->size)
       cirb->ct++;
    DBGTRC_DONE(debug, DDCA_TRC_NONE, "cirb=%p, cirb->nextpos=%2d, cirb->ct=%2d",
          cirb, cirb->nextpos, cirb->ct);
}


/** Given a logical index into a #Circular_Invocation_Result_Buffer returns the
 *  physical index.
 *
 *  @param  cirb     pointer to a #Circular_Invocation_Result_Buffer
 *  @param  logical  logical index, 0 for the oldest entry in the buffer
 *  @return physical physical index, -1 if < 0 or logical exceeds the number of
 *                            values in the buffer
 */
static int
cirb_logical_to_physical_index(Circular_Invocation_Result_Buffer *cirb, int logical) {
   bool debug = false;
   int physical = -1;
   if (logical < cirb->ct) {
      physical = (cirb->ct < cirb->size)
                       ? logical
                       : (cirb->nextpos +logical) % cirb->size;
   }
   DBGTRC(debug, DDCA_TRC_NONE,
         "Executing logical=%2d, cirb->ct=%2d, cirb->size=%2d, cirb->nextpos=%2d, Returning: physical=%2d",
         logical, cirb->ct, cirb->size, cirb->nextpos, physical);
   return physical;
}


/** Returns the #Successful_Invocation value at the specified logical index
 *  in a #Circular_Invocation_Result_Buffer.
 *
 *  @param  cirb     pointer to a #Circular_Invocation_Result_Buffer
 *  @param  logical  logical index, 0 based
 *  @return #Successful_Invocation_Result value, {-1, -1, 0} if not found
 */
static Successful_Invocation
cirb_get_logical(Circular_Invocation_Result_Buffer *cirb, int logical) {
   int physical = cirb_logical_to_physical_index(cirb, logical);
   Successful_Invocation result = {-1,-1,0};
   if (physical >= 0)
      result = cirb->values[physical];
   return result;
}


/** Returns an array of the most recent values in a #Circular_Invocation_Result_Buffer.
 *
 *  @param  cirb            pointer to a #Circular_Invocation_Result_Buffer
 *  @param  ct              number of values to retrieve, if < 0 retrieve all
 *  @param  latest_values[] pointer to suitably sized buffer in which to
 *                          return values
 */
static int
cirb_get_latest(Circular_Invocation_Result_Buffer * cirb,
                int ct,
                Successful_Invocation latest_values[])
{
   if (ct < 0)
      ct = cirb->ct;

   int skipct = 0;
   if (ct <= cirb->ct)
      skipct = cirb->ct - ct;
   if (ct > cirb->ct)
      ct = cirb->ct;
   for (int ctr = 0; ctr < ct; ctr++) {
      latest_values[ctr] = cirb_get_logical(cirb, ctr+skipct);
   }
   return ct;
}


/** Output a debugging report of a #Circular_Invocation_Result_Buffer
 *
 *  @param cirb   pointer to buffer
 *  @param depth  logical indentation depth
 */
static void
dbgrpt_circular_invocation_results_buffer(Circular_Invocation_Result_Buffer * cirb,
                                          int depth)
{
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Circular_Invocation_Result_Buffer", cirb, depth);
   rpt_int("size", NULL, cirb->size, d1);
   rpt_int("ct",   NULL, cirb->ct,   d1);
   rpt_label(d1, "Buffer contents:");
   for (int ndx = 0; ndx < MIN(cirb->size, cirb->ct); ndx++) {
      rpt_vstring(d2, "values[%2d]: tryct = %d, required_step=%d, timestamp=%s",
                      ndx,
                      cirb->values[ndx].tryct,
                      cirb->values[ndx].required_step,
                      formatted_epoch_time_t(cirb->values[ndx].epoch_seconds));
   }
   rpt_label(d1, "Values by latest: ");
   for (int ndx = 0; ndx < cirb->ct; ndx++) {
      int physical = cirb_logical_to_physical_index(cirb, ndx);
      Successful_Invocation si = cirb_get_logical(cirb, ndx);
      rpt_vstring(d2, "logical index: %2d, physical index: %2d, tryct = %d, required_step=%d, timestamp=%s",
                      ndx, physical, si.tryct, si.required_step,
                      formatted_epoch_time_t(si.epoch_seconds));
   }
}


//
// Results Tables
//

#define MAX_RECENT_VALUES 20

static int steps[] = {0,5,10,20,30,50,70,100,130, 160, 200};    // multiplier * 100
static int step_ct = ARRAY_SIZE(steps);         //11
static int step_last = ARRAY_SIZE(steps)-1;     // index of last entry

#define RTABLE_FROM_CACHE    0x01
#define RTABLE_BUS_DETECTED  0x02
#define RTABLE_EDID_VERIFIED 0x04

Value_Name_Table rtable_status_flags_table = {
      VN(RTABLE_FROM_CACHE),
      VN(RTABLE_BUS_DETECTED),
      VN(RTABLE_EDID_VERIFIED),
      VN_END
};

typedef struct Results_Table {
   Circular_Invocation_Result_Buffer * recent_values;
   // use int rather than a smaller type to simplify use of str_to_int()
   int  busno;
   int  cur_step;

   int  remaining_interval;
   int  min_ok_step;
   int  cur_retry_loop_step;

   int  initial_step;
   int  initial_lookback;
   int  cur_lookback;
   int  adjustments_up;
   int  total_steps_up;
   int  adjustments_down;
   int  total_steps_down;
   int  successful_try_ct;
   int  retryable_failure_ct;
   int  reset_ct;
   Byte edid_checksum_byte;
   Byte state;

   // format 1
   // bool found_failure_step;
   // int  lookback;
} Results_Table;

static Results_Table ** results_tables;


/** Output a debugging report for a #Results_Table
 *
 *  @param rtable  pointer to table instance
 *  @param depth   logical indentation depth
 */
static void
dbgrpt_results_table(Results_Table * rtable, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Results_Table", rtable, depth);
#define ONE_INT_FIELD(_name) rpt_int(#_name, NULL, rtable->_name, d1)
   ONE_INT_FIELD(busno);
   ONE_INT_FIELD(cur_step);
   ONE_INT_FIELD(cur_lookback);
   ONE_INT_FIELD(remaining_interval);
   // ONE_INT_FIELD(min_ok_step);
   // rpt_bool("found_failure_step", NULL, rtable->found_failure_step, d1);
   ONE_INT_FIELD(cur_retry_loop_step);

   ONE_INT_FIELD(initial_step);
// rpt_bool("initial_step_from_cache", NULL, rtable->initial_step_from_cache, d1);
   ONE_INT_FIELD(adjustments_up);
   ONE_INT_FIELD(total_steps_up);
   ONE_INT_FIELD(adjustments_down);
   ONE_INT_FIELD(total_steps_down);
   ONE_INT_FIELD(successful_try_ct);
   ONE_INT_FIELD(retryable_failure_ct);
   ONE_INT_FIELD(initial_lookback);
   rpt_vstring(d1, "edid_checksum_byte                    0x%02x", rtable->edid_checksum_byte);
   rpt_vstring(d1, "state                          %s",
                   VN_INTERPRET_FLAGS_T(rtable->state, rtable_status_flags_table, "|"));
#undef ONE_INT_FIELD
   dbgrpt_circular_invocation_results_buffer(rtable->recent_values, d1);
}


/** Allocates a new #Results_Table
 *
 *  @param  busno  I2C bus number
 *  @return pointer to newly allocated #Results_Table
 */
static
Results_Table * new_results_table(int busno) {
   Results_Table * rtable = calloc(1, sizeof(Results_Table));
   rtable->busno = busno;
   rtable->initial_step = initial_step;
   rtable->cur_step = initial_step;
   rtable->cur_lookback = global_lookback;
   rtable->recent_values = cirb_new(MAX_RECENT_VALUES);
   rtable->remaining_interval = Default_Interval;
   // rtable->min_ok_step = 0;
   // rtable->found_failure_step = false;
   rtable->state = 0x00;
   rtable->initial_lookback = rtable->cur_lookback;
   return rtable;
}


//
static Byte
get_edid_checkbyte(int busno) {
   bool debug = false;
   I2C_Bus_Info * bus_info = i2c_find_bus_info_by_busno(busno);
   if (!bus_info)
      SEVEREMSG("i2c_find_bus_info_by_busno(%d) failed!", busno);
   assert(bus_info);
   Byte checkbyte = bus_info->edid->bytes[127];
   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "busno=%d, returning 0x%02x", busno, checkbyte);
   return checkbyte;
}



/** Frees a #Results_Table
 *
 *  @param rtable  pointer to table instance to free
 */
static void
free_results_table(Results_Table * rtable) {
   if (rtable) {
      if (rtable->recent_values)
         cirb_free(rtable->recent_values);
      free(rtable);
   }
}


/** Returns the #Results_Table for an I2C bus number
 *
 *  @param  bus number
 *  @return pointer to #Results_Table (may be newly created)
 */
Results_Table *
dsa2_get_results_table_by_busno(int busno, bool create_if_not_found) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bussno=%d, create_if_not_found=%s",
                                         busno, sbool(create_if_not_found));
   assert(busno <= I2C_BUS_MAX);
   Results_Table * rtable = results_tables[busno];
   if (rtable) {
      rtable->state |= RTABLE_BUS_DETECTED;
      if ( (rtable->state & RTABLE_FROM_CACHE) && !(rtable->state & RTABLE_EDID_VERIFIED)) {
         if (get_edid_checkbyte(busno) != rtable->edid_checksum_byte) {
            DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "EDID verification failed");
            free_results_table(rtable);
            results_tables[busno] = NULL;
            rtable = NULL;
         }
         else {
            rtable->state |= RTABLE_EDID_VERIFIED;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "EDID verification succeeded");
         }
      }
   }
   if (!rtable && create_if_not_found) {
      rtable = new_results_table(busno);
      results_tables[busno] = rtable;
      rtable->cur_step = initial_step;
      rtable->cur_retry_loop_step = initial_step;
      rtable->state = RTABLE_BUS_DETECTED;
      rtable->edid_checksum_byte = get_edid_checkbyte(busno);
   }
   DBGTRC_RET_STRUCT(debug, DDCA_TRC_NONE, "Results_Table", dbgrpt_results_table, rtable);
   return rtable;
}


#ifdef UNUSED
// static
void set_multiplier(Results_Table * rtable, Sleep_Multiplier multiplier) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "multiplier=%7.3f", multiplier);
   rtable->cur_step = multiplier_to_step(multiplier);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set cur_step=%d", initial_step);
}


void
dsa2_set_multiplier_by_path(DDCA_IO_Path dpath, Sleep_Multiplier multiplier) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, multiplier=%7.3f", dpath_repr_t(&dpath), multiplier);
   Results_Table * rtable = dsa2_get_results_table_by_busno(dpath_busno(dpath));
   rtable->cur_step = multiplier_to_step(multiplier);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set cur_step=%d", initial_step);
}
#endif


/** Given a floating point multiplier value, return the index of the step
 *  found by rounding down the value specified.
 *
 *  @param  multiplier  floating point multiplier value
 *  @return step index
 *
 *  @remark
 *  Relies on fact that IEEE floating point variables with whole integer values
 *  convert to correct integer variables.
 */
static int
multiplier_to_step(Sleep_Multiplier multiplier) {
   bool debug = false;
   int imult = multiplier * 100;

   int ndx = 0;
   for (; ndx < step_ct; ndx++) {
      if ( steps[ndx] >= imult )
               break;
   }

   int step = (ndx == step_ct) ? step_ct-1 : ndx;
   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "multiplier = %7.5f, imult = %d, step=%d, steps[%d]=%d",
                                         multiplier, imult, step, step, steps[step]);
   return step;
}


#ifdef TEST
void test_float_to_step_conversion() {
   for (int ndx = 0; ndx < step_ct; ndx++) {
      Sleep_Multiplier f = steps[ndx] / 100.0;
      int found_ndx = multiplier_to_step(f);
      printf("ndx=%2d, steps[ndx]=%d, f=%2.5f, found_ndx=%d\n",
             ndx, steps[ndx], f, found_ndx);
      assert(found_ndx == ndx);
   }
}
#endif


/** Sets the global initial_step value used for new #Results_Table records
 *  and also resets the cur_step and related values in each existing
 *  #Results_Table.
 *
 *  @param multiplier sleep multiplier value
 */
void
dsa2_reset_multiplier(Sleep_Multiplier multiplier) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "multiplier=%7.3f", multiplier);
   initial_step = multiplier_to_step(multiplier);
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx]) {
         Results_Table * rtable = results_tables[ndx];
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Processing Results_Table for /dev/i2c-%d", rtable->busno);
         rtable->cur_step = initial_step;
         // rtable->found_failure_step = false;
         // rtable->min_ok_step = 0;
         rtable->cur_retry_loop_step = initial_step;
         rtable->adjustments_down = 0;
         rtable->adjustments_up = 0;
         rtable->total_steps_up = 0;
         rtable->total_steps_down = 0;
         rtable->successful_try_ct = 0;
         rtable->retryable_failure_ct = 0;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set initial_step=%d", initial_step);
}


//
// The Algorithm
//

/** Encapsulates the algorithm used by #adjust_for_recent_successes() to
 *  determine if recent Successful_Invocation buffer statistics indicate
 *  that the multiplier currently supplied by the dsa2 subsystem should
 *  be increased.
 *
 *  @param highest_tryct  highest try count for any Successful_Invocation record
 *  @param total_tryct    total number of tries reported
 *  @param interval       number of Successful_Invocation records examined
 *  @param true if cur_step needs to be increased, false if not
 */
static bool
dsa2_too_many_errors(int highest_tryct, int total_tryct, int interval) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
         "target_greatest_tries_upper_bound=%d, target_avg_tries_upper_bound_10=%d, highest_tryct=%d, total_tryct=%d, interval=%d",
          target_greatest_tries_upper_bound,    target_avg_tries_upper_bound_10,    highest_tryct,    total_tryct,    interval);

   int computed_avg_10 = (total_tryct * 10)/interval;
   bool result = ( highest_tryct > target_greatest_tries_upper_bound ||
                   computed_avg_10 > target_avg_tries_upper_bound_10);     // i.e. total_tryct/interval > 1.4)

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "computed_avg_10=%d", computed_avg_10);
   return result;
}

// #ifdef PERHAPS_FUTURE
static bool
dsa2_too_few_errors(int highest_tryct, int total_tryct, int interval) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
         "target_greatest_tries_lower_bound=%d, target_avg_tries_lower_bound_10=%d, highest_tryct=%d, total_tryct=%d, interval=%d",
          target_greatest_tries_lower_bound,    target_avg_tries_lower_bound_10,    highest_tryct,    total_tryct,    interval);

   int computed_avg_10 = (total_tryct * 10)/interval;
   bool result = (highest_tryct   <= target_greatest_tries_lower_bound &&
                  computed_avg_10 <= target_avg_tries_lower_bound_10);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "computed_avg_10=%d", computed_avg_10);
   return result;
}
// #endif


/** Calculates the step to be used on the next try loop iteration after a
 *  retryable loop failure.  The step number may be incremented some amount
 *  based on the number of tries remaining.
 *
 *  If remaining_tries == 0, there's no next_step that's possible.
 *  Return prev_step in this degenerate case.
 *
 *  @param  prev_step        number of the step that failed
 *  @param  remaining_tries  number of tries remaining
 *  @return step number to be used for next try loop iteration
 */
int
dsa2_next_retry_step(int prev_step, int remaining_tries)  {
   bool debug = false;
   int next_step = prev_step;
   if (remaining_tries > 0) {   // handle maxtries failure
      int remaining_steps = step_ct - prev_step;
      Sleep_Multiplier fadj = (1.0*remaining_steps)/remaining_tries;
      Sleep_Multiplier fadj2 = fadj;
      if (fadj > .75 && fadj < 1.0)
         fadj2 = 1.0;
      int adjustment = fadj2;
      next_step = prev_step + adjustment;
      if (next_step > step_last)
         next_step = step_last;
      DBGTRC_EXECUTED(debug, DDCA_TRC_NONE,
            "Executing prev_step=%d, remaining_tries=%d, fadj=%2.3f, fadj2=%2.3f, adjustment=%d, returning %d",
            prev_step, remaining_tries, fadj, fadj2, adjustment, next_step);
   }
   else {
      DBGTRC_EXECUTED(debug, DDCA_TRC_NONE,
            "remaining_tries == 0, returning next_step = prev_step = %d", next_step);
   }
   return next_step;
}


#ifdef TESTING
void test_dsa2_next_retry_step() {
   for (int max_tries = 5; max_tries <= 5; max_tries++) {
      for (int initial_step = 0; initial_step <= step_last; initial_step++) {
         int cur_step = initial_step;
         int tryctr = 1;
         while (tryctr < max_tries) {
            printf("max_tries=%2d, initial_step=%2d, tryctr=%2d, cur_step=%2d\n", max_tries, initial_step, tryctr, cur_step);
            cur_step = dsa2_next_retry_step(cur_step, max_tries-tryctr);
            tryctr++;
         }
         printf("\n");
      }
      printf("=============================================\n");
   }
}
#endif


/** This function is called periodically to possibly adjust the cur_step value
 *  for a device either up or down based on recent successful execution data
 *  recorded in the circular successful invocation structure.
 *
 *  @param rtable pointer to #Results_Table to examine
 */
static void
dsa2_adjust_for_recent_successes(Results_Table * rtable) {
   bool debug = false;

   // n. called only if most recent try was a success
   Successful_Invocation latest_values[MAX_RECENT_VALUES];
   int actual_lookback = cirb_get_latest(
                              rtable->recent_values,
                              -1,     // rtable->lookback,
                              latest_values);
   assert(actual_lookback > 0);
   int max_tryct = 0;
   int min_tryct = 99;
   int total_tryct = 0;
   char  b[300];
   b[0] = '\0';
   for (int ndx = 0; ndx < actual_lookback; ndx++) {
      if (debug)
         sprintf(b + strlen(b), "%s{tryct:%d,reqd step:%d,%ld}",
             (ndx > 0) ? ", " : "",
             latest_values[ndx].tryct, latest_values[ndx].required_step,
             latest_values[ndx].epoch_seconds);

      total_tryct += latest_values[ndx].tryct;
      if (latest_values[ndx].tryct > max_tryct)
            max_tryct = latest_values[ndx].tryct;
      if (latest_values[ndx].tryct < min_tryct)
            min_tryct = latest_values[ndx].tryct;
   }
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, actual_lookback = %d, latest_values:%s",
         rtable->busno, actual_lookback, b);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "max_tryct = %d, min_tr yct = %d, total_tryct = %d",
                                         max_tryct, min_tryct, total_tryct);
   int last_value_pos = actual_lookback - 1;
   int most_recent_step = latest_values[last_value_pos].required_step;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "actual_lookback=%d,  most_recent_step=%d",
                                         actual_lookback, most_recent_step);

   if (dsa2_too_many_errors(max_tryct, total_tryct, actual_lookback) && rtable->cur_step < most_recent_step) {
      if (rtable->cur_step < step_last) {
         rtable->cur_step++;
         rtable->total_steps_up++;
         rtable->adjustments_up++;
      }
      // rtable->found_failure_step = true;
      // rtable->min_ok_step = rtable->cur_step;
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
      //       "busno=%d, Incremented cur_step. Set found_failure_step=true, min_ok_step=%d, cur_step=%2d",
      //       rtable->busno, rtable->min_ok_step, rtable->cur_step);
   }
   else {
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
      //        "busno=%d, Looking to decrement cur_step, cur_step=%d, found_failure_step=%s, min_ok_step = %d",
      //        rtable->busno, rtable->cur_step, sbool(rtable->found_failure_step), rtable->min_ok_step);
#ifdef OLD
      if (rtable->cur_step > 0) {
         if (total_tryct <= actual_lookback+1) {  // i.e. no more than 1 retry was required
            rtable->cur_step--;
            rtable->adjustments_down++;
            if (rtable->cur_step > rtable->min_ok_step)
               rtable->min_ok_step = rtable->cur_step;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Unconditionally decremented cur_step.  New value: %d", rtable->cur_step);
         }
         else if (rtable->found_failure_step) {
            if (rtable->cur_step > rtable->min_ok_step) {
               rtable->cur_step--;
               rtable->adjustments_down++;
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Decremented cur_step. New value: %d", rtable->cur_step);
            }
         }
         else {
            rtable->cur_step--;
            rtable->adjustments_down++;
            DBGTRC(debug, DDCA_TRC_NONE, "Decremented cur_step. New value: %d", rtable->cur_step);
         }
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "rtable->cur_step = 0, cannot decrement");
      }
#endif
      if (actual_lookback >= min_decrement_lookback && rtable->cur_step > 0 && dsa2_too_few_errors(max_tryct, total_tryct, actual_lookback)) {
         rtable->cur_step--;
         rtable->total_steps_down++;
         rtable->adjustments_down++;
         rtable->cur_lookback = actual_lookback;
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, Decremented cur_step. New value: %d", rtable->busno, rtable->cur_step);
      }
   }
   // DBGTRC_DONE(debug, DDCA_TRC_NONE,
   //        "busno=%d, max_tryct=%d, total_tryct=%d, rtable->cur_step=%d, rtable->min_ok_step=%d. rtable->found_failure_step=%s",
   //         rtable->busno, max_tryct, total_tryct, rtable->cur_step, rtable->min_ok_step, sbool(rtable->found_failure_step) );
   DBGTRC_DONE(debug, DDCA_TRC_NONE,
          "busno=%d, max_tryct=%d, total_tryct=%d, rtable->cur_step=%d",
           rtable->busno, max_tryct, total_tryct, rtable->cur_step);
}


/** Called at the bottom of each try loop that fails in #ddc_read_write_with_retry().
 *
 *  Based on the number of tries remaining, may increment the retry_loop_step
 *  for the next step execution in the current loop.
 *
 *  @param rtable            Results_Table for device
 *  @param remaining_tries   number of tries remaining
 */
void
dsa2_note_retryable_failure(Results_Table * rtable, int remaining_tries) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rtable=%p, busno=%d, remaining_tries=%d, dsa2_enabled=%s",
         rtable, rtable->busno, remaining_tries, sbool(dsa2_enabled));
   assert(rtable);
   rtable->retryable_failure_ct++;
   int prev_step = rtable->cur_retry_loop_step;
   // has special handling for case of remaining_tries = 0;

   int next_step =  dsa2_next_retry_step(prev_step, remaining_tries);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "dsa2_next_retry_step(%d,%d) returned %d",
                                         prev_step, remaining_tries, next_step);
   rtable->cur_retry_loop_step = next_step;

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Previous step=%d, next step = %d",
                                     prev_step, rtable->cur_retry_loop_step);
}


/** Called after all (possible) retries in #ddc_write_read_with_retry()
 *
 *  If ddcrc = 0 (i.e. the operation succeeded, which is the normal case)
 *  a #Successful_Invocation record is added to the Circular Invocation
 *  Response buffer. The results table for the bus is updated.
 *  Depending on how many tries were required, the current step
 *  may be adjusted up or down. The cur_retry_loop_step is reset to the
 *  (possibly update) cur_loop_step, ready to be used on the next
 *  #ddc_write_read_with_retry() operation.
 *
 *  If ddcrc != 0 (the operation failed, either because of a fatal error
 *  or retries exhausted) it's not clear what to do.  Currently just
 *  cur_retry_loop_step is set to the global initial_step.
 *
 *  @param  rtable  #Results_Table for device
 *  @param  ddcrc   #ddc_write_read_with_retry() return code
 *  @param  tries   number of tries used, always < max tries for success,
 *                  always max tries for retries exhausted, and either
 *                  in case of a fatal error of some sort
 */
void
dsa2_record_final(Results_Table * rtable, DDCA_Status ddcrc, int tries) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rtable=%p, busno=%d, ddcrc=%s, tries=%d dsa2_enabled=%s",
                   rtable, rtable->busno, psc_desc(ddcrc), tries, sbool(dsa2_enabled));
   if (!dsa2_enabled) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "dsa2 not enabled");
      return;
   }
   // Per_Display_Data * pdd = pdd_get_per_display_data(dpath,  false);
   // assert(pdd);

   assert(rtable);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_retry_loop_step=%d", rtable->cur_retry_loop_step);
   if (ddcrc == 0) {
      rtable->successful_try_ct++;
      Successful_Invocation si = {time(NULL), tries, rtable->cur_retry_loop_step};
      cirb_add(rtable->recent_values, si);
      if (tries > 3) {
         if (rtable->cur_step < step_last) {
            rtable->total_steps_up += rtable->cur_retry_loop_step - rtable->cur_step;
            rtable->cur_step = rtable->cur_retry_loop_step;
            rtable->adjustments_up++;
            // rtable->min_ok_step = rtable->cur_step;
            // rtable->found_failure_step = true;
         }
      }
      else if (tries > 2) {
         rtable->remaining_interval -= 1;
         if (rtable->remaining_interval == 0) {
            dsa2_adjust_for_recent_successes(rtable);
            rtable->remaining_interval = adjustment_interval;
         }
      }
      else {
         dsa2_adjust_for_recent_successes(rtable);
         rtable->remaining_interval = adjustment_interval;
      }
      rtable->cur_retry_loop_step = rtable->cur_step;
   }

   else {    // ddcrc != 0
      // no more tries are possible, set dummy values
      rtable->remaining_interval = adjustment_interval;
      rtable->cur_retry_loop_step = initial_step;    // ???
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE,
               "cur_step=%d, cur_retry_loop_step=%d, remaining_interval=%d",
               rtable->cur_step, rtable->cur_retry_loop_step, rtable->remaining_interval);
}


/** Gets the current sleep multiplier value for a device
 *
 *  Converts the internal step number for the current retry loop
 *  to a floating point value.
 *
 *  @param  rtable #Results_Table for device
 *  @return multiplier value
 */
Sleep_Multiplier
dsa2_get_adjusted_sleep_multiplier(Results_Table * rtable) {
   bool debug = false;
   Sleep_Multiplier result = 1.0f;
   assert(rtable);
   result = steps[rtable->cur_retry_loop_step]/100.0;
   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE,
                  "rtable=%p, rtable->cur_retry_loop_step=%d, Returning: %.2f",
                  rtable,  rtable->cur_retry_loop_step, result);
   return result;
}


/** Reports internal statistics on the dsa2 algorithm.
 *
 *  @param rtable pointer to #Results_Table
 *  @param depth  logical indentation
 */
void dsa2_report_internal(Results_Table * rtable, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Dynamic sleep algorithm 2 data for /dev/i2c-%d:", rtable->busno);
   rpt_vstring(d1, "Initial Step:       %3d,  multiplier = %4.2f", rtable->initial_step, steps[rtable->initial_step]/100.0);
// rpt_vstring(d1, "Initial step from cache: %s", sbool(rtable->initial_step_from_cache));
   rpt_vstring(d1, "Final Step:         %3d,  multiplier = %4.2f", rtable->cur_step, steps[rtable->cur_step]/100.0);
   rpt_vstring(d1, "Initial lookback ct:%3d", rtable->initial_lookback);
   rpt_vstring(d1, "Final lookback ct:  %3d", rtable->cur_lookback);
   rpt_vstring(d1, "Adjustment interval:%3d", adjustment_interval);
   rpt_vstring(d1, "Adjustments up:     %3d", rtable->adjustments_up);
   rpt_vstring(d1, "Total steps up:     %3d", rtable->total_steps_up);
   rpt_vstring(d1, "Adjustments down:   %3d", rtable->adjustments_down);
   rpt_vstring(d1, "Total steps down:   %3d", rtable->total_steps_down);
   rpt_vstring(d1, "Successes:          %3d", rtable->successful_try_ct);
   rpt_vstring(d1, "Retryable Failures: %3d", rtable->retryable_failure_ct);
}


void dsa2_report_internal_all(int depth) {
   int d1 = depth+1;
   rpt_label(depth, "Dynamic Sleep Adjustment (algorithm 2)");
   for (int busno = 0; busno <= I2C_BUS_MAX; busno++) {
      Results_Table * rtable = dsa2_get_results_table_by_busno(busno, false);
      if (rtable)
         dsa2_report_internal(rtable, d1);
   }
}


//
// Persistent Statistics
//

/** Returns the name of the file that stores persistent stats
 *
 *  @return name of file, normally $HOME/.cache/ddcutil/stats
 *
 *  Caller is responsible for freeing returned value
 */
static char *
stats_cache_file_name() {
   return xdg_cache_home_file("ddcutil", "stats");
}

bool dsa2_is_from_cache(Results_Table * rtable) {
   assert(rtable);
   // return (rtable && rtable->initial_step_from_cache);
   return (rtable && (rtable->state & RTABLE_FROM_CACHE));
}


/** Saves the current performance statistics in file ddcutil/stats
 *  within the user's XDG cache directory, typically $HOME/.cache.
 *
 *  @retval 0      success
 *  @return -errno if unable to open the stats file for writing
 */
Status_Errno
dsa2_save_persistent_stats() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   int result = 0;
   int results_tables_ct = 0;
   char * stats_fn = stats_cache_file_name();
   FILE * stats_file = NULL;
   result = fopen_mkdir(stats_fn, "w", ferr(), &stats_file);
   if (!stats_file) {
      result = -errno;
      SEVEREMSG("Error opening %s: %s", strerror(errno));
      goto bye;
   }
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened %s", stats_fn);
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx] && (results_tables[ndx]->state & RTABLE_BUS_DETECTED))
         results_tables_ct++;
   }
   DBGTRC(debug, DDCA_TRC_NONE, "results_tables_ct = %d", results_tables_ct);


   int format_id = 2;
   fprintf(stats_file, "FORMAT %d\n", format_id);
   fprintf(stats_file, "* DEV  /dev/i2c device\n");
   fprintf(stats_file, "* EC   EDID check sum byte\n");
   fprintf(stats_file, "* C    current step\n");
   if (format_id == 1) {
      fprintf(stats_file, "* L    lookback\n");
      fprintf(stats_file, "* I    interval remaining\n");
      fprintf(stats_file, "* M    minimum ok step\n");
      fprintf(stats_file, "* F    found failure step\n");
      fprintf(stats_file, "* Values {epoch_seconds, try_ct, required_step}\n");
      fprintf(stats_file, "* DEV EC C L I M F Values\n");
   }
   else {
      fprintf(stats_file, "* I    interval remaining\n");
      fprintf(stats_file, "* DEV EC C I Values\n");
      fprintf(stats_file, "* Values {tries required, step, epoch seconds}\n");
   }

   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx]) {
         Results_Table * rtable = results_tables[ndx];
         if (debug)
            dbgrpt_results_table(rtable, 2);
         if (format_id == 1) {
            fprintf(stats_file, "i2c-%d %02x %d %d %d %d %d",
                 rtable->busno, rtable->edid_checksum_byte, rtable->cur_step, rtable->cur_lookback,
                 rtable->remaining_interval, 0, 0);
         }
         else {
            fprintf(stats_file, "i2c-%d %02x %d %d %d",
                 rtable->busno, rtable->edid_checksum_byte, rtable->cur_step,
                 rtable->remaining_interval, rtable->cur_lookback);
         }
         for (int k = 0; k < rtable->recent_values->ct; k++) {
            Successful_Invocation si = cirb_get_logical(rtable->recent_values, k);
            fprintf(stats_file, " {%d,%d,%ld}", si.tryct, si.required_step, si.epoch_seconds);
         }
         fputc('\n', stats_file);
      }
   }
   fclose(stats_file);
bye:
   free(stats_fn);
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result,
                    "Wrote %d Results_Table(s)", results_tables_ct);
   return result;
}


/** Deletes the stats file.  It is not an error if the file does not exist.
 *
 *  @retval -errno if deletion fails for any reason other than non-existence
 *  @retval  0     success
 */
Status_Errno
dsa2_erase_persistent_stats() {
   bool debug = false;
   Status_Errno result = 0;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   char * stats_fn = stats_cache_file_name();
   int rc = remove(stats_fn);
   if (rc < 0 && errno != ENOENT)
      result = -errno;
   free(stats_fn);
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result, "");
   return result;
}


static void
stats_file_error(GPtrArray* errmsgs, char * format, ...) {
   va_list(args);
   va_start(args, format);
   char buffer[200];
   vsnprintf(buffer, sizeof(buffer), format, args);
   if (errmsgs)
      g_ptr_array_add(errmsgs, strdup(buffer));
   // DBGMSG(buffer);
   va_end(args);
}


static bool
cirb_parse_and_add(Circular_Invocation_Result_Buffer * cirb, char * segment) {
   bool debug = false;
   DBGMSF(debug, "segment |%s|", segment);
   bool result = false;
   if ( strlen(segment) >= 7 &&
        segment[0] == '{'    &&
        segment[strlen(segment)-1] == '}' )
   {
      char * s = g_strdup(segment);
      char * comma_pos = strchr(s, ',');
      char * comma_pos2 = (comma_pos) ? strchr(comma_pos+1, ',') : NULL;
      char * lastpos = s + strlen(s) - 1;  // subtract for final '}'
      if (comma_pos && comma_pos2) {
         *comma_pos  = '\0';
         *comma_pos2 = '\0';
         *lastpos    = '\0';
         if (strlen(s+1) > 0 && strlen(comma_pos+1) > 0 && strlen(comma_pos2 + 1) > 0 ) {
            Successful_Invocation si;
            result  = str_to_int(s+1,             &si.tryct,         10);
            result &= str_to_int(comma_pos  + 1,  &si.required_step, 10);
            result &= str_to_long(comma_pos2 + 1, &si.epoch_seconds, 10);
            if (result) {
               cirb_add(cirb, si);
            }
         }
      }
      g_free(s);
   }
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}


/** Load execution statistics from a file.
 *
 *  The file name is determined using XDG rules
 *
 *  @retval  false a file was found but parsing failed
 *  @retval  true  loading succeeded or no file found
 */
Error_Info *
dsa2_restore_persistent_stats() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   char * stats_fn = stats_cache_file_name();
   Error_Info * result = NULL;

   // DBGMSG("stats_fn=%s", stats_fn);
   bool all_ok = true;
   GPtrArray* line_array = g_ptr_array_new_with_free_func(g_free);
   int linect = file_getlines(stats_fn, line_array, debug);
   if (linect == -ENOENT)
      goto bye0;

   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
   if (linect < 0) {
      stats_file_error(errmsgs, "Error  %s reading stats file %s", psc_desc(linect), stats_fn);
      all_ok = false;
      goto bye;
   }
   if (linect == 0)  { // empty file
      stats_file_error(errmsgs, "Empty stats file");
      goto bye;
   }
   char * format_id_line = g_ptr_array_index(line_array, 0);
   // DBGMSG("format_id_line %p |%s|", format_line, format_line);
   if (!str_starts_with(format_id_line, "FORMAT ")) {
      stats_file_error(errmsgs, "Invalid format line: %s", format_id_line);
      all_ok = false;
      goto bye;
   }
   int format_id;
   char * sformat = format_id_line + strlen("FORMAT ");
   // DBGMSG("sformat %d %p |%s|", strlen("FORMAT "), sformat, sformat);
   bool ok = str_to_int( sformat, &format_id, 10);
   if (!ok || (format_id != 1 && format_id != 2)) {
      stats_file_error(errmsgs, "Invalid format: %s", sformat);
      all_ok = false;
      goto bye;
   }

   for (int linendx = 1; linendx < line_array->len; linendx++) {
      char * cur_line = g_ptr_array_index(line_array, linendx);
      // DBGMSG("cur_line = |%s|", cur_line);
      if (strlen(cur_line) >= 1 && cur_line[0] != '#' && cur_line[0] != '*') {
         Null_Terminated_String_Array pieces = strsplit(cur_line, " ");
         int piecect = ntsa_length(pieces);
         int busno = -1;
         Results_Table * rtable = NULL;

         int fieldndx = 0;
         int min_pieces = 7;   // format 1
         if (format_id == 2)
            min_pieces = 5;

         bool ok = (piecect >= min_pieces);
         if (ok) {
            busno = i2c_name_to_busno(pieces[fieldndx++]);
            rtable = new_results_table(busno);
            // rtable->initial_step_from_cache = true;
            ok = (busno >= 0);
         }
         ok = ok && any_one_byte_hex_string_to_byte_in_buf(pieces[fieldndx++], &rtable->edid_checksum_byte);
         ok = ok && str_to_int(pieces[fieldndx++], &rtable->cur_step, 10);
         if (format_id == 1) {
            int isink;
            ok = ok && str_to_int(pieces[fieldndx++], &isink, 10);
         }
         ok = ok && str_to_int(pieces[fieldndx++], &rtable->remaining_interval, 10);
         if (format_id == 1) {
               int isink;
               ok = ok && str_to_int(pieces[fieldndx++], &isink, 10);
               ok = ok && str_to_int(pieces[fieldndx++], &isink, 10);
         }
         if (ok) {
            // rtable->found_failure_step = (iwork);
            rtable->cur_retry_loop_step = rtable->cur_step;
            rtable->initial_step = rtable->cur_step;
            rtable->initial_lookback = global_lookback;
         }
         else {
            if (ok) {
               rtable->cur_retry_loop_step = rtable->cur_step;
               rtable->initial_step = rtable->cur_step;

               // temp: dummy values
               // rtable->found_failure_step = false;
               rtable->initial_lookback = Default_Look_Back;
            }
         }
         if (piecect >= min_pieces) {   // handle no Successful_Invocation data
            for (int ndx = min_pieces; ndx < piecect; ndx++) {
               ok = ok && cirb_parse_and_add(rtable->recent_values, pieces[ndx]);
            }
         }
         if (!ok) {
            all_ok = false;
            stats_file_error(errmsgs, "Invalid: %s", cur_line);
            free_results_table(rtable);
         }
         else {
            rtable->state = RTABLE_FROM_CACHE;
            results_tables[busno] = rtable;
            if (debug)
               dbgrpt_results_table(rtable, 1);
         }
         ntsa_free(pieces, true);
         DBGTRC(debug, DDCA_TRC_NONE, "Restored stats for /dev/i2c-%d", busno);
      }
   }

   if (!all_ok) {
      for (int ndx = 0; ndx <= I2C_BUS_MAX; ndx++) {
         if (results_tables[ndx]) {
            free_results_table(results_tables[ndx]);
            results_tables[ndx] = NULL;
         }
      }
   }

bye:
   if (!all_ok) {
      result = errinfo_new(DDCRC_BAD_DATA, __func__, "Error(s) reading cached performance stats file %s", stats_fn);
      for (int ndx = 0; ndx < errmsgs->len; ndx++) {
         Error_Info * err = errinfo_new(DDCRC_BAD_DATA, __func__, g_ptr_array_index(errmsgs, ndx));
         errinfo_add_cause(result, err);
      }
   }
   g_ptr_array_free(errmsgs, true);

bye0:
  free(stats_fn);
  g_ptr_array_free(line_array, true);
  DBGTRC_RET_STRUCT(debug, DDCA_TRC_NONE, "Error_Info", errinfo_report, result);
  return result;
}


#ifdef DIDNT_WORK
Sleep_Multiplier logistic(double x) {
  // const double M_E =   2.7182818284590452354;
   double k = .5;
  double result =  exp(k*x)/(1+exp(k*x));
  return result;
}


void test_one_logistic(int steps) {
   double domain_min = -8.0f;
   double domain_max =  8.0f;
// dpiuble interval = 1.0f/steps;
   double interval = (domain_max-domain_min)/steps;
   for (int i = 0; i <= steps; i++) {
      double x = i *interval + domain_min;
      double y = logistic(x);
      printf("i = %2d  x = %2.3f   y = %2.3f\n", i, x, y);
   }
}
#endif


//
// Initialization and Termination
//

/** Initialize this file.
 */
void
init_dsa2() {
   RTTI_ADD_FUNC(dsa2_adjust_for_recent_successes);
   RTTI_ADD_FUNC(dsa2_erase_persistent_stats);
   RTTI_ADD_FUNC(dsa2_get_adjusted_sleep_multiplier);
   RTTI_ADD_FUNC(dsa2_get_results_table_by_busno);
   RTTI_ADD_FUNC(dsa2_note_retryable_failure);
   RTTI_ADD_FUNC(dsa2_record_final);
   RTTI_ADD_FUNC(dsa2_reset_multiplier);
   RTTI_ADD_FUNC(dsa2_restore_persistent_stats);
   RTTI_ADD_FUNC(dsa2_save_persistent_stats);
   RTTI_ADD_FUNC(dsa2_too_few_errors);
   RTTI_ADD_FUNC(dsa2_too_many_errors);

   results_tables = calloc(I2C_BUS_MAX+1, sizeof(Results_Table*));

   // test_one_logistic(10);
   // test_dsa2_next_retry_step();
   // test_float_to_step_conversion();
}


/** Release all resources
 */
void terminate_dsa2() { // release all resources
   if (results_tables) {
      for (int ndx = 0; ndx < I2C_BUS_MAX+1; ndx++) {
         if (results_tables[ndx])
            free_results_table(results_tables[ndx]);
      }
   }
}

