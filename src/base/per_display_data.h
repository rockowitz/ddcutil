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

#include "base/core.h"
#include "base/parms.h"
#include "base/displays.h"
#include "base/stats.h"

// use struct instead of #include "dsa2.h", etc. to avoid circular includes
struct DSA0_Data;
struct Results_Table;

extern GHashTable *  per_display_data_hash;
// extern GMutex     per_display_data_mutex;    // temp, replace by function calls

typedef enum {
   Default,
   Explicit,
   Reset
} User_Multiplier_Source;

const char * user_multiplier_source_name(User_Multiplier_Source source);

extern DDCA_Sleep_Multiplier default_user_sleep_multiplier;

typedef
struct {
    Retry_Operation  retry_op;    // nice as a consistency check, but has to be initialized to non-zero value
    uint16_t         counters[MAX_MAX_TRIES+2];
} Per_Display_Try_Stats;

typedef struct Per_Display_Data {
   DDCA_IO_Path           dpath;
   DDCA_Sleep_Multiplier  user_sleep_multiplier;           // set by user
   User_Multiplier_Source user_multiplier_source;
   struct Results_Table * dsa2_data;
   int                    total_sleep_time_millis;
   int                    cur_loop_null_msg_ct;
   Per_Display_Try_Stats  try_stats[4];
   DDCA_Sleep_Multiplier  initial_adjusted_sleep_multiplier;
   DDCA_Sleep_Multiplier  final_successful_adjusted_sleep_multiplier;
   DDCA_Sleep_Multiplier  most_recent_adjusted_sleep_multiplier;   // may have failed
   DDCA_Sleep_Multiplier  min_successful_sleep_multiplier;
   DDCA_Sleep_Multiplier  max_successful_sleep_multiplier;
   DDCA_Sleep_Multiplier  total_successful_sleep_multiplier;
   int                    successful_sleep_multiplier_ct;
   bool                   dsa2_enabled;
   bool                   dynamic_sleep_active;
   bool                   cur_loop_null_adjustment_occurred;
} Per_Display_Data;

// For new displays
void   pdd_set_default_sleep_multiplier_factor(
          DDCA_Sleep_Multiplier multiplier, User_Multiplier_Source source);
DDCA_Sleep_Multiplier
       pdd_get_default_sleep_multiplier_factor();

bool   pdd_cross_display_operation_start(const char * msg);
void   pdd_cross_display_operation_end(const char * msg);
void   pdd_cross_display_operation_block(const char * msg);

void   pdd_init_pdd(Per_Display_Data * pdd);
Per_Display_Data *
       pdd_get_per_display_data(DDCA_IO_Path, bool create_if_not_found);

// Apply a function to all Per_Display_Data records
typedef void (*Pdd_Func)(Per_Display_Data * data, void * arg);   // Template for function to apply
void   pdd_apply_all(Pdd_Func func, void * arg);
void   pdd_apply_all_sorted(Pdd_Func func, void * arg);

void   pdd_reset_all();

void   pdd_enable_dynamic_sleep_all(bool onoff);
bool   pdd_is_dynamic_sleep_enabled();

void   dbgrpt_per_display_data(Per_Display_Data * data, int depth);
void   dbgrpt_per_display_data_locks(int depth);

void   pdd_report_all_per_display_error_counts(int depth);
void   pdd_report_all_per_display_call_stats(int depth);

void   pdd_report_elapsed(Per_Display_Data * pdd, bool include_dsa_internal, int depth);
void   pdd_report_all_per_display_elapsed_stats(bool include_dsa_internal, int depth);

void   pdd_record_adjusted_sleep_multiplier(Per_Display_Data * pdd, bool successful);

bool   pdd_set_dynamic_sleep_active(Per_Display_Data * pdd, bool onoff);
bool   pdd_is_dynamic_sleep_active(Per_Display_Data * pdd);
void   pdd_reset_multiplier(Per_Display_Data * pdd, DDCA_Sleep_Multiplier multiplier);
DDCA_Sleep_Multiplier
       pdd_get_adjusted_sleep_multiplier(Per_Display_Data* pdd);
void   pdd_note_retryable_failure(Per_Display_Data * pdd, DDCA_Status ddcrc, int remaining_tries);
void   pdd_record_final(Per_Display_Data * pdd, DDCA_Status ddcrc, int retries);

void   pdd_reset_multiplier_by_dh(Display_Handle * dh, DDCA_Sleep_Multiplier multiplier);
DDCA_Sleep_Multiplier
       pdd_get_sleep_multiplier_by_dh(Display_Handle * dh);
void   pdd_note_retryable_failure_by_dh(Display_Handle * dh, DDCA_Status ddcrc, int remaining_tries);
void   pdd_record_final_by_dh(Display_Handle * dh, DDCA_Status ddcrc, int retries);

void   init_per_display_data();
void   terminate_per_display_data();

#endif /* PER_DISPLAY_DATA_H_ */
