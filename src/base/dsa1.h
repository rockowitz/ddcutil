/** @file dsa1.h
 *
 *  Experimental dynamic sleep adjustment
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA1_H_
#define DSA1_H_

/** \cond */
#include <inttypes.h>
#include <stdbool.h>
/** \endcond */

#include "base/displays.h"
// #include "base/per_display_data.h"

typedef  struct Per_Display_Data Per_Display_Data;

extern const bool DSA1_Enabled_Default;
extern bool       dsa1_enabled;

typedef struct DSA1_Data {
   int    busno;
   double adjusted_sleep_multiplier;     //
   int    cur_ok_status_count;
   int    cur_error_status_count;
   int    total_ok_status_count;
   int    total_error_status_count;
   int    total_other_status_ct;
   int    calls_since_last_check;
   int    adjustment_check_interval;
   int    total_adjustment_checks;
   int    total_adjustment_ct;
// int    total_non_adjustment_ct;
// int    total_max_adjustment_ct;
// int    spec_sleep_time_millis;
} DSA1_Data;

DSA1_Data * new_dsa1_data(Per_Display_Data * pdd);
void   dsa1_reset_data(DSA1_Data * data);
void   dsa1_record_ddcrw_status_code(Display_Handle * dh, int rc);

void   dsa1_reset(DSA1_Data * data);
double dsa1_get_adjusted_sleep_multiplier(DSA1_Data * data);
void   dsa1_note_retryable_failure_by_pdd(Per_Display_Data * data, int remaining_tries);
void   dsa1_record_final_by_pdd(Per_Display_Data * pdd, DDCA_Status ddcrc, int retries);


void   dsa1_update_adjustment_factor(Display_Handle * dh, int spec_sleep_time_millis);
int    dsa1_get_sleep_time(Display_Handle * dh, int spec_sleep_time_millis);
double dsa1_get_sleep_multiplier(DDCA_IO_Path dpath);
void   dsa1_report(DSA1_Data * data, int depth);
void   init_dsa1();

#endif /* DSA1_H_ */
