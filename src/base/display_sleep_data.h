/** @file display_sleep_data.h
  *
  * Maintains thread specific sleep data
  */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef DISPLAY_SLEEP_DATA_H_
#define DISPLAY_SLEEP_DATA_H_


#include "base/per_display_data.h"


void dsd_init_display_sleep_data(Per_Display_Data * data);

Per_Display_Data * dsd_get_display_sleep_data();

// Experimental Dynamic Sleep Adjustment
void   dsd_enable_dsa_all(bool enable);
void   dsd_enable_dynamic_sleep(bool enabled);   // controls field display in reports

void   dsd_dsa_enable_globally(bool enabled);
void   dsd_dsa_enable(bool enabled);
bool   dsd_dsa_is_enabled();
#ifdef UNUSED
void   dsd_set_dsa_enabled_default(bool enabled);
#endif
bool   dsd_get_dsa_enabled_default();

//
// Sleep time adjustments
//

// For new threads
void   dsd_set_default_sleep_multiplier_factor(double multiplier);
double dsd_get_default_sleep_multiplier_factor();

//  Per thread sleep-multiplier
double dsd_get_sleep_multiplier_factor();
void   dsd_set_sleep_multiplier_factor(double factor);

//  sleep_multiplier_ct is set by functions performing I2C retry
//  Per thread
int    dsd_get_sleep_multiplier_ct();
void   dsd_set_sleep_multiplier_ct(int multiplier_ct);
void   dsd_bump_sleep_multiplier_changer_ct();

// Reporting
void   report_display_sleep_data(Per_Display_Data * data, int depth);
void   report_all_display_sleep_data(int depth);

// Module Initialization
void   init_display_sleep_data();

#endif /* DISPLAY_SLEEP_DATA_H_ */
