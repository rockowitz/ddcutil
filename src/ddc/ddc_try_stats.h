/** @file ddc_try_stats.h
 *
 *  Maintains statistics on DDC retries.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRY_STATS_H_
#define TRY_STATS_H_

#include "ddcutil_types.h"

#include "base/parms.h"

#define MAX_STAT_NAME_LENGTH  31

#define TRY_DATA_TAG "STAT"
typedef
struct {
   char   tag[4];
   DDCA_Retry_Type retry_type;
   char   stat_name[MAX_STAT_NAME_LENGTH+1];
   int    max_tries;
   int    counters[MAX_MAX_TRIES+2];
} Try_Data;


Try_Data * try_data_create(DDCA_Retry_Type retry_type, char * stat_name, int max_tries);

void try_data_reset(Try_Data * stats_rec);

void try_data_record_tries(Try_Data * stats_rec, int rc, int tryct);

int  try_data_get_total_attempts(Try_Data * stats_rec);

void try_data_report(Try_Data * stats_rec, int depth);

int  try_data_get_max_tries(Try_Data * stats_rec);

void try_data_set_max_tries(Try_Data * stats_rec,int new_max_tries);

#endif /* TRY_STATS_H_ */
