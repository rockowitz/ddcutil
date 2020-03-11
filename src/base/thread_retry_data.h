// thread_retry_data.h

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef THREAD_RETRY_DATA_H_
#define THREAD_RETRY_DATA_H_

#include "public/ddcutil_types.h"
#include "base/per_thread_data.h"


typedef struct {
   DDCA_Retry_Type retry_type;
   uint16_t        max_highest_maxtries;
   uint16_t        min_lowest_maxtries;
} Global_Maxtries_Accumulator;


//
// Retry management
//

// These functions probably belong elsewhere
const char * retry_type_name(DDCA_Retry_Type stat_id);
const char * retry_type_description(DDCA_Retry_Type retry_class);

void init_thread_retry_data(Per_Thread_Data * data);

// These functions manage retry counts on a per-thread basis

void     trd_set_default_max_tries(DDCA_Retry_Type type_id, uint16_t new_maxtries);
void     trd_set_initial_thread_max_tries( DDCA_Retry_Type type_id, uint16_t new_maxtries);
void     trd_set_thread_max_tries( DDCA_Retry_Type type_id, uint16_t new_maxtries);
uint16_t trd_get_thread_max_tries( DDCA_Retry_Type type_id);

#ifdef FUTURE
void     ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]);
void     ddc_set_thread_all_max_tries( uint16_t new_max_tries[RETRY_TYPE_COUNT]);
#endif

Global_Maxtries_Accumulator trd_get_all_threads_maxtries_range(DDCA_Retry_Type typeid);

void report_all_thread_maxtries_data(int depth);

// Try Stats
void trd_reset_cur_thread_tries();
void trd_reset_all_threads_tries();
// void trd_record_cur_thread_successful_tries(DDCA_Retry_Type type_id, int tryct);
// void trd_record_cur_thread_failed_max_tries(DDCA_Retry_Type type_id);
// void trd_record_cur_thread_failed_fatally(DDCA_Retry_Type type_id);
void trd_record_cur_thread_tries(DDCA_Retry_Type type_id, int rc, int tryct);
int get_thread_total_tries_for_all_types_by_data(Per_Thread_Data  * data);
void report_thread_try_typed_data_by_data(
      DDCA_Retry_Type     try_type_id,
      bool                for_all_threads_total,
      Per_Thread_Data *   data,
      int                 depth);
void report_thread_all_types_data_by_data(
      bool                for_all_threads,   // controls message
      Per_Thread_Data   * data,
      int                 depth);

#endif /* THREAD_RETRY_DATA_H_ */

