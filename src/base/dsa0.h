/** @file dsa0.h
  *
  * Maintains thread specific sleep data
  */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA0_H_
#define DSA0_H_

#include <stdbool.h>

#include "public/ddcutil_types.h"


typedef  struct Per_Display_Data Per_Display_Data;

extern bool dsa0_enabled;

void    dsa0_enable(bool enabled);
bool    dsa0_is_enabledt();


typedef struct DSA0_Data {
   int    busno;
   int    sleep_multiplier_ct    ;         // can be changed by retry logic
   int    highest_sleep_multiplier_ct;     // high water mark
   int    sleep_multiplier_changer_ct;      // number of function calls that adjusted multiplier ct
   double adjusted_sleep_multiplier;     //
} DSA0_Data;

DSA0_Data * new_dsa0_data();
DSA0_Data * dsa0_get_dsa0_data(int busno);

void    dsa0_init_dsa0_data(DSA0_Data * data);
void    dsa0_reset(DSA0_Data * data);

double  dsa0_get_adjusted_sleep_multiplier(DSA0_Data * data);
void    dsa0_note_retryable_failure(DSA0_Data * data, int remaining_tries);
void    dsa0_record_final_by_pdd(Per_Display_Data * pdd, DDCA_Status ddcrc, int retries);

//  sleep_multiplier_ct is set by functions performing I2C retry
//  Per thread
int    dsa0_get_sleep_multiplier_ct(DSA0_Data * dsa0);
void   dsa0_set_sleep_multiplier_ct(DSA0_Data * dsa0, int multiplier_ct);
void   dsa0_bump_sleep_multiplier_changer_ct(DSA0_Data * dsa0);

// Reporting
void   report_dsa0_data(DSA0_Data * data, int depth);
void   report_all_dsa0_data(int depth);


// Apply a function to all DSA0_Data records
typedef void (*Dsa0_Func)(DSA0_Data * data, void * arg);   // Template for function to apply
void dsa0_apply_all(Dsa0_Func func, void * arg);
void dsa0_apply_all_sorted(Dsa0_Func func, void * arg);


// Module Initialization
void   init_dsa0();

#endif /* DSA0_H_ */
