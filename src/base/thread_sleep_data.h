/** @file thread_sleep_data.h
  *
  * Maintains thread specific sleep data
  */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef THREAD_SLEEP_DATA_H_
#define THREAD_SLEEP_DATA_H_


#include "base/per_thread_data.h"


void init_thread_sleep_data(Per_Thread_Data * data);

Per_Thread_Data * tsd_get_thread_sleep_data();

// Experimental Dynamic Sleep Adjustment
void   tsd_enable_dsa_all(bool enable);
void   tsd_enable_dynamic_sleep(bool enabled);   // controls field display in reports

void   tsd_dsa_enable_globally(bool enabled);
void   tsd_dsa_enable(bool enabled);
bool   tsd_dsa_is_enabled();
void   tsd_set_dsa_enabled_default(bool enabled);
bool   tsd_get_dsa_enabled_default();

//
// Sleep time adjustments
//

// For new threads
void   tsd_set_default_sleep_multiplier_factor(double multiplier);
double tsd_get_default_sleep_multiplier_factor();

//  Per thread sleep-multiplier
double tsd_get_sleep_multiplier_factor();
void   tsd_set_sleep_multiplier_factor(double factor);

//  sleep_multiplier_ct is set by functions performing I2C retry
//  Per thread
int    tsd_get_sleep_multiplier_ct();
void   tsd_set_sleep_multiplier_ct(int multiplier_ct);
void   tsd_bump_sleep_multiplier_changer_ct();

// Reporting
void   report_thread_sleep_data(Per_Thread_Data * data, int depth);
void   report_all_thread_sleep_data(int depth);

#endif /* THREAD_SLEEP_DATA_H_ */
