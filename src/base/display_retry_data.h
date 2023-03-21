/** @file display_retry_data.h
 *
 * Maintain retry counts on a per-display basis.
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DISPLAY_RETRY_DATA_H_
#define DISPLAY_RETRY_DATA_H_

#include "public/ddcutil_types.h"

#include "base/per_display_data.h"


void     drd_init_display_data(Per_Display_Data * data);

// Maintain max_tries data
void     drd_set_default_max_tries(                            Retry_Operation type_id, uint16_t new_maxtries);
void     drd_set_display_max_tries(        DDCA_IO_Path dpath, Retry_Operation type_id, uint16_t new_maxtries);
uint16_t drd_get_display_max_tries(        DDCA_IO_Path dpath, Retry_Operation type_id);
void     drd_set_all_maxtries(                                 Retry_Operation type_id, uint16_t maxtries);

#ifdef FUTURE
void     ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]);
void     ddc_set_display_all_max_tries( uint16_t new_max_tries[RETRY_TYPE_COUNT]);
#endif

Global_Maxtries_Accumulator
         drd_get_all_displays_maxtries_range(Retry_Operation typeid);
void     drd_report_all_display_maxtries_data(int depth);

// Try Stats

#ifdef UNUSED
void drd_reset_display_tries( Per_Display_Data * pdd);
void drd_reset_all_displays_tries();
#endif

void drd_record_display_tries(Per_Display_Data * pdd, Retry_Operation type_id, int rc, int tryct);


#ifdef UNUSED
int get_display_total_tries_for_all_types_by_data(Per_Display_Data  * data);
#endif

void report_display_try_typed_data_by_data(
      Retry_Operation     try_type_id,
      bool                for_all_displays_total,
      Per_Display_Data *  data,
      int                 depth);
void report_display_all_types_data_by_data(
      bool                for_all_displays,   // controls message
      Per_Display_Data *  data,
      int                 depth);

#endif /* DISPLAY_RETRY_DATA_H_ */

