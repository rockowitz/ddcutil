/** @file ddc_try_stats.h
 *
 *  Maintains statistics on DDC retries and also maxtries settings.
 *
 *  These statistics are global, not broken out by thread.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRY_STATS_H_
#define TRY_STATS_H_

#include <stdbool.h>

#include "ddcutil_types.h"

#include "base/core.h"
#include "base/parms.h"

void     try_data_init_retry_type(Retry_Operation retry_type, Retry_Op_Value maxtries);
void     try_data_init();
bool     try_data_lock();
void     try_data_unlock(bool this_function_owns_lock);
Retry_Op_Value
         try_data_get_maxtries2(Retry_Operation retry_type);
void     try_data_set_maxtries2(Retry_Operation retry_type, Retry_Op_Value new_maxtries);
void     try_data_reset2_all();
void     try_data_record_tries2(Retry_Operation retry_type, DDCA_Status rc, int tryct);

void     ddc_report_max_tries(int depth);
void     ddc_report_ddc_stats(int depth);

#endif /* TRY_STATS_H_ */
