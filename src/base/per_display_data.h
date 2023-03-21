/** @file per_display_data.h
 *
 *  Maintains per-display settings and statistics.
 *
 *  The dependencies between this file and thread_retry_data.c and thread_sleep.data
 *  are not unidirectional.  The functionality has been split into 3 files for clarity.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PER_DISPLAY_DATA_H_
#define PER_DISPLAY_DATA_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "base/parms.h"
#include "base/stats.h"

extern GHashTable *  per_display_data_hash;
// extern GMutex        per_display_data_mutex;    // temp, replace by function calls

#ifdef PTD
void pdd_init_display_data_module();     // module initialization
void pdd_release_display_data_module();  // release all resources
#endif

extern int  pdd_lock_count;
extern int  pdd_unlock_count;
extern int  pdd_cross_thread_operation_blocked_count;

typedef
struct {
    Retry_Operation  retry_op;    // nice as a consistency check, but has to be initialized to non-zero value
    uint16_t         counters[MAX_MAX_TRIES+2];
} Per_Display_Try_Stats;


typedef struct {
   bool          initialized;
   DDCA_IO_Path  dpath;

   // Standard sleep adjustment settings
   bool   display_sleep_data_defined;
   double sleep_multiplier_factor;         // initially set by user
   int    sleep_multiplier_ct    ;         // can be changed by retry logic
   int    highest_sleep_multiplier_ct;     // high water mark
   int    sleep_multipler_changer_ct;      // number of function calls that adjusted multiplier ct

   // For Dynamic Sleep Adjustment
   bool   dynamic_sleep_enabled;
   int    cur_ok_status_count;
   int    cur_error_status_count;
   int    total_ok_status_count;
   int    total_error_status_count;
   int    total_other_status_ct;
   int    calls_since_last_check;
   int    adjustment_check_interval;
   int    total_adjustment_checks;
   int    total_adjustment_ct;
// int    total_non_adjustment_ct;
// int    total_max_adjustment_ct;
// int    spec_sleep_time_millis;

   double cur_sleep_adjustment_factor;
   int    total_sleep_time_millis;

   // Retry management
   bool              display_retry_data_defined;
// #ifdef UNUSED
   Retry_Op_Value    current_maxtries[4];
   Retry_Op_Value    highest_maxtries[4];
   Retry_Op_Value    lowest_maxtries[4];

   Per_Display_Try_Stats  try_stats[4];
// #endif
} Per_Display_Data;

void pdd_init(Per_Display_Data * pdd);

bool pdd_cross_display_operation_start(const char * msg);
void pdd_cross_display_operation_end(const char * msg);
void pdd_cross_display_operation_block(const char * msg);

Per_Display_Data * pdd_get_per_display_data(DDCA_IO_Path);

// Apply a function to all Display_Sleep_Data records
typedef void (*Dtd_Func)(Per_Display_Data * data, void * arg);   // Template for function to apply
void pdd_apply_all(Dtd_Func func, void * arg);
void pdd_apply_all_sorted(Dtd_Func func, void * arg);

void dbgrpt_per_display_data(Per_Display_Data * data, int depth);
void pdd_list_displays(int depth);

void dbgrpt_per_display_data_locks(int depth);

void report_all_display_status_counts(int depth);

void init_per_display_data();
#endif /* PER_DISPLAY_DATA_H_ */
