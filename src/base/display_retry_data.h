/** \file thread_retry_data.h */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DISPLAY_RETRY_DATA_H_
#define DISPLAY_RETRY_DATA_H_

#include "public/ddcutil_types.h"
#include "base/stats.h"

#include "base/per_display_data.h"



typedef struct {
   Retry_Operation retry_type;
   uint16_t        max_highest_maxtries;
   uint16_t        min_lowest_maxtries;
} Global_Maxtries_Accumulator;


void drd_init(Per_Display_Data * data);

// These functions manage retry counts on a per-display basis

void     drd_set_default_max_tries(Retry_Operation type_id, uint16_t new_maxtries);
void     drd_set_initial_display_max_tries( Retry_Operation type_id, uint16_t new_maxtries);
void     drd_set_display_max_tries( Retry_Operation type_id, uint16_t new_maxtries);
uint16_t drd_get_display_max_tries( Retry_Operation type_id);
void     drd_set_all_maxtries(Retry_Operation rcls, uint16_t maxtries);

#ifdef FUTURE
void     ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]);
void     ddc_set_display_all_max_tries( uint16_t new_max_tries[RETRY_TYPE_COUNT]);
#endif

Global_Maxtries_Accumulator drd_get_all_displays_maxtries_range(Retry_Operation typeid);

void report_all_display_maxtries_data(int depth);

// Try Stats

void drd_record_cur_display_tries(Retry_Operation type_id, int rc, int tryct);
#ifdef UNUSED
void drd_reset_cur_display_tries();
void drd_reset_all_displays_tries();
void drd_record_cur_display_successful_tries(Retry_Operation type_id, int tryct);
void drd_record_cur_display_failed_max_tries(Retry_Operation type_id);
void drd_record_cur_display_failed_fatally(Retry_Operation type_id);
int get_display_total_tries_for_all_types_by_data(Per_Display_Data  * data);
#endif
void report_display_try_typed_data_by_data(
      Retry_Operation     try_type_id,
      bool                for_all_displays_total,
      Per_Display_Data *   data,
      int                 depth);
void report_display_all_types_data_by_data(
      bool                for_all_displays,   // controls message
      Per_Display_Data   * data,
      int                 depth);

#endif /* DISPLAY_RETRY_DATA_H_ */

