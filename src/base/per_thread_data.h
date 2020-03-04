// per_thread_data.h

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later



#ifndef PER_THREAD_DATA_H_
#define PER_THREAD_DATA_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "base/displays.h"

extern GHashTable *  per_thread_data_hash;
extern GMutex        per_thread_data_mutex;    // temp, replace by function calls

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
   uint16_t current_maxtries[4];
   uint16_t highest_maxtries[4];
   uint16_t lowest_maxtries[4];

} Per_Thread_Data;


void ptd_lock_all_thread_data();
void ptd_unlock_all_thread_data();

Per_Thread_Data * ptd_get_per_thread_data();

// void ptd_register_thread_dref(Display_Ref * dref);
void ptd_set_thread_description(const char * description);

// Apply function to all Thread_Sleep_Data records
// Template for function to apply
typedef void (*Ptd_Func)(Per_Thread_Data * data, void * arg);

void ptd_apply_all(Ptd_Func func, void * arg);
void ptd_apply_all_sorted(Ptd_Func func, void * arg);

void ptd_register_display_ref(Display_Ref * dref);

void   dbgrpt_per_thread_data(Per_Thread_Data * data, int depth);


#endif /* PER_THREAD_DATA_H_ */
