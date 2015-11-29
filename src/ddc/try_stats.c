/*  try_stats.c
 *
 *  Created on: Jul 15, 2014
 *      Author: rock
 *
 *  Maintains statistics on DDC retries.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/parms.h"

#include "ddc/try_stats.h"

#define MAX_STAT_NAME_LENGTH  31


static char * TAG_VALUE = "STAT";

typedef
struct {
   char   tag[4];
   char   stat_name[MAX_STAT_NAME_LENGTH+1];
   int    max_tries;
   int    counters[MAX_MAX_TRIES+1];
} Try_Data;


/* Allocates and initializes a Try_Data data structure . 
 * 
 * Arguments: 
 *    stat_name   name of the statistic being recorded
 *    max_tries   maximum number of tries 
 *
 * Returns: 
 *    opaque pointer to the allocated data structure
 */
void * create_try_data(char * stat_name, int max_tries) {
   assert(strlen(stat_name) <= MAX_STAT_NAME_LENGTH);
   assert(0 <= max_tries && max_tries <= MAX_MAX_TRIES);
   Try_Data* try_data = calloc(1,sizeof(Try_Data));
   memcpy(try_data->tag, TAG_VALUE,4);
   strcpy(try_data->stat_name, stat_name);
   try_data->max_tries = max_tries;
   return try_data;
}


static Try_Data * unopaque(void * opaque_ptr) {
   Try_Data * try_data = (Try_Data*) opaque_ptr;
   assert(memcmp(try_data->tag, TAG_VALUE, 4) == 0);
   return try_data;
}

void reset_try_data(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   try_data->counters[try_data->max_tries] = 0;
}

static void record_successful_tries(void * stats_rec, int tryct){
   Try_Data * try_data = unopaque(stats_rec);
   assert(0 < tryct && tryct <= try_data->max_tries);
   try_data->counters[tryct] += 1;
}

static void record_failed_max_tries(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   try_data->counters[try_data->max_tries+1] += 1;
}

static void record_failed_fatally(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   try_data->counters[0] += 1;
}


void record_tries(void * stats_rec, int rc, int tryct) {
   // printf("(%s) stats_rec=%p, rc=%d, tryct=%d\n", __func__, stats_rec, rc, tryct);
   Try_Data * try_data = unopaque(stats_rec);
   // TODO: eliminate function calls
   if (rc == 0) {
      record_successful_tries(try_data, tryct);
   }
   else if (tryct == try_data->max_tries) {
      record_failed_max_tries(try_data);
   }
   else {
      record_failed_fatally(try_data);
   }
}

// used to test whether there's anything to report
int get_total_tries(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   int total_tries = 0;
   int ndx;
   for (ndx=1; ndx <= try_data->max_tries; ndx++) {
      total_tries += try_data->counters[ndx];
   }
   total_tries += try_data->counters[try_data->max_tries+1];
   total_tries += try_data->counters[0];

   return total_tries;
}

void report_try_data(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   printf("\nRetry statistics for %s\n", try_data->stat_name);
   if (get_total_tries(stats_rec) == 0) {
      printf("   No tries attempted\n");
   }
   else {
      printf("   Max tries allowed: %d\n", try_data->max_tries);
      printf("   Successful attempts by number of tries required:\n");
      int ndx;
      for (ndx=1; ndx <= try_data->max_tries; ndx++) {
         printf("     %2d:  %3d\n", ndx, try_data->counters[ndx]);
      }
      printf("   Failed due to max tries exceeded: %3d\n", try_data->counters[try_data->max_tries+1]);
      printf("   Failed due to fatal error:        %3d\n", try_data->counters[0]);
      printf("   Total tries:                      %3d\n", get_total_tries(stats_rec));
   }

}
