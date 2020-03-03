/** @file ddc_try_stats.h
 *
 *  Maintains statistics on DDC retries.
 *
 *  These statistics are global, not broken out by thread.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRY_STATS_H_
#define TRY_STATS_H_

#include "ddcutil_types.h"

#include "base/parms.h"


void init_ddc_try_data();

int  try_data_get_max_tries2(     DDCA_Retry_Type retry_type);
void try_data_set_max_tries2(     DDCA_Retry_Type retry_type, int new_maxtries);
void try_data_reset2(             DDCA_Retry_Type retry_type);
void try_data_record_tries2(      DDCA_Retry_Type retry_type, int rc, int tryct);
int  try_data_get_total_attempts2(DDCA_Retry_Type retry_type);
void try_data_report2(            DDCA_Retry_Type retry_type, int depth);

#endif /* TRY_STATS_H_ */
