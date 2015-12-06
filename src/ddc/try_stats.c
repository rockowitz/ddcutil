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

#include "base/ddc_errno.h"
#include "base/parms.h"

#include "ddc/try_stats.h"

#define MAX_STAT_NAME_LENGTH  31


static char * TAG_VALUE = "STAT";

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
   // printf("(%s) try_data->counters[MAX_MAX_TRIES+1]=%d, MAX_MAX_TRIES=%d\n", __func__, try_data->counters[MAX_MAX_TRIES+1], MAX_MAX_TRIES);
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
   Try_Data * try_data = unopaque(stats_rec);
   assert(new_max_tries >= 1 && new_max_tries <= MAX_MAX_TRIES);
   try_data->max_tries = new_max_tries;
}

void try_data_reset(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   int ndx;
   for (ndx=0; ndx < MAX_MAX_TRIES+1; ndx++)
      try_data->counters[ndx] = 0;
}

static void record_successful_tries(void * stats_rec, int tryct){
   Try_Data * try_data = unopaque(stats_rec);
   assert(0 < tryct && tryct <= try_data->max_tries);
   if ( (tryct+1) == MAX_MAX_TRIES) {
      printf("(%s) tryct+1 == MAX_MAX_TRIES (%d)\n", __func__, MAX_MAX_TRIES);
   }
   try_data->counters[tryct+1] += 1;
}

static void record_failed_max_tries(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   try_data->counters[1] += 1;
}

static void record_failed_fatally(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   try_data->counters[0] += 1;
}


void try_data_record_tries(void * stats_rec, int rc, int tryct) {
   // printf("(%s) stats_rec=%p, rc=%d, tryct=%d\n", __func__, stats_rec, rc, tryct);
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


void try_data_report(void * stats_rec) {
   Try_Data * try_data = unopaque(stats_rec);
   printf("\nRetry statistics for %s\n", try_data->stat_name);
   if (try_data_get_total_attempts(stats_rec) == 0) {
      printf("   No tries attempted\n");
   }
   else {
      int total_successful_attempts = 0;
      printf("   Max tries allowed: %d\n", try_data->max_tries);
      printf("   Successful attempts by number of tries required:\n");
      int ndx;
      for (ndx=2; ndx <= try_data->max_tries+1; ndx++) {
         total_successful_attempts += try_data->counters[ndx];
         // printf("(%s) ndx=%d\n", __func__, ndx);
         printf("     %2d:  %3d\n", ndx-1, try_data->counters[ndx]);
      }
      printf("   Total successful attempts:        %3d\n", total_successful_attempts);
      printf("   Failed due to max tries exceeded: %3d\n", try_data->counters[1]);
      printf("   Failed due to fatal error:        %3d\n", try_data->counters[0]);
      printf("   Total attempts:                   %3d\n", try_data_get_total_attempts(stats_rec));
   }
}
