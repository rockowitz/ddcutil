/* ddc_try_stats.c
 *
 * Maintains statistics on DDC retries.
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/report_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/parms.h"

#include "ddc/ddc_try_stats.h"

#define MAX_STAT_NAME_LENGTH  31


static char * TAG_VALUE = "STAT";

static GMutex try_data_mutex;
static bool debug_mutex = false;

typedef
struct {
   char   tag[4];
   char   stat_name[MAX_STAT_NAME_LENGTH+1];
   int    max_tries;
   int    counters[MAX_MAX_TRIES+2];
} Try_Data;


// counters usage:
//  0  number of failures because of fatal errors
//  1  number of failures because retry exceeded
//  n>1 number of successes after n-1 tries,
//      e.g. if succeed after 1 try, recorded in counter 2


/* Allocates and initializes a Try_Data data structure
 * 
 * Arguments: 
 *    stat_name   name of the statistic being recorded
 *    max_tries   maximum number of tries 
 *
 * Returns: 
 *    opaque pointer to the allocated data structure
 */
void * try_data_create(char * stat_name, int max_tries) {
   assert(strlen(stat_name) <= MAX_STAT_NAME_LENGTH);
   assert(0 <= max_tries && max_tries <= MAX_MAX_TRIES);
   Try_Data* try_data = calloc(1,sizeof(Try_Data));
   memcpy(try_data->tag, TAG_VALUE,4);
   strcpy(try_data->stat_name, stat_name);
   try_data->max_tries = max_tries;
   // DBGMSG("try_data->counters[MAX_MAX_TRIES+1]=%d, MAX_MAX_TRIES=%d", try_data->counters[MAX_MAX_TRIES+1], MAX_MAX_TRIES);
   return try_data;
}


static inline Try_Data * unopaque(void * opaque_ptr) {
   Try_Data * try_data = (Try_Data*) opaque_ptr;
   assert(try_data && memcmp(try_data->tag, TAG_VALUE, 4) == 0);
   return try_data;
}

int  try_data_get_max_tries(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   return try_data->max_tries;
}

void try_data_set_max_tries(void * stats_rec, int new_max_tries) {
   bool debug = false || debug_mutex;
   DBGMSF0(debug, "Starting");

   Try_Data * try_data = unopaque(stats_rec);
   assert(new_max_tries >= 1 && new_max_tries <= MAX_MAX_TRIES);

   g_mutex_lock(&try_data_mutex);
   try_data->max_tries = new_max_tries;
   g_mutex_unlock(&try_data_mutex);

   DBGMSF0(debug, "Done");
}

void try_data_reset(void * stats_rec) {
   bool debug = false || debug_mutex;
   DBGMSF0(debug, "Starting");

   Try_Data * try_data = unopaque(stats_rec);

   g_mutex_lock(&try_data_mutex);
   for (int ndx=0; ndx < MAX_MAX_TRIES+1; ndx++)
      try_data->counters[ndx] = 0;
   g_mutex_unlock(&try_data_mutex);

   DBGMSF0(debug, "Done");
}

static void record_successful_tries(void * stats_rec, int tryct){
   bool debug = false || debug_mutex;
   DBGMSF0(debug, "Starting");

   Try_Data * try_data = unopaque(stats_rec);
   assert(0 < tryct && tryct <= try_data->max_tries);

   g_mutex_lock(&try_data_mutex);
   try_data->counters[tryct+1] += 1;
   g_mutex_unlock(&try_data_mutex);

   DBGMSF0(debug, "Done");
}

static void record_failed_max_tries(void * stats_rec) {
   bool debug = false || debug_mutex;
   DBGMSF0(debug, "Starting");

   Try_Data * try_data = unopaque(stats_rec);

   g_mutex_lock(&try_data_mutex);
   try_data->counters[1] += 1;
   g_mutex_unlock(&try_data_mutex);

   DBGMSF0(debug, "Done");
}

static void record_failed_fatally(void * stats_rec) {
   bool debug = false || debug_mutex;
    DBGMSF0(debug, "Starting");

   Try_Data * try_data = unopaque(stats_rec);

   g_mutex_lock(&try_data_mutex);
   try_data->counters[0] += 1;
   g_mutex_unlock(&try_data_mutex);

   DBGMSF0(debug, "Done");
}


void try_data_record_tries(void * stats_rec, int rc, int tryct) {
   // DBGMSG("stats_rec=%p, rc=%d, tryct=%d", stats_rec, rc, tryct);
   Try_Data * try_data = unopaque(stats_rec);
   // TODO: eliminate function calls
   if (rc == 0) {
      record_successful_tries(try_data, tryct);
   }
   // else if (tryct == try_data->max_tries) {
   // fragile, but eliminates testing for max_tries:
   else if (rc == DDCRC_RETRIES || rc == DDCRC_ALL_TRIES_ZERO) {
      record_failed_max_tries(try_data);
   }
   else {
      record_failed_fatally(try_data);
   }
}


// used to test whether there's anything to report
int try_data_get_total_attempts(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   int total_attempts = 0;
   int ndx;
   for (ndx=0; ndx <= try_data->max_tries+1; ndx++) {
      total_attempts += try_data->counters[ndx];
   }
   return total_attempts;
}


/** Reports a statistics record.
 *
 *  Output is written to the current FOUT destination.
 *
 *  \param stats_rec    opaque reference to stats record
 *  \param depth        logical indentation depth
 *
 *  \remark
 *  Why does this data structure need to be opaque?  (4/2017)
 */
void try_data_report(void * stats_rec, int depth) {
   int d1 = depth+1;
   Try_Data * try_data = unopaque(stats_rec);
   rpt_nl();
   rpt_vstring(depth, "Retry statistics for %s", try_data->stat_name);
   if (try_data_get_total_attempts(stats_rec) == 0) {
      rpt_vstring(d1, "No tries attempted");
   }
   else {
      int total_successful_attempts = 0;
      rpt_vstring(d1, "Max tries allowed: %d", try_data->max_tries);
      rpt_vstring(d1, "Successful attempts by number of tries required:");
      int ndx;
      for (ndx=2; ndx <= try_data->max_tries+1; ndx++) {
         total_successful_attempts += try_data->counters[ndx];
         // DBGMSG("ndx=%d", ndx);
         rpt_vstring(d1, "   %2d:  %3d", ndx-1, try_data->counters[ndx]);
      }
      rpt_vstring(d1, "Total:                            %3d", total_successful_attempts);
      rpt_vstring(d1, "Failed due to max tries exceeded: %3d", try_data->counters[1]);
      rpt_vstring(d1, "Failed due to fatal error:        %3d", try_data->counters[0]);
      rpt_vstring(d1, "Total attempts:                   %3d", try_data_get_total_attempts(stats_rec));
   }
}
