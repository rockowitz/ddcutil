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

// from ddc_packet_io.h
// void ddc_set_max_write_only_exchange_tries(Retry_Op_Value ct);
Retry_Op_Value
     ddc_get_max_write_only_exchange_tries();
// void ddc_set_max_write_read_exchange_tries(Retry_Op_Value ct);
Retry_Op_Value
     ddc_get_max_write_read_exchange_tries();

void ddc_report_write_only_stats(int depth);
void ddc_report_write_read_stats(int depth);

// Statistics - from ddc_multi_part_io.c:
#ifdef OLD
void ddc_reset_multi_part_read_stats();
void ddc_reset_multi_part_write_stats();
#endif
void ddc_report_multi_part_read_stats(int depth);
void ddc_report_multi_part_write_stats(int depth);

void ddc_report_ddc_stats(int depth);

// Retry management
// void ddc_set_max_multi_part_read_tries(int ct);
Retry_Op_Value ddc_get_max_multi_part_read_tries();

// void ddc_set_max_multi_part_write_tries(int ct);
Retry_Op_Value  ddc_get_max_multi_part_write_tries();



void     try_data_init();
bool     try_data_lock();
void     try_data_unlock(bool this_function_owns_lock);
Retry_Op_Value
         try_data_get_maxtries2(Retry_Operation retry_type);
void     try_data_set_maxtries2(Retry_Operation retry_type, Retry_Op_Value new_maxtries);
void     try_data_reset2(       Retry_Operation retry_type);
void     try_data_reset2_all();
void     try_data_record_tries2(Retry_Operation retry_type, DDCA_Status rc, int tryct);
void     try_data_report2(      Retry_Operation retry_type, int depth);

#endif /* TRY_STATS_H_ */
