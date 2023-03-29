// dsa2.c

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE    // for localtime_r()
#define __ISOC99_SOURCE

#include <assert.h>
#include <math.h>
#include <regex.h>
#include <stdbool.h>
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
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/status_code_mgt.h"
#include "base/rtti.h"

#include "i2c/i2c_bus_core.h"     // UGH

#include "dsa2.h"


const bool Default_DSA2_Enabled = DEFAULT_ENABLE_DSA2;
const int  Default_Look_Back    = 5;
const int  Default_Initial_Step = 7;  // 1.0
const int  Max_Recent_Values    = 20;
const int  Default_Interval     = 3;

bool dsa2_enabled        = Default_DSA2_Enabled;
int  initial_step        = Default_Initial_Step;
int  adjustment_interval = Default_Interval;


//
// Utility Functions
//

int dpath_busno(DDCA_IO_Path dpath) {
   assert(dpath.io_mode == DDCA_IO_I2C);
   return        dpath.path.i2c_busno;
}


//
// Successful Invocation Struct
//

typedef struct {
   time_t epoch_seconds;    // timestamp to aid in development
   int    tryct;            // how many tries
   int    required_step;    // step level required
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
 *  @param  ct              number of values to retrieve
 *  @param  latest_values[] pointer to suitably sized buffer in which to
 *                          return values
 */
static int
cirb_get_latest(Circular_Invocation_Result_Buffer * cirb,
                int ct,
                Successful_Invocation latest_values[])
{
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

int steps[] = {0,5,10,20,30,50,70,100,130, 160, 200};    // multiplier * 100
int step_ct = ARRAY_SIZE(steps);   //11
int step_last = ARRAY_SIZE(steps)-1;        // index of last entry

#define RTABLE_FROM_CACHE    0x01
#define RTABLE_BUS_DETECTED  0x02
#define RTABLE_EDID_VERIFIED 0x04

typedef struct Results_Table {
   Circular_Invocation_Result_Buffer * recent_values;
   // use int rather than a smaller type to simplify use of str_to_int()
   int  busno;
   int  cur_step;
   int  lookback;
   int  remaining_interval;
   int  min_ok_step;
   bool found_failure_step;
   int  cur_retry_loop_step;

// bool initial_step_from_cache;   // ELIMINATE, USE RESULTS_TABLE_FROM_CACHE flag
   int  initial_step;
   int  initial_lookback;
   int  adjustments_up;
   int  adjustments_down;
   int  successful_observation_ct;
   int  reset_ct;
   int  retryable_failure_ct;
   Byte edid_checksum_byte;
   Byte state;
} Results_Table;


/** Output a debugging report a #Results_Table
 *
 *  @param rtable  point to table instance
 *  @param depth   logical indentation depth
 */
static void
dbgrpt_results_table(Results_Table * rtable, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Results_Table", rtable, depth);
#define ONE_INT_FIELD(_name) rpt_int(#_name, NULL, rtable->_name, d1)
   ONE_INT_FIELD(busno);
   ONE_INT_FIELD(cur_step);
   ONE_INT_FIELD(lookback);
   ONE_INT_FIELD(remaining_interval);
   ONE_INT_FIELD(min_ok_step);
   rpt_bool("found_failure_step", NULL, rtable->found_failure_step, d1);
   ONE_INT_FIELD(cur_retry_loop_step);

   ONE_INT_FIELD(initial_step);
// rpt_bool("initial_step_from_cache", NULL, rtable->initial_step_from_cache, d1);
   ONE_INT_FIELD(adjustments_up);
   ONE_INT_FIELD(adjustments_down);
   ONE_INT_FIELD(successful_observation_ct);
   ONE_INT_FIELD(retryable_failure_ct);
   ONE_INT_FIELD(initial_lookback);
   rpt_vstring(d1, "edid_checksum_byte                    0x%02x", rtable->edid_checksum_byte);
   char statebuf[100];
   statebuf[0] = '\0';
   if (rtable->state & RTABLE_BUS_DETECTED)
      strcpy(statebuf, "RTABLE_BUS_DETECTED");
   if (rtable->state & RTABLE_FROM_CACHE) {
      if (strlen(statebuf) > 0)
         strcat(statebuf, "|");
      strcat(statebuf, "RTABLE_FROM_CACHE");
   }
   if (rtable->state & RTABLE_EDID_VERIFIED) {
      if (strlen(statebuf) > 0)
         strcat(statebuf, "|");
      strcat(statebuf, "RTABLE_EDID_VERIFIED");
   }
   rpt_vstring(d1, "state                          %s", statebuf);
#undef ONE_INT_FIELD
   dbgrpt_circular_invocation_results_buffer(rtable->recent_values, d1);
}


Results_Table ** results_tables;


/** Allocates a new #Results_Table
 *
 *  @return pointer to newly allocated #Results_Table
 */
static
Results_Table * new_results_table(int busno) {
   Results_Table * rtable = calloc(1, sizeof(Results_Table));
   rtable->busno = busno;
   rtable->initial_step = initial_step;
   rtable->cur_step = initial_step;
   rtable->lookback = Default_Look_Back;
   rtable->recent_values = cirb_new(MAX_RECENT_VALUES);
   rtable->remaining_interval = Default_Interval;
   rtable->min_ok_step = 0;
   rtable->found_failure_step = false;
   rtable->state = 0x00;

// rtable->initial_step_from_cache = false;
   rtable->initial_lookback = rtable->lookback;
   return rtable;
}


Byte get_edid_checkbyte(int busno) {
   bool debug = false;
   I2C_Bus_Info * bus_info = i2c_find_bus_info_by_busno(busno);
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
Results_Table * dsa2_get_results_table_by_busno(int busno, bool create_if_not_found) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bussno=%d, create_if_not_found=%s", busno, sbool(create_if_not_found));
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
            DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "EDID verification succeeded");
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


Results_Table * dsa2_find_results_table_by_busno(int busno, bool create_if_not_found) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bussno=%d, create_if_not_found=%s", busno, sbool(create_if_not_found));
   assert(busno <= I2C_BUS_MAX);
   Results_Table * rtable = results_tables[busno];
   // rtable->state |= RESULTS_TABLE_FROM_DETECTED;

   if (!rtable && create_if_not_found) {
      rtable = new_results_table(busno);
      results_tables[busno] = rtable;
      rtable->cur_step = initial_step;
      rtable->cur_retry_loop_step = initial_step;
   }
   DBGTRC_RET_STRUCT(debug, DDCA_TRC_NONE, "Results_Table", dbgrpt_results_table, rtable);
   return rtable;
}



#ifdef UNUSED
// static
void set_multiplier(Results_Table * rtable, float multiplier) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "multiplier=%7.3f", multiplier);
   rtable->cur_step = multiplier_to_step(multiplier);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set cur_step=%d", initial_step);
}


void
dsa2_set_multiplier_by_path(DDCA_IO_Path dpath, float multiplier) {
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
static
int multiplier_to_step(float multiplier) {
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
      float f = steps[ndx] / 100.0;
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
void dsa2_reset_multiplier(float multiplier) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "multiplier=%7.3f", multiplier);
   initial_step = multiplier_to_step(multiplier);
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx]) {
         Results_Table * rtable = results_tables[ndx];
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Processing Results_Table for /dev/i2c-%d", rtable->busno);
         rtable->cur_step = initial_step;
         rtable->found_failure_step = false;
         rtable->min_ok_step = 0;

         rtable->cur_retry_loop_step = initial_step;
         rtable->adjustments_down = 0;
         rtable->adjustments_up = 0;
         rtable->successful_observation_ct = 0;
         rtable->retryable_failure_ct = 0;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set initial_step=%d", initial_step);
}

#ifdef UNUSED
void dsa2_reset(Results_Table * rtable) {
   if (rtable) {
      int busno = rtable->busno;
      free_results_table(rtable);
      results_tables[busno] = dsa2_get_results_table_by_busno(busno, true);
   }
}


void dsa2_reset_by_dpath(DDCA_IO_Path dpath) {
   assert(dpath.io_mode == DDCA_IO_I2C);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s", dpath_repr_t(&dpath));

   Results_Table * rtable = dsa2_get_results_table_by_busno(dpath.path.i2c_busno, false);
   dsa2_reset(rtable);
}
#endif


//
// The Algorithm
//


/** Encapsulates the algorithm used by #adjust_for_recent_successes()
 *  to determine if recent Successful_Invocation buffer stats
 *  indicate that the multiplier supplied by the dsa2 subsystem
 *  should be increased.
 *
 *  @param max_tryct   highest try count for any Successful_Invocation record
 *  @param total_tryct total number of tries reported
 *  @param interval    number of Successful_Invocation records examined
 *  @param true if cur_step for the needs to be increased, false if not
 */
static bool
too_many_errors(int max_tryct, int total_tryct, int interval) {
   bool debug = false;
   bool result = false;
   if (max_tryct > 3)
      result = true;
   else {
      if (total_tryct * 10 / interval > 14) {   // i.e. total_tryct/interval > 1.4)
         result = true;
      }
   }
   DBGTRC(debug, DDCA_TRC_NONE,
          "Executing max_tryct=%d, total_tryct=%d, interval=%d, Returning: %s",
          max_tryct, total_tryct, interval, sbool(result));
   return result;
}


/** Calculates the step to be used on the next try loop iteration after
 *  retryable loop failure.  The step number may be incremented some
 *  amount based on the number of tries remaining.
 *
 *  If remaining_tries == 0, there's no next_step that's needed.
 *  Return prev_step in this degenerate case.
 *
 *  @param  prev_step  number of step that failed
 *  @param  remaining_tries number of tries remaining
 *  @return step number to be used for next try loop iteration
 */
int
dsa2_next_retry_step(int prev_step, int remaining_tries)  {
   bool debug = false;
   int next_step = prev_step;
   if (remaining_tries > 0) {   // handle maxtries failure
      int remaining_steps = step_ct - prev_step;
      float fadj = (1.0*remaining_steps)/remaining_tries;
      float fadj2 = fadj;
      if (fadj > .75 && fadj < 1.0)
         fadj2 = 1.0;
      int adjustment = fadj2;
      int next_step = prev_step + adjustment;
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

   Successful_Invocation latest_values[MAX_RECENT_VALUES];
   int actual_lookback = cirb_get_latest(rtable->recent_values, rtable->lookback, latest_values);
   int max_tryct = 0;
   int total_tryct = 0;
   char  b[300];
   b[0] = '\0';
   for (int ndx = 0; ndx < actual_lookback; ndx++) {
      sprintf(b + strlen(b), "%s{tryct:%d,reqd step:%d,%ld}",
            (ndx > 0) ? ", " : "",
            latest_values[ndx].tryct, latest_values[ndx].required_step, latest_values[ndx].epoch_seconds);

      total_tryct += latest_values[ndx].tryct;
      if (latest_values[ndx].tryct > max_tryct)
            max_tryct = latest_values[ndx].tryct;
   }
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, actual_lookback = %d, latest_values:%s",
         rtable->busno, actual_lookback, b);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "max_tryct = %d, total_tryct = %d", max_tryct, total_tryct);

   if (too_many_errors(max_tryct, total_tryct, actual_lookback)) {
      if (rtable->cur_step < step_last) {
         rtable->cur_step++;
         rtable->adjustments_up++;
      }
      rtable->found_failure_step = true;
      rtable->min_ok_step = rtable->cur_step;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Set found_failure_step=true, min_ok_step=%d, cur_step=%2d",
            rtable->min_ok_step, rtable->cur_step);
   }
   else {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "Looking to decrement cur_step, cur_step=%d, found_failure_step=%s, min_ok_step = %d",
             rtable->cur_step, sbool(rtable->found_failure_step), rtable->min_ok_step);
      if (rtable->cur_step > 0) {
         if (total_tryct <= actual_lookback+1) {  // i.e. no more that 1 retry was required
            rtable->cur_step--;
            rtable->adjustments_down++;
            if (rtable->cur_step > rtable->min_ok_step)
               rtable->min_ok_step = rtable->cur_step;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Unconditionally decremented cur_step.  New value: %d", rtable->cur_step);
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
            DBGTRC(debug, DDCA_TRC_NONE, "Decremented cur_step. New value: %d", rtable->cur_step);
         }
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "rtable->cur_step = 0, cannot decrement");
      }

   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE,
               "max_tryct=%d, total_tryct=%d, rtable->cur_step=%d, rtable->min_ok_step=%d. rtable->found_failure_step=%s",
          max_tryct, total_tryct, rtable->cur_step, rtable->min_ok_step, sbool(rtable->found_failure_step) );
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
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rtable=%p, remaining_tries=%d, dsa2_enabled=%s",
         rtable, remaining_tries, sbool(dsa2_enabled));
   assert(rtable);
   int prev_step = rtable->cur_retry_loop_step;
   // has special handling for case of remaining_tries = 0;
   rtable->cur_retry_loop_step = dsa2_next_retry_step(prev_step, remaining_tries);

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
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "rtable=%p, ddcrc=%s, tries=%d dsa2_enabled=%s",
                   rtable, psc_desc(ddcrc), tries, sbool(dsa2_enabled));
   if (!dsa2_enabled) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "dsa2 not enabled");
      return;
   }
   // Per_Display_Data * pdd = pdd_get_per_display_data(dpath,  false);
   // assert(pdd);

   assert(rtable);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_retry_loop_step=%d", rtable->cur_retry_loop_step);
   if (ddcrc == 0) {
      rtable->successful_observation_ct++;
      Successful_Invocation si = {time(NULL), tries, rtable->cur_retry_loop_step};
      cirb_add(rtable->recent_values, si);
      if (tries > 3) {
         if (rtable->cur_step < step_last) {
            rtable->cur_step = rtable->cur_retry_loop_step;
            rtable->min_ok_step = rtable->cur_step;
            rtable->found_failure_step = true;
            rtable->adjustments_up++;
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
   else {
      rtable->remaining_interval = adjustment_interval;
      rtable->cur_retry_loop_step = initial_step;    // ???
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE,
               "cur_step=%d, cur_retry_loop_step=%d, min_ok_step=%d, found_failure_step=%s, remaining_interval=%d",
               rtable->cur_step, rtable->cur_retry_loop_step, rtable->min_ok_step,
               sbool(rtable->found_failure_step), rtable->remaining_interval);
}


/** Gets the current sleep multiplier value for a device
 *
 *  Converts the internal step number for the current retry loop
 *  to a floating point value.
 *
 *  @param  rtable #Results_Table for device
 *  @return multiplier value
 */
float
dsa2_get_adjusted_sleep_multiplier(Results_Table * rtable) {
   bool debug = false;
   float result = 1.0f;
   assert(rtable);
   result = steps[rtable->cur_retry_loop_step]/100.0;
   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE,
                  "rtable=%p, rtable->cur_retry_loop_step=%d, Returning %7.2f",
                  rtable,  rtable->cur_retry_loop_step, result);
   return result;
}


void dsa2_report(Results_Table * rtable, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Dynamic sleep algorithm 2 data for /dev/i2c-%d:", rtable->busno);
   rpt_vstring(d1, "Initial Step:       %3d, %5.2f millisec", rtable->initial_step, steps[rtable->initial_step]/100.0);
// rpt_vstring(d1, "Initial step from cache: %s", sbool(rtable->initial_step_from_cache));
   rpt_vstring(d1, "Final Step:         %3d, %5.2f millisec", rtable->cur_step, steps[rtable->cur_step]/100.0);
   rpt_vstring(d1, "Initial lookback ct:%3d", rtable->initial_lookback);
   rpt_vstring(d1, "Final lookback ct:  %3d", rtable->lookback);
   rpt_vstring(d1, "Adjustment interval:%3d", adjustment_interval);
   rpt_vstring(d1, "Adjustments up:     %3d", rtable->adjustments_up);
   rpt_vstring(d1, "Adjustments down:   %3d", rtable->adjustments_down);
   rpt_vstring(d1, "All tries:          %3d", rtable->successful_observation_ct);
   rpt_vstring(d1, "Retryable Failures: %3d", rtable->retryable_failure_ct);
}


void dsa2_report_all(int depth) {
   int d1 = depth+1;
   rpt_label(depth, "Dynamic Sleep Adjustment (algorithm 2)");
   for (int busno = 0; busno <= I2C_BUS_MAX; busno++) {
      Results_Table * rtable = dsa2_get_results_table_by_busno(busno, false);
      if (rtable)
         dsa2_report(rtable, d1);
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
   FILE * stats_file = fopen(stats_fn, "w");
   if (!stats_file) {
      result = -errno;
      goto bye;
   }
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened %s", stats_fn);
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx] && (results_tables[ndx]->state & RTABLE_BUS_DETECTED))
         results_tables_ct++;
   }
   DBGTRC(debug, DDCA_TRC_NONE, "results_tables_ct = %d", results_tables_ct);
   fprintf(stats_file, "FORMAT 1\n");
   fprintf(stats_file, "* DEV  /dev/i2c device\n");
   fprintf(stats_file, "* EC   EDID check sum byte\n");
   fprintf(stats_file, "* C    cur step\n");
   fprintf(stats_file, "* L    lookback\n");
   fprintf(stats_file, "* I    interval\n");
   fprintf(stats_file, "* M    minimum ok step\n");
   fprintf(stats_file, "* F    found failure step\n");
   fprintf(stats_file, "* Values {epoch_seconds, try_ct, required_step}\n");
   fprintf(stats_file, "* DEV EC C L I M F Values\n");
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx]) {
         Results_Table * rtable = results_tables[ndx];
         if (debug)
            dbgrpt_results_table(rtable, 2);
         Byte edid_ck_digit = get_edid_checkbyte(rtable->busno);
         fprintf(stats_file, "i2c-%d %02x %d %d %d %d %d",
                 rtable->busno, edid_ck_digit, rtable->cur_step, rtable->lookback, rtable->remaining_interval,
                 rtable->min_ok_step, rtable->found_failure_step);
         for (int k = 0; k < rtable->recent_values->ct; k++) {
            Successful_Invocation si = cirb_get_logical(rtable->recent_values, k);
            fprintf(stats_file, " {%ld,%d,%d}", si.epoch_seconds, si.tryct, si.required_step);
         }
         fputc('\n', stats_file);
      }
   }
   fclose(stats_file);
bye:
   free(stats_fn);
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result,
                    "Wrote %d Results_Table(s)"  , results_tables_ct);
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
   int rc = unlink(stats_fn);
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
      char * lastpos = s + strlen(s) - 1;
      if (comma_pos && comma_pos < lastpos && comma_pos2 < lastpos) {
         *comma_pos  = '\0';
         *comma_pos2 = '\0';
         *lastpos    = '\0';
         Successful_Invocation si;
         result  = str_to_long(s+1,           &si.epoch_seconds, 10);
         result &= str_to_int(comma_pos  + 1, &si.tryct,         10);
         result &= str_to_int(comma_pos2 + 1, &si.required_step, 10);
         if (result) {
            cirb_add(cirb, si);
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
   char * format_line = g_ptr_array_index(line_array, 0);
   // DBGMSG("format_line %p |%s|", format_line, format_line);
   if (!str_starts_with(format_line, "FORMAT ")) {
      stats_file_error(errmsgs, "Invalid format line: %s", format_line);
      all_ok = false;
      goto bye;
   }
   int format_id;
   char * sformat = format_line + strlen("FORMAT ");
   // DBGMSG("sformat %d %p |%s|", strlen("FORMAT "), sformat, sformat);
   bool ok = str_to_int( sformat, &format_id, 10);
   if (!ok || format_id != 1) {
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
         bool ok = (piecect >= 7);
         if (ok) {
            busno = i2c_name_to_busno(pieces[0]);
            rtable = new_results_table(busno);
            // rtable->initial_step_from_cache = true;
            ok = (busno >= 0);
         }
         int iwork;

         ok = ok && any_one_byte_hex_string_to_byte_in_buf(pieces[1], &rtable->edid_checksum_byte);
         ok = ok && str_to_int(pieces[2], &rtable->cur_step, 10);
         ok = ok && str_to_int(pieces[3], &rtable->lookback, 10);
         ok = ok && str_to_int(pieces[4], &rtable->remaining_interval, 10);
         ok = ok && str_to_int(pieces[5], &rtable->min_ok_step, 10);
         ok = ok && str_to_int(pieces[6], &iwork, 10);
         if (ok) {
            rtable->found_failure_step = (iwork);
            rtable->cur_retry_loop_step = rtable->cur_step;
            rtable->initial_step = rtable->cur_step;
            rtable->initial_lookback = rtable->lookback;
         }
         if (piecect >= 7) {   // handle no Successful_Invocation data
            for (int ndx = 7; ndx < piecect; ndx++) {
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
double logistic(double x) {
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
// Initialization
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

#ifdef UNUSED
   RTTI_ADD_FUNC(dsa2_note_retryable_failure_by_dpath);
   RTTI_ADD_FUNC(dsa2_record_final_by_dpath);
#endif

   RTTI_ADD_FUNC(dsa2_restore_persistent_stats);
   RTTI_ADD_FUNC(dsa2_save_persistent_stats);

   results_tables = calloc(I2C_BUS_MAX+1, sizeof(Results_Table*));

   // test_one_logistic(10);
   // test_dsa2_next_retry_step();
   // test_float_to_step_conversion();
}


void terminate_dsa2() { // release all resources
   if (results_tables) {
      for (int ndx = 0; ndx < I2C_BUS_MAX+1; ndx++) {
         if (results_tables[ndx])
            free_results_table(results_tables[ndx]);
      }
   }
}

