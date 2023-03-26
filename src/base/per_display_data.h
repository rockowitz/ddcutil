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
#include "base/displays.h"
//#include "base/dsa0.h"
// #include "base/dsa1.h"
#include "base/stats.h"

struct DSA1_Data;
struct DSA0_Data;

extern GHashTable *  per_display_data_hash;
// extern GMutex        per_display_data_mutex;    // temp, replace by function calls

extern double default_sleep_multiplier_factor;
extern int    pdd_lock_count;
extern int    pdd_unlock_count;
extern int    pdd_cross_thread_operation_blocked_count;

typedef
struct {
    Retry_Operation  retry_op;    // nice as a consistency check, but has to be initialized to non-zero value
    uint16_t         counters[MAX_MAX_TRIES+2];
} Per_Display_Try_Stats;

typedef struct Per_Display_Data {
   DDCA_IO_Path           dpath;
   double                 user_sleep_multiplier;           // set by user
   double                 adjusted_sleep_multiplier;     //
   struct DSA0_Data *     dsa0_data;
   struct DSA1_Data *     dsa1_data;
   int                    total_sleep_time_millis;
   Per_Display_Try_Stats  try_stats[4];
   double                 initial_adjusted_sleep_multiplier;
   double                 final_successful_adjusted_sleep_multiplier;
   double                 most_recent_adjusted_sleep_multiplier;   // may have failed
} Per_Display_Data;

// For new displays
void   pdd_set_default_sleep_multiplier_factor(double multiplier);
double pdd_get_default_sleep_multiplier_factor();

//  Per display sleep-multiplier
double pdd_get_sleep_multiplier_factor(Per_Display_Data * pdd);
void   pdd_set_sleep_multiplier_factor(Per_Display_Data * pdd, double factor);

bool   pdd_cross_display_operation_start(const char * msg);
void   pdd_cross_display_operation_end(const char * msg);
void   pdd_cross_display_operation_block(const char * msg);

void   pdd_init_pdd(Per_Display_Data * pdd);
Per_Display_Data * pdd_get_per_display_data(DDCA_IO_Path, bool create_if_not_found);

// Apply a function to all Per_Display_Data records
typedef void (*Pdd_Func)(Per_Display_Data * data, void * arg);   // Template for function to apply
void   pdd_apply_all(Pdd_Func func, void * arg);
void   pdd_apply_all_sorted(Pdd_Func func, void * arg);

void   pdd_reset_all();

void   dbgrpt_per_display_data(Per_Display_Data * data, int depth);
void   dbgrpt_per_display_data_locks(int depth);

void   pdd_report_all_display_status_counts(int depth);

void   pdd_report_elapsed(Per_Display_Data * pdd, int depth);
void   pdd_report_all_elapsed(int depth);

void   pdd_record_adjusted_successful_sleep_multiplier_bounds(Per_Display_Data * pdd);

void   pdd_reset_multiplier(Per_Display_Data * pdd, float multiplier);
double pdd_get_adjusted_sleep_multiplier(Per_Display_Data* pdd);
void   pdd_note_retryable_failure(Per_Display_Data * pdd, int remaining_tries);
void   pdd_record_final(Per_Display_Data * pdd, DDCA_Status ddcrc, int retries);

void   pdd_reset_multiplier_by_dh(Display_Handle * dh, float multiplier);
float  pdd_get_sleep_multiplier_by_dh(Display_Handle * dh);
void   pdd_note_retryable_failure_by_dh(Display_Handle * dh, int remaining_tries);
void   pdd_record_final_by_dh(Display_Handle * dh, DDCA_Status ddcrc, int retries);

void init_per_display_data();
void terminate_per_display_data();

#endif /* PER_DISPLAY_DATA_H_ */
