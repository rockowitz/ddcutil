/** \file per_thread_data.h
 *
 *  Maintains per-thread settings and statistics.
 *
 *  The dependencies between this file and thread_retry_data.c and thread_sleep.data
 *  are not unidirectional.  The functionality has been split into 3 files for clarity.
 */

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PER_THREAD_DATA_H_
#define PER_THREAD_DATA_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "base/parms.h"
#include "base/displays.h"

extern GHashTable *  per_thread_data_hash;
extern GMutex        per_thread_data_mutex;    // temp, replace by function calls

void init_thread_data_module();     // module initialization

extern int  ptd_lock_count;
extern int  ptd_unlock_count;
extern int  cross_thread_operation_blocked_count;

typedef
struct {
   // DDCA_Retry_Type  stat_id;    // nice as a consistency check, but has to be initialized to non-zero value
//   int          maxtries;
   uint16_t       counters[MAX_MAX_TRIES+2];
} Per_Thread_Try_Stats;


typedef struct {
   bool   initialized;
   bool   dynamic_sleep_enabled;
   pid_t  thread_id;
   // Display_Ref * dref;
   char * description;

   // Standard sleep adjustment settings
   bool   thread_sleep_data_defined;
   double sleep_multiplier_factor;         // initially set by user

   int    sleep_multiplier_ct    ;         // can be changed by retry logic
   int    highest_sleep_multiplier_value;  // high water mark
   int    sleep_multipler_changer_ct;      // number of function calls that adjusted multiplier ct

   // For Dynamic Sleep Adjustment
   int    current_ok_status_count;
   int    current_error_status_count;
   int    total_ok_status_count;
   int    total_error_status_count;
   int    total_other_status_ct;
   int    calls_since_last_check;
   int    total_adjustment_checks;
   int    adjustment_ct;
   int    non_adjustment_ct;
   int    max_adjustment_ct;
   double current_sleep_adjustment_factor;
   double thread_adjustment_increment;
   int    adjustment_check_interval;

   // Retry management
   bool thread_retry_data_defined;
   DDCA_Retry_Count_Type current_maxtries[4];
   DDCA_Retry_Count_Type highest_maxtries[4];
   DDCA_Retry_Count_Type lowest_maxtries[4];

   Per_Thread_Try_Stats  try_stats[4];
} Per_Thread_Data;

bool ptd_cross_thread_operation_start();
void ptd_cross_thread_operation_end();
void ptd_cross_thread_operation_block();

char * int_array_to_string(uint16_t * start, int ct);

Per_Thread_Data * ptd_get_per_thread_data();

void         ptd_set_thread_description(const char * description);
void         ptd_append_thread_description(const char * addl_description);
const char * ptd_get_thread_description_t();

// Apply a function to all Thread_Sleep_Data records
typedef void (*Ptd_Func)(Per_Thread_Data * data, void * arg);   // Template for function to apply
void ptd_apply_all(Ptd_Func func, void * arg);
void ptd_apply_all_sorted(Ptd_Func func, void * arg);

void dbgrpt_per_thread_data(Per_Thread_Data * data, int depth);
void ptd_list_threads(int depth);

void dbgrpt_per_thread_data_locks(int depth);
#endif /* PER_THREAD_DATA_H_ */
