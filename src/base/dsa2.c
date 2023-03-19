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
#include "base/status_code_mgt.h"
#include "base/rtti.h"

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


#ifdef OLD
//
// Circular Integer Buffer
//

typedef struct {
   int *    values;
   int      size;
   int      ct;
} Circular_Integer_Buffer;


/** Allocates a new #Circular_Integer_Buffer
 *
 *  @param  size  buffer size (number of entries)
 *  @return newly allocated #Circular_Integer_Buffer
 */
Circular_Integer_Buffer *
cib_new(int size) {
   Circular_Integer_Buffer * cib = calloc(1, sizeof(Circular_Integer_Buffer));
   cib->values = calloc(size, sizeof(int));
   cib->size = size;
   cib->ct = 0;
   return cib;
}


void cib_free(Circular_Integer_Buffer * cib) {
   free(cib->values);
   free(cib);
}


/** Appends an integer to a #Circular_Integer_Buffer.
 *
 *  @param   cib   #Circular_Integer_Buffer
 *  @param   value value to append
 */
void
cib_add(Circular_Integer_Buffer * cib, int value) {
    int nextpos = cib->ct % cib->size;
    // printf("(%s) Adding at ct %d, pos %d, value %d\n", __func__, cib->ct, nextpos, value);
       cib->values[nextpos] = value;
    cib->ct++;
}


void cib_get_latest(Circular_Integer_Buffer * cib, int ct, int latest_values[]) {
   assert(ct <= cib->ct);
   int ctr = 0;

   while(ctr < ct) {int_min
      int ndx = (ctr > 0) ? (ctr-1) % cib->size : cib->size - 1;
      latest_values[ctr] = cib->values[ ndx ];
   }
}

#endif


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

typedef struct {
   Circular_Invocation_Result_Buffer * recent_values;
   // use int rather than a smaller type to simplify use of str_to_int()
   int  busno;
   int  cur_step;
   int  lookback;
   int  remaining_interval;
   int  min_ok_step;
   bool found_failure_step;
   int  cur_retry_loop_step;
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
   rtable->cur_step = initial_step;
   rtable->lookback = Default_Look_Back;
   rtable->recent_values = cirb_new(MAX_RECENT_VALUES);
   rtable->remaining_interval = Default_Interval;
   rtable->min_ok_step = 0;
   rtable->found_failure_step = false;
   return rtable;
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
static
Results_Table * get_results_table(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bussno=%d", busno);
   assert(busno <= I2C_BUS_MAX);
   Results_Table * rtable = results_tables[busno];
   if (!rtable) {
      rtable = new_results_table(busno);
      results_tables[busno] = rtable;
      rtable->cur_step = initial_step;
      rtable->cur_retry_loop_step = initial_step;
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning rtable=%p, rtable->cur_step=%d, rtable->cur_retry_loop_step=%d",
                       rtable, rtable->cur_step, rtable->cur_retry_loop_step);
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
   Results_Table * rtable = get_results_table(dpath_busno(dpath));
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
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "multiplier = %7.5f, imult = %d, step=%d, steps[%d]=%d",
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
         results_tables[ndx]->cur_step = initial_step;
         results_tables[ndx]->found_failure_step = false;
         results_tables[ndx]->min_ok_step = 0;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set initial_step=%d", initial_step);
}


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

#ifdef OLD
/**
 *  Returns the next retry loop step to use, adjusting upwards
 *  if necessary.
 *
 */
static int
next_retry_step(Results_Table * rtable, int tryctr) {
   bool debug = false;
   rtable->cur_retry_loop_ct = tryctr;
   int maxtries = 10;     // ***TEMP***  Get this from where?

   // alt rtable->cur_retry_loop_ct++;
   int remaining_steps = step_ct - rtable->cur_retry_loop_step;
   int remaining_tries = maxtries - tryctr;
   int adjustment = 0;
   if (remaining_tries < 3)
      adjustment = remaining_tries;
   else if (remaining_tries < 6)
      adjustment = remaining_steps/2;
   else
      adjustment = remaining_steps/3;
   int next_step = tryctr + adjustment;

   rtable->cur_retry_loop_step = next_step;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "tryctr-%d, maxtries=%d, remaining_tries=%d, returning %d",
                                tryctr, maxtries, remaining_tries, next_step);
   return next_step;
}
#endif


/** Calculates the step to be used on the next try loop iteration after
 *  retryable loop failure.  The step number may be incremented some
 *  amount based on the number of tries remaining.
 *
 *  @param  prev_step  number of step that failed
 *  @param  remaining_tries number of tries remaining
 *  @return step number to be used for next try loop iteration
 */
int
dsa2_next_retry_step(int prev_step, int remaining_tries)  {
   bool debug = false;

   int remaining_steps = step_ct - prev_step;
   float fadj = (1.0*remaining_steps)/remaining_tries;
   float fadj2 = fadj;
   if (fadj > .75 && fadj < 1.0)
      fadj2 = 1.0;
   int adjustment = fadj2;
   int next_step = prev_step + adjustment;
   if (next_step > step_last)
      next_step = step_last;

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "Executing prev_step=%d, remaining_tries=%d, fadj=%2.3f, fadj2=%2.3f, adjustment=%d, returning %d",
         prev_step, remaining_tries, fadj, fadj2, adjustment, next_step);
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
adjust_for_recent_successes(Results_Table * rtable) {
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
            if (rtable->cur_step > rtable->min_ok_step)
               rtable->min_ok_step = rtable->cur_step;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Unconditionally decremented cur_step.  New value: %d", rtable->cur_step);
         }
         else if (rtable->found_failure_step) {
            if (rtable->cur_step > rtable->min_ok_step) {
               rtable->cur_step--;
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
 *  @param dpath             device path
 *  @param remaining_tries   number of tries remaining
 */
void
dsa2_note_retryable_failure(DDCA_IO_Path dpath, int remaining_tries) {
   assert(dpath.io_mode == DDCA_IO_I2C);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, remaining_tries=%d, dsa2_enabled=%s",
         dpath_repr_t(&dpath), remaining_tries, sbool(dsa2_enabled));
   if (!dsa2_enabled) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "dsa2 not enabled");
      return;
   }

   Results_Table * rtable = get_results_table(dpath.path.i2c_busno);
   int prev_step = rtable->cur_retry_loop_step;
   rtable->cur_retry_loop_step = dsa2_next_retry_step(prev_step, remaining_tries);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Previous step=%d, next step = %d",
                                     prev_step, rtable->cur_retry_loop_step);
}


/** Called after all (possible) retries in #ddc_write_read_with_retry()
 *
 *  If ddcrw = 0 (i.e. the operation succeeded, which is the normal case)
 *  a #Successful_Invocation record is added to the Circular Invocation
 *  Response buffer. The results table for the bus is updated.
 *  Depending on how many tries were required, the current step
 *  may be adjusted up or down. The cur_retry_loop_step is reset to the
 *  (possibly update) cur_loop_step, ready to be used on the next
 *  #ddc_write_read_with_retry() operation.
 *
 *  If ddcrw != 0 (the operation failed, either because of a fatal error
 *  or retries exhausted) it's not clear what to do.  Currently just
 *  cur_retry_loop_step is set to the global initial_step.
 *
 *  @param  dpath   device path
 *  @param  ddcrw   #ddc_write_read_with_retry() return code
 *  @param  tries   number of tries used, always < max tries for success,
 *                  always max tries for retries exhausted, and either
 *                  in case of a fatal error of some sort
 */
void
dsa2_record_final(DDCA_IO_Path dpath, DDCA_Status ddcrw, int tries) {
   assert(dpath.io_mode == DDCA_IO_I2C);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, ddcrw=%s, tries=%d dsa2_enabled=%s",
                   dpath_repr_t(&dpath), psc_desc(ddcrw), tries, sbool(dsa2_enabled));
   if (!dsa2_enabled) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "dsa2 not enabled");
      return;
   }

   Results_Table * rtable = get_results_table(dpath.path.i2c_busno);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_retry_loop_step=%d", rtable->cur_retry_loop_step);
   if (ddcrw == 0) {
      Successful_Invocation si = {time(NULL), tries, rtable->cur_retry_loop_step};
      cirb_add(rtable->recent_values, si);
      if (tries > 3) {
         if (rtable->cur_step < step_last) {
            rtable->cur_step = rtable->cur_retry_loop_step;
            rtable->min_ok_step = rtable->cur_step;
            rtable->found_failure_step = true;
         }
      }
      else if (tries > 2) {
         rtable->remaining_interval -= 1;
         if (rtable->remaining_interval == 0) {
            adjust_for_recent_successes(rtable);
            rtable->remaining_interval = adjustment_interval;
         }
      }
      else {
         adjust_for_recent_successes(rtable);
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


#ifdef OLD
void
dsa2_record_ddcrw_status_code(DDCA_IO_Path dpath,
                              int          retries,
                              DDCA_Status  ddcrc,
                              bool         retryable)
{
   assert(dpath.io_mode == DDCA_IO_I2C);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, retries=%d, ddcrc=%s, retryable=%s, dsa2_enabled=%s",
         dpath_repr_t(&dpath), retries, psc_desc(ddcrc), sbool(retryable), sbool(dsa2_enabled));
   if (!dsa2_enabled) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, " ");
      return;
   }

   Results_Table * rtable = get_results_table(dpath.path.i2c_busno);
   if (ddcrc == 0) {
      Successful_Invocation si = {time(NULL), retries, rtable->cur_retry_loop_step};
      cirb_add(rtable->recent_values, si);
      if (rtable->cur_retry_loop_ct > 3) {
         if (rtable->cur_step < step_last) {
            rtable->cur_step = rtable->cur_retry_loop_step+1;
            rtable->min_ok_step = rtable->cur_step;
            rtable->found_failure_step = true;
         }
      }
      else if (rtable->cur_retry_loop_ct > 2) {
         rtable->remaining_interval -= 1;
         if (rtable->remaining_interval == 0) {
            adjust_for_recent_successes(rtable);
            rtable->remaining_interval = adjustment_interval;
         }
      }
      else {
         adjust_for_recent_successes(rtable);
         rtable->remaining_interval = adjustment_interval;
      }
      rtable->cur_retry_loop_ct = 0;
      rtable->cur_retry_loop_step = rtable->cur_step;
   }
   else if (retryable) {
      rtable->cur_retry_loop_step = next_retry_step(rtable, retries);
   }
   else {
      // It's fatal.  Caller should be giving up.
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "rtable->cur_step = %d, rtable->cur_retry_loop_step=%d, rtable->found_failure_step=%s",
         rtable->cur_step, rtable->cur_retry_loop_step, sbool(rtable->found_failure_step));
}


void
dsa2_record_ddcrw_status_code_by_dh(Display_Handle * dh,
                                    int              retries,
                                    DDCA_Status      ddcrc,
                                    bool             retryable)
{
   dsa2_record_ddcrw_status_code(dh->dref->io_path, retries, ddcrc, retryable);
}
#endif

#ifdef UNUSED
int dsa2_get_base_sleep_multiplier_step(DDCA_IO_Path dpath) {
   bool debug = false;
   Results_Table * rtable = get_results_table(dpath_busno(dpath));
   // in case called for usb device, or dsa2 not initialized
   // int result = (rtable) ? steps[rtable->cur_retry_loop_step]: 1;
   int result = (rtable) ? steps[rtable->cur_step]: 1;
   DBGTRC(debug, DDCA_TRC_NONE,
                 "Executing dpath=%s, rtable->cur_retry_loop_step=%d",
                 dpath_repr_t(&dpath),  rtable->cur_retry_loop_step);
   return result;
}
#endif

#ifdef UNUSED
int dsa2_get_sleep_multiplier_step(DDCA_IO_Path dpath) {
   bool debug = false;
   Results_Table * rtable = get_results_table(dpath_busno(dpath));
   // in case called for usb device, or dsa2 not initialized
   int result = (rtable) ? steps[rtable->cur_retry_loop_step]: 1;
   DBGTRC(debug, DDCA_TRC_NONE,
                 "Executing dpath=%s, rtable->cur_retry_loop_step=%d",
                 dpath_repr_t(&dpath),  rtable->cur_retry_loop_step);
   return result;
}
#endif

#ifdef UNUSED
float dsa2_multiplier_step_to_float(int step) {
   bool debug = false;
   int iresult = 0;
   if (step < 0)
      iresult = steps[0];
   else if (step > step_last)
      iresult = steps[step_last];
   else
      iresult = steps[step];
   float fresult = iresult/100.0;
   DBGMSF(debug, "step=%d, returning %7.3f", step, fresult);
   return fresult;
}
#endif


/** Gets the current sleep multiplier value for a device
 *
 *  Converts the internal step number for the current retry loop
 *  to a floating point value.
 *
 *  @param  dpath  io path
 *  @return multiplier value
 */
float
dsa2_get_sleep_multiplier(DDCA_IO_Path dpath) {
   bool debug = false;
   float result = 1.0f;
   if (dpath.io_mode == DDCA_IO_I2C) {   // in case called for usb device, or dsa2 not initialized
      Results_Table * rtable = get_results_table(dpath_busno(dpath));
      assert(rtable);
      result = steps[rtable->cur_retry_loop_step]/100.0;
      DBGTRC(debug, DDCA_TRC_NONE,
                   "Executing dpath=%s, rtable->cur_retry_loop_step=%d, Returning %7.2f",
                   dpath_repr_t(&dpath),  rtable->cur_retry_loop_step, result);
   }
   return result;
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
   char * stats_fn = stats_cache_file_name();
   FILE * stats_file = fopen(stats_fn, "w");
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened %s", stats_fn);
   int result = 0;
   int results_tables_ct = 0;
   if (!stats_file) {
      result = -errno;
      goto bye;
   }
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx])
         results_tables_ct++;
   }
   DBGTRC(debug, DDCA_TRC_NONE, "results_tables_ct = %d", results_tables_ct);
   fprintf(stats_file, "FORMAT 1\n");
   fprintf(stats_file, "* bus cur_step lookback interval min_ok_step"
                       " found_failure_step {epoch_seconds, try_ct, required_step} ... \n");
   for (int ndx = 0; ndx < I2C_BUS_MAX; ndx++) {
      if (results_tables[ndx]) {
         Results_Table * rtable = results_tables[ndx];
         fprintf(stats_file, "i2c-%d %d %d %d %d %d",
                 rtable->busno, rtable->cur_step, rtable->lookback, rtable->remaining_interval,
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
stats_file_error(char * format, ...) {
   va_list(args);
   va_start(args, format);
   char buffer[200];
   vsnprintf(buffer, sizeof(buffer), format, args);
   DBGMSG(buffer);
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
bool
dsa2_restore_persistent_stats() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   char * stats_fn = stats_cache_file_name();
   // DBGMSG("stats_fn=%s", stats_fn);
   bool all_ok = true;
   GPtrArray* line_array = g_ptr_array_new_with_free_func(g_free);
   int linect = file_getlines(stats_fn, line_array, debug);
   if (linect == -ENOENT)
      goto bye;
   if (linect < 0) {
      stats_file_error("Error  %s reading stats file %s", psc_desc(linect), stats_fn);
      all_ok = false;
      goto bye;
   }
   if (linect == 0)  { // empty file
      stats_file_error("Empty stats file");
      goto bye;
   }
   char * format_line = g_ptr_array_index(line_array, 0);
   // DBGMSG("format_line %p |%s|", format_line, format_line);
   if (!str_starts_with(format_line, "FORMAT ")) {
      stats_file_error("Invalid format line: %s", format_line);
      all_ok = -1;
      goto bye;
   }
   int format_id;
   char * sformat = format_line + strlen("FORMAT ");
   // DBGMSG("sformat %d %p |%s|", strlen("FORMAT "), sformat, sformat);
   bool ok = str_to_int( sformat, &format_id, 10);
   if (!ok || format_id != 1) {
      stats_file_error("Invalid format: %s", sformat);
      all_ok = -1;
      goto bye;
   }

#ifdef REGEX
   const char * dev_stats_pattern = "^i2c-([0-9]+)\\s+"
         "([0-9]+)\\s+([0-9]+)\\s+([0-9])+\\s+([0-9]+)\\s+([0-9]+)\\s+(-?[0-9]+\\s+"
         "(?:[{][0-9]+,[0-9]+[}])$";
   regex_t * re = calloc(1, sizeof(regex_t));
   if (debug)
      printf("(%s) Allocated regex %p, compiling...\n", __func__, (void*)re);
   int rc = regcomp(re, dev_stats_pattern, REG_EXTENDED);
   if (rc != 0) {
      printf("(%s) regcomp() returned %d\n", __func__, rc);
      char buffer[100];
      regerror(rc, re, buffer, 100);
      printf("regcomp() failed with '%s'\n", buffer);


      assert(rc == 0);
   }
   regmatch_t matches[100];
   for (int linendx; linendx < line_array->len; linendx++) {
      char * cur_line = g_ptr_array_index(line_array, linendx);
      regmatch_t matches[100];
      int rc = regexec(
             re,                  /* the compiled pattern */
             cur_line,                /* the subject string */
             100,     // absurdly large number of matches
             matches,
             0
          );
      if (rc != 0) {
         stats_file_error("Invalid stats file line: %s", cur_line);
      }
      else {
         int matchndx = 0;
         while (matches[matchndx].rm_so != -1) {
            regmatch_t cur_match = matches[matchndx];
            printf("%*s\n", cur_match.rm_eo-cur_match.rm_so, cur_line + cur_match.rm_so);
         }
      }
   }
#endif

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
            ok = (busno >= 0);
         }
         int iwork;
         ok = ok && str_to_int(pieces[1], &rtable->cur_step, 10);
         ok = ok && str_to_int(pieces[2], &rtable->lookback, 10);
         ok = ok && str_to_int(pieces[3], &rtable->remaining_interval, 10);
         ok = ok && str_to_int(pieces[4], &rtable->min_ok_step, 10);
         ok = ok && str_to_int(pieces[5], &iwork, 10);
         if (ok)
            rtable->found_failure_step = (iwork);
         if (ok) {
            rtable->cur_retry_loop_step = rtable->cur_step;
         }
         for (int ndx = 6; ndx < piecect; ndx++) {
            ok = ok && cirb_parse_and_add(rtable->recent_values, pieces[ndx]);
         }
         if (!ok) {
            all_ok = false;
            stats_file_error("Invalid: %s", cur_line);
            free_results_table(rtable);
         }
         else {
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
  free(stats_fn);
  g_ptr_array_free(line_array, true);
  DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, all_ok, "");
  return all_ok;
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
   RTTI_ADD_FUNC(adjust_for_recent_successes);
   RTTI_ADD_FUNC(dsa2_record_final);
   RTTI_ADD_FUNC(dsa2_note_retryable_failure);
   RTTI_ADD_FUNC(dsa2_reset_multiplier);
   RTTI_ADD_FUNC(dsa2_get_sleep_multiplier);
   RTTI_ADD_FUNC(dsa2_save_persistent_stats);
   RTTI_ADD_FUNC(dsa2_erase_persistent_stats);
   RTTI_ADD_FUNC(dsa2_restore_persistent_stats);

   results_tables = calloc(I2C_BUS_MAX+1, sizeof(Results_Table*));

   // test_one_logistic(10);
   // test_dsa2_next_retry_step();
   // test_float_to_step_conversion();
}
