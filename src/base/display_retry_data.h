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


// Maintain max_tries
void drd_set_default_max_tries(
      Retry_Operation     type_id,
      uint16_t            new_maxtries);

// Try Stats

void drd_record_display_tries(
      Per_Display_Data *  pdd,
      Retry_Operation     type_id,
      int                 rc,
      int                 tryct);

void report_display_try_typed_data_by_data(
      Retry_Operation     try_type_id,
      bool                for_all_displays_total,
      Per_Display_Data *  data,
      int                 depth);

void report_display_all_types_data_by_data(
      bool                for_all_displays,   // controls message
      Per_Display_Data *  data,
      int                 depth);

void drd_report_all_display_retry_data(int depth);

#endif /* DISPLAY_RETRY_DATA_H_ */
