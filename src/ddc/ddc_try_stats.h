/** @file ddc_try_stats.h
 *
 *  Maintains statistics on DDC retries.
 */

// Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRY_STATS_H_
#define TRY_STATS_H_

#define MAX_STAT_NAME_LENGTH  31

// Returns an opaque pointer to a Try_Data data structure
void * try_data_create(char * stat_name, int max_tries);

void try_data_reset(void * stats_rec);

void try_data_record_tries(void * stats_rec, int rc, int tryct);

int  try_data_get_total_attempts(void * stats_rec);

void try_data_report(void * stats_rec, int depth);

int  try_data_get_max_tries(void * stats_rec);

void try_data_set_max_tries(void* stats_rec,int new_max_tries);

#endif /* TRY_STATS_H_ */
