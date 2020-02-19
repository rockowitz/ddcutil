/** @file thread_sleep_data.h
  *
  * Consolidates all thread specific sleep data
  */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef THREAD_SLEEP_DATA_H_
#define THREAD_SLEEP_DATA_H_

typedef struct {
   bool   initialized;
   pid_t  thread_id;

   // For Dynamic Sleep Adjustment
   int    ok_status_count;
   int    error_status_count;
   int    total_ok;
   int    total_error;
   int    other_status_ct;
   int    adjustment_ct;
   int    non_adjustment_ct;
   int    max_adjustment_ct;
   double current_sleep_adjustment_factor;

   double sleep_multiplier_factor;   // as set by user
   int    sleep_multiplier_ct;       // set by retry logic
   int    max_sleep_multiplier_ct;
   int    sleep_multipler_changed_ct;

} Thread_Sleep_Data;

void set_global_sleep_multiplier_factor(double factor);
double get_global_sleep_multiplier_factor();

Thread_Sleep_Data *  get_thread_sleep_data(bool create_if_necessary);


double tsd_get_sleep_multiplier_factor();
void tsd_set_sleep_multiplier_factor(double factor);

int  tsd_get_sleep_multiplier_ct();
void tsd_set_sleep_multiplier_ct(int multiplier_ct);
void tsd_bump_sleep_multiplier_changed_ct();

void report_thread_sleep_data(Thread_Sleep_Data * data, int depth);
void dbgrpt_thread_sleep_data(Thread_Sleep_Data * data, int depth);
void report_all_thread_sleep_data(int depth);

#endif /* THREAD_SLEEP_DATA_H_ */
