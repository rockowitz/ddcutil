// dsa2.c

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE    // for localtime_r()

#include <assert.h>
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
#include "util/xdg_util.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"
// #include "base/persistent_stats.h"
#include "base/status_code_mgt.h"
#include "base/rtti.h"

#include "dsa2.h"


bool dsa2_enabled = false;

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
// Circular Struct Buffer
//

typedef struct {
   time_t epoch_seconds;    // timestamp to aid in development
   int    tryct;            // how many tries
   int    required_step;    // step level required
} Successful_Invocation;


typedef struct {
   Successful_Invocation *    values;
   int      size;     // size of values[]
   int      ct;       // number of values used: 0..size
   int      nextpos;  // index to next write to
} Circular_Invocation_Result_Buffer;


/** Allocates a new #Circular_Invocation_Result_Buffer
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

static void
cirb_free(Circular_Invocation_Result_Buffer * cirb) {
   free(cirb->values);
   free(cirb);
}

/** Appends a #Successful_Invocation struct to a #Circular_Invocation_Result_Buffer.
 *
 *  @param   cirb   #Circular_Integer_Buffer
 *  @param   value value to append
 */

static void
cirb_add(Circular_Invocation_Result_Buffer *cirb, Successful_Invocation value) {
    cirb->values[cirb->nextpos] = value;
    cirb->nextpos = (cirb->nextpos+1) % cirb->size;
    if (cirb->ct < cirb->size)
       cirb->ct++;
}


static int
cirb_logical_to_physical_index(Circular_Invocation_Result_Buffer *cirb, int logical) {
   int physical = -1;
   if (logical < cirb->ct) {
      physical = (cirb->ct <= cirb->size)
                       ? logical
                       : (cirb->nextpos +logical) % cirb->size;
   }
   return physical;
}


static Successful_Invocation
cirb_get_logical(Circular_Invocation_Result_Buffer *cirb, int logical) {
   int physical = cirb_logical_to_physical_index(cirb, logical);
   Successful_Invocation result = {-1,-1};
   if (physical >= 0)
      result = cirb->values[physical];
   return result;
}


// static
Successful_Invocation
cirb_get_by_index(Circular_Invocation_Result_Buffer * cirb, int index) {
   Successful_Invocation result = {-1,-1};
   if (index < cirb->ct) {
      int physical_index = (cirb->ct <= cirb->size) ? index : (cirb->nextpos + index) % cirb->size;
      result = cirb->values[physical_index];
   }
   return result;
}

#ifdef UNUSED
int * cirb_linear_indexes(Circular_Invocation_Result_Buffer  * cirb) {
   int * result = calloc(cirb->ct+1, sizeof(int));
   int ndx = 0;
   for (; ndx < cirb->ct; ndx++) {
      result[ndx] = (cirb->ct <= cirb->size) ? ndx : (cirb->nextpos + ndx) % cirb->size;
   }
   result[ndx] = -1;
   return result;
}
#endif



static void
cirb_get_latest(Circular_Invocation_Result_Buffer * cirb,
                int ct,
                Successful_Invocation latest_values[])
{
   assert(ct <= cirb->ct);
   int skipct = cirb->ct - ct;
   for (int ctr = 0; ctr < ct; ctr++) {
      latest_values[ctr] = cirb_get_logical(cirb, ctr+skipct);
   }
}


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
      rpt_vstring(d2, "values[%2d], tryct = %d, required_step=%d",
            ndx, cirb->values[ndx].tryct, cirb->values[ndx].required_step);
   }
   rpt_label(d1, "Values by latest: ");
   for (int ndx = 0; ndx < cirb->ct; ndx++) {
      int physical = cirb_logical_to_physical_index(cirb, ndx);
      Successful_Invocation si = cirb_get_logical(cirb, ndx);

      struct tm broken_down_time;
      localtime_r(&si.epoch_seconds, &broken_down_time);
      char time_buf[40];
      strftime(time_buf, 40, "%b %d %T", &broken_down_time);
      rpt_vstring(d2, "logical index: %2d, physical index: %2d, tryct = %d, required_step=%d, timestamp=%s",
                      ndx, physical, si.tryct, si.required_step, time_buf);
   }
}


//
// Results Tables
//

#define MAX_RECENT_VALUES 20
#define MAX_BUS 31

int  steps[] = {0,5,10,20,30,50,70,100,130, 160, 200};    // multiplier * 100
int step_ct = ARRAY_SIZE(steps);   //11

typedef struct {
   Circular_Invocation_Result_Buffer * recent_values;
   // use int rather than a smaller type to simplify use of str_to_int()
   int  busno;
   int  cur_step;
   int  lookback;
   int  interval;
   int  min_ok_step;
   bool found_failure_step;

   // DDCA_Status cur_retry_loop_status[MAX_MAX_TRIES];
   int  cur_retry_loop_step;
   int  cur_retry_loop_ct;
} Results_Table;


static void
dbgrpt_results_table(Results_Table * rtable, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Results_Table", rtable, depth);
#define ONE_INT_FIELD(_name) rpt_int(#_name, NULL, rtable->_name, d1)
   ONE_INT_FIELD(busno);
   ONE_INT_FIELD(cur_step);
   ONE_INT_FIELD(lookback);
   ONE_INT_FIELD(interval);
   rpt_bool("found_failure_step", NULL, rtable->found_failure_step, d1);
   ONE_INT_FIELD(cur_retry_loop_step);
   ONE_INT_FIELD(cur_retry_loop_ct);
#undef ONE_INT_FIELD
   dbgrpt_circular_invocation_results_buffer(rtable->recent_values, d1);
}


Results_Table ** results_tables;

int Default_Look_Back = 5;
const int Default_Initial_Step = 7;  // 1.0
int initial_step  = Default_Initial_Step;
int Max_Recent_Values = 20;
int Default_Interval  = 3;
// assert(Default_Look_Back <= Max_Recent_Values);


static
int multiplier_to_step(float multiplier) {
   bool debug = true;
   int imult = multiplier * 100;

   int ndx = 0;
   for (; ndx < step_ct; ndx++) {
   //    if ( (steps[ndx]-.01) < multiplier) {
   //       break;
      if ( steps[ndx] >= imult ) {
               break;
      }
   }

   int step = (ndx == step_ct) ? step_ct-1 : ndx;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "multiplier = %7.5f, imult = %d, step=%d, steps[%d]=%d",
                                         multiplier, imult, step, step, steps[step]);
   return step;
}


void dsa2_reset_multiplier(float multiplier) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "multiplier=%7.3f", multiplier);
   initial_step = multiplier_to_step(multiplier);
   for (int ndx = 0; ndx < MAX_BUS; ndx++) {
      if (results_tables[ndx]) {
         results_tables[ndx]->cur_step = initial_step;
         results_tables[ndx]->found_failure_step = false;
         results_tables[ndx]->min_ok_step = 0;
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set initial_step=%d", initial_step);
}


static
Results_Table * new_results_table(int busno) {
   Results_Table * rtable = calloc(1, sizeof(Results_Table));
   rtable->busno = busno;
   rtable->cur_step = initial_step;
   rtable->lookback = Default_Look_Back;
   rtable->recent_values = cirb_new(MAX_RECENT_VALUES);
   rtable->interval = Default_Interval;
   rtable->min_ok_step = 0;;
   return rtable;
}

static void
free_results_table(Results_Table * rtable) {
   if (rtable) {
      if (rtable->recent_values)
         cirb_free(rtable->recent_values);
      free(rtable);
   }
}


static
Results_Table * get_results_table(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "bussno=%d", busno);
   assert(busno <= MAX_BUS);
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


// static
void set_multiplier(Results_Table * rtable, float multiplier) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "multiplier=%7.3f", multiplier);
   rtable->cur_step = multiplier_to_step(multiplier);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set cur_step=%d", initial_step);
}


void
dsa2_set_multiplier_by_path(DDCA_IO_Path dpath, float multiplier) {
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, multiplier=%7.3f", dpath_repr_t(&dpath), multiplier);
   Results_Table * rtable = get_results_table(dpath_busno(dpath));
   rtable->cur_step = multiplier_to_step(multiplier);
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Set cur_step=%d", initial_step);
}


//
// The Algorithm
//

static bool
too_many_errors(int max_tryct, int total_tryct, int interval) {
   bool debug = true;
   bool result = false;
   if (max_tryct > 2)
      result = true;
   else {
      if (total_tryct * 10 / interval > 5) {   // i.e. total_tryct/interval > .5)
         result = true;
      }
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "max_tryct=%d, total_tryct=%d, interval=%d, Returning: %s",
                             max_tryct, total_tryct, interval, sbool(result));
   return result;
}

/**
 *  Returns the next retry loop step to use, adjusting upwards
 *  if necessary.
 *
 */
static int
next_retry_step(Results_Table * rtable, int tryctr) {
   bool debug = true;
   rtable->cur_retry_loop_ct = tryctr;
   int maxtries = 10;     // ***TEMP***  Get this from where?

   // alt rtable->cur_retry_loop_ct++;
   int remaining_steps = step_ct - rtable->cur_retry_loop_step;
   int remaining_tries = maxtries - tryctr;
   int last_step = step_ct - 1;
   int next_step = -1;
   if (remaining_tries < 3)
      next_step = last_step;
   else if (remaining_tries < 6)
      next_step = remaining_steps/2;
   else
      next_step = remaining_steps/3;

   rtable->cur_retry_loop_step = next_step;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "tryctr-%d, maxtries=%d, remaining_tries=%d, returning %d",
                                tryctr, maxtries, remaining_tries, next_step);
   return next_step;
}


static void
adjust_for_recent_successes(Results_Table * rtable) {
   bool debug = true;

   Successful_Invocation latest_values[MAX_RECENT_VALUES];
   cirb_get_latest(rtable->recent_values, rtable->lookback, latest_values);
   int max_tryct = 0;
   int total_tryct = 0;
   char  b[300];
   for (int ndx = 0; ndx < rtable->lookback; ndx++) {
      sprintf(b + strlen(b), "%s{%d,%d}",
            (ndx > 0) ? ", " : "",
            latest_values[ndx].tryct, latest_values[ndx].required_step);

      total_tryct += latest_values[ndx].tryct;
      if (latest_values[ndx].tryct > max_tryct)
            max_tryct = latest_values[ndx].tryct;
   }
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, latest_values:%s", rtable->busno, b);

   if (too_many_errors(max_tryct, total_tryct, rtable->lookback)) {
      if (rtable->cur_step < ARRAY_SIZE(steps) - 2) {
         rtable->cur_step++;
      }
      else {
         assert(rtable->cur_step == ARRAY_SIZE(steps) -1);
      }
      rtable->found_failure_step = true;
      rtable->min_ok_step = rtable->cur_step;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Set found_failure_step=true, min_ok_step=%d",
               rtable->min_ok_step);
   }
   else {
      if (!rtable->found_failure_step && rtable->cur_step > 0) {
            rtable->cur_step--;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Decremented rtable->cur_step.  Now %d",
                  rtable->cur_step);
      }
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE,
               "max_tryct=%d, total_tryct=%d, rtable->cur_step=%d, rtable->min_ok_step=%d. rtable->found_failure_step=%s",
          max_tryct, total_tryct, rtable->cur_step, rtable->min_ok_step, sbool(rtable->found_failure_step) );
}


void
dsa2_record_ddcrw_status_code(DDCA_IO_Path dpath,
                              int tryctr,
                              DDCA_Status ddcrc,
                              bool retryable)
{
   assert(dpath.io_mode == DDCA_IO_I2C);
   bool debug = true;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, tryctr=%d, ddcrc=%s, retryable=%s, dsa2_enabled=%s",
         dpath_repr_t(&dpath), tryctr, psc_desc(ddcrc), sbool(retryable), sbool(dsa2_enabled));

   if (!dsa2_enabled) {
      DBGTRC_DONE(debug, DDCA_TRC_NONE, " ");
      return;
   }

   // get Results_Table for device dpath
   int busno = dpath.path.i2c_busno;

   Results_Table * rtable = get_results_table(busno);
   // rtable->cur_retry_loop_status[rtable->cur_retry_loop_ct++];

   if (ddcrc == 0) {
      Successful_Invocation si = {time(NULL), tryctr, rtable->cur_retry_loop_step};
      cirb_add(rtable->recent_values, si);
      if (rtable->cur_retry_loop_ct > 2) {
         rtable->cur_step = rtable->cur_retry_loop_step;
         rtable->found_failure_step = true;
      }
      else if (rtable->cur_retry_loop_ct == 2) {
         if (--rtable->interval == 0) {

            adjust_for_recent_successes(rtable);


            rtable->interval = Default_Interval;
         }
      }
      rtable->cur_retry_loop_ct = 0;
      rtable->cur_retry_loop_step = rtable->cur_step;
   }
   else if (retryable) {
      rtable->cur_retry_loop_step = next_retry_step(rtable, tryctr);
   }
   else {
      // ???
   }


#ifdef TO_REWORK
   if (ddcrc == DDCRC_RETRIES) {
      DBGTRC(true, DDCA_TRC_NONE, "DDCRC_RETRIES detected");
      rtable->cur_step = initial_step;  // ???
      rtable->found_failure_step = true;
   }
   else if (ddcrc != 0 ) {
      DBGTRC(true, DDCA_TRC_NONE, "Fatal error. ddcrc = %s", psc_desc(ddcrc));
      rtable->cur_step = initial_step;  // ???
      rtable->found_failure_step = true;
   }
   else {
      int retry_ct = try_ct;
      cib_add(rtable->recent_values, retry_ct);

      if (--rtable->interval == 0) {
         int latest_values[MAX_RECENT_VALUES];
         cib_get_latest(rtable->recent_values, rtable->lookback, latest_values);
         int max_tryct = 0;
         int total_tryct = 0;
         for (int ndx = 0; ndx < rtable->lookback; ndx++) {
            total_tryct += latest_values[ndx];
            if (latest_values[ndx] > max_tryct)
                  max_tryct = latest_values[ndx];
         }
         if (too_many_errors(max_tryct, total_tryct, rtable->lookback)) {
            if (rtable->cur_step < ARRAY_SIZE(steps) - 2) {
               rtable->cur_step++;
               rtable->found_failure_step = true;
            }
            else {
               if (!rtable->found_failure_step && rtable->cur_step > 0) {
                  rtable->cur_step--;
               }
            }
         }
         rtable->interval = Default_Interval;
      }
   }
#endif

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "rtable->cur_step = %d, rtable->cur_retry_loop_step=%d, rtable->found_failure_step=%s",
         rtable->cur_step, rtable->cur_retry_loop_step, sbool(rtable->found_failure_step));
}


void
dsa2_record_ddcrw_status_code_by_dh(Display_Handle * dh,
                                    int tryctr,
                                    DDCA_Status ddcrc,
                                    bool retryable)
{
   dsa2_record_ddcrw_status_code(dh->dref->io_path, tryctr, ddcrc, retryable);
}


/** Gets the current sleep multiplier value for a device
 *
 *  @param  dpath  io path
 *  @return multiplier value
 */
float
dsa2_get_sleep_multiplier(DDCA_IO_Path dpath) {
   bool debug = true;
   Results_Table * rtable = get_results_table(dpath_busno(dpath));
   // in case called for usb device, or dsa2 not initialized
   float result = (rtable) ? steps[rtable->cur_retry_loop_step]/100.0 : 1.0f;
   DBGTRC(debug, DDCA_TRC_NONE,
                 "Executing. dpath=%s, rtable->cur_retry_loop_step=%d, Returning %7.2f",
                 dpath_repr_t(&dpath),  rtable->cur_retry_loop_step, result);
   return result;
}


//
// Persistent Stats
//

/** Returns the name of the file that stores persistent stats
 *
 *  \return name of file, normally $HOME/.cache/ddcutil/stats
 */
/* caller is responsible for freeing returned value */
static char *
stats_cache_file_name() {
   return xdg_cache_home_file("ddcutil", "stats");
}


/** Saves the current performance statistics in file ddcutil/stats
 *  within the user's XDG cache directory, typically $HOME/.cache.
 *
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
   if (!stats_file) {
      result = -errno;
      goto bye;
   }
   int results_tables_ct = 0;
   for (int ndx = 0; ndx < MAX_BUS; ndx++) {
      if (results_tables[ndx])
         results_tables_ct++;
   }
   DBGTRC(debug, DDCA_TRC_NONE, "results_tables_ct = %d", results_tables_ct);
   fprintf(stats_file, "FORMAT 1\n");
   fprintf(stats_file, "* bus cur_step lookback interval min_ok_step"
                       " found_failure_step {try_ct, required_step} ... \n");
   for (int ndx = 0; ndx < MAX_BUS; ndx++) {
      if (results_tables[ndx]) {
         Results_Table * rtable = results_tables[ndx];
         fprintf(stats_file, "i2c-%d %d %d %d %d %d",
                 rtable->busno, rtable->cur_step, rtable->lookback, rtable->interval,
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
 *  @return -errno if deletion fails for any reason other than non-existence.
 */
Status_Errno
dsa2_erase_persistent_stats() {
   bool debug = true;
   Status_Errno result = 0;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   char * stats_fn = stats_cache_file_name();
   int rc = unlink(stats_fn);
   if (rc < 0 && errno != ENOENT)
      result = -errno;
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result, "");
   return result;
}


static void
stats_file_error(char * format, ...) {
   va_list(args);
   va_start(args, format);
   // if (debug)
   //    printf("(%s) &args=%p, args=%p\n", __func__, &args, args);
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
   if ( strlen(segment) >= 5 &&
        segment[0] == '{'    &&
        segment[strlen(segment)-1] == '}' )
   {
      char * s = g_strdup(segment);
      char * comma_pos = strchr(s, ',');
      char * comma_pos2 = strchr(comma_pos+1, ',');
      char * lastpos = s + strlen(s) - 1;
      if (comma_pos && comma_pos < lastpos) {
         *comma_pos  = '\0';
         *comma_pos2 = '\0';
         *lastpos    = '\0';
         result = true;
         Successful_Invocation si;
         result  = str_to_long(s+1,            &si.epoch_seconds, 10);
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


bool
dsa2_restore_persistent_stats() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   char * stats_fn = stats_cache_file_name();
   // DBGMSG("stats_fn=%s", stats_fn);
   bool all_ok = true;
   GPtrArray* line_array = g_ptr_array_new_with_free_func(g_free);
   int linect = file_getlines(stats_fn, line_array, debug);
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
         // DBGMSG("ntsa_length(pieces) = %d", piecect);
         // for (int ndx = 0; ndx < piecect; ndx++) {
         //    DBGMSG("   %s", pieces[ndx]);
         // }

         // rtable->busno, rtable->cur_step, rtable->lookback, rtable->interval, rtable->min_ok_step, rtable->found_failure_step);

         int busno = -1;
         Results_Table * rtable = NULL;
         // DBGMSG("piecect = %d", piecect);
         bool ok = (piecect >= 7);
         if (ok) {
            busno = i2c_name_to_busno(pieces[0]);
            rtable = new_results_table(busno);
            // DBGMSG("busno = %d", busno);
            ok = (busno >= 0);
         }
         int iwork;
         ok = ok && str_to_int(pieces[1], &rtable->cur_step, 10);
         ok = ok && str_to_int(pieces[2], &rtable->lookback, 10);
         ok = ok && str_to_int(pieces[3], &rtable->interval, 10);
         ok = ok && str_to_int(pieces[4], &rtable->min_ok_step, 10);
         ok = ok && str_to_int(pieces[5], &iwork, 10);
         // DBGMSG("A");
         // todo assign additional fields
         if (ok)
            rtable->found_failure_step = (iwork);
         if (ok) {
            rtable->cur_retry_loop_ct = 0;
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
      }
   }

   if (!all_ok) {
      for (int ndx = 0; ndx <= MAX_BUS; ndx++) {
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


//
// Initialization
//


void
init_dsa2() {
   RTTI_ADD_FUNC(dsa2_record_ddcrw_status_code);

   results_tables = calloc(MAX_BUS+1, sizeof(Results_Table*));
}
