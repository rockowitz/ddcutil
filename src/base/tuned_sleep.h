/** @file tuned_sleep.h
 */

// Copyright (C) 2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TUNED_SLEEP_H_
#define TUNED_SLEEP_H_

#include <stdbool.h>

#include "base/sleep.h"
#include "base/execution_stats.h"


void   set_sleep_multiplier_ct(/* Sleep_Event_Type event_types,*/ int multiplier);
int    get_sleep_multiplier_ct();

void   set_sleep_multiplier_factor(/* Sleep_Event_Type event_types,*/ double multiplier);
double get_sleep_multiplier_factor();


// Functions for sleeping.  The actual sleep time is determined
// by the strategy in place given the situation in which sleep is invoked.

// Convenience methods that call call_tuned_sleep():
void tuned_sleep_i2c(Sleep_Event_Type event_type);   // DDCA_IO_I2C
#ifdef DEPRECATED
void call_tuned_sleep_adl(Sleep_Event_Type event_type);   // DDCA_IO_ADL
#endif
void tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);
// The workhorse:
void tuned_sleep(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type);
void tuned_sleep_with_trace(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type, char * loc, char * msg);

#endif /* TUNED_SLEEP_H_ */
