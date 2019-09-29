/** @file tuned_sleep.h
 */

// Copyright (C) 2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef TUNED_SLEEP_H_
#define TUNED_SLEEP_H_

#include <stdbool.h>

#include "base/sleep.h"
#include "base/execution_stats.h"


#ifdef SLEEP_STRATEGY
// Sleep Strategy

bool   set_sleep_strategy(int strategy);
int    get_sleep_strategy();
char * sleep_strategy_desc(int sleep_strategy);
#endif


void   set_sleep_multiplier_ct(/* Sleep_Event_Type event_types,*/ int multiplier);
int    get_sleep_multiplier_ct();

void   set_sleep_multiplier_factor(/* Sleep_Event_Type event_types,*/ double multiplier);
double get_sleep_multiplier_factor();


// Functions for sleeping.  The actual sleep time is determined
// by the strategy in place given the situation in which sleep is invoked.

// Convenience methods that call call_tuned_sleep():
void call_tuned_sleep_i2c(Sleep_Event_Type event_type);   // DDC_IO_DEVI2C
void call_tuned_sleep_adl(Sleep_Event_Type event_type);   // DDC_IO_ADL
void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);
// The workhorse:
void call_tuned_sleep(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type);
void call_dynamic_tuned_sleep( DDCA_IO_Mode io_mode,Sleep_Event_Type event_type, int occno);
void call_dynamic_tuned_sleep_i2c(Sleep_Event_Type event_type, int occno);


#endif /* TUNED_SLEEP_H_ */
