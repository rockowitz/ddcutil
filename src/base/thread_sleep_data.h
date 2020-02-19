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
   pid_t  thread_id;

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
   int adjustment_check_interval;

   double sleep_multiplier_factor;    // as set by user
   int    sleep_multiplier_ct;        // set by retry logic
   int    max_sleep_multiplier_ct;    // high water mark
   int    sleep_multipler_changed_ct; //
} Thread_Sleep_Data;


// This is the --sleep-multiplier command line option
void   set_global_sleep_multiplier_factor(double factor);
double get_global_sleep_multiplier_factor();
void   set_sleep_multiplier_factor_all(double factor);

void   tsd_enable_dynamic_sleep(bool enabled);   // controls field display in reports

Thread_Sleep_Data *
       get_thread_sleep_data(bool create_if_necessary);

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

#endif /* THREAD_SLEEP_DATA_H_ */
