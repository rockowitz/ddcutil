/** @file thread_sleep_data.h
  *
  * Maintains thread specific sleep data
  */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef THREAD_SLEEP_DATA_H_
#define THREAD_SLEEP_DATA_H_

typedef struct {
   bool   initialized;
   bool   dynamic_sleep_enabled;
   pid_t  thread_id;

   // Standard sleep adjustment settings
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
   uint16_t current_maxtries[4];
   uint16_t highest_maxtries[4];
   uint16_t lowest_maxtries[4];

} Thread_Sleep_Data;


typedef struct {
   DDCA_Retry_Type retry_type;
   uint16_t        max_highest_maxtries;
   uint16_t        min_lowest_maxtries;
} Global_Maxtries_Accumulator;


void tsd_lock_all_thread_data();
void tsd_unlock_all_thread_data();

// Sleep time adjustments
void   tsd_set_default_sleep_multiplier_factor(double multiplier);
double tsd_get_default_sleep_multiplier_factor();

void   tsd_enable_dsa_all(bool enable);
void   tsd_enable_dynamic_sleep(bool enabled);   // controls field display in reports

void   tsd_dsa_enable_globally(bool enabled);
void   tsd_dsa_enable(bool enabled);
bool   tsd_dsa_is_enabled();
void   tsd_set_dsa_enabled_default(bool enabled);

Thread_Sleep_Data * tsd_get_thread_sleep_data();

//  Per thread sleep-multiplier
double tsd_get_sleep_multiplier_factor();
void   tsd_set_sleep_multiplier_factor(double factor);

// sleep_multiplier_ct is set by functions performing I2C retry
int    tsd_get_sleep_multiplier_ct();
void   tsd_set_sleep_multiplier_ct(int multiplier_ct);
void   tsd_bump_sleep_multiplier_changer_ct();

void   dbgrpt_thread_sleep_data(Thread_Sleep_Data * data, int depth);
void   report_thread_sleep_data(Thread_Sleep_Data * data, int depth);
void   report_all_thread_sleep_data(int depth);

// arg can point to a value or a struct
typedef void (*Tsd_Func)(Thread_Sleep_Data * data, void * arg);

void tsd_apply_all(Tsd_Func func, void * arg);


// Retry management

// #define RETRY_TYPE_COUNT  4  // number of entries in DDCA_Retry_Type

const char * ddc_retry_type_name(DDCA_Retry_Type stat_id);
const char * ddc_retry_type_description(DDCA_Retry_Type retry_class);

void     ddc_set_default_max_tries(DDCA_Retry_Type type_id, uint16_t new_max_tries);
void     ddc_set_initial_thread_max_tries( DDCA_Retry_Type type_id, uint16_t new_max_tries);
void     ddc_set_thread_max_tries( DDCA_Retry_Type type_id, uint16_t new_max_tries);
uint16_t ddc_get_thread_max_tries( DDCA_Retry_Type type_id);

#ifdef FUTURE
void     ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]);
void     ddc_set_thread_all_max_tries( uint16_t new_max_tries[RETRY_TYPE_COUNT]);
#endif

Global_Maxtries_Accumulator tsd_get_all_threads_maxtries_range(DDCA_Retry_Type typeid);

#endif /* THREAD_SLEEP_DATA_H_ */
