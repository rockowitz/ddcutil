// retry_mgt.h

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_RETRY_MGT_H_
#define DDC_RETRY_MGT_H_

#include <stdint.h>

#include "public/ddcutil_types.h"

#define RETRY_TYPE_COUNT  3  // number of entries in DDCA_Retry_Type

extern uint16_t max_tries_default[RETRY_TYPE_COUNT];

const char * ddc_retry_type_name(DDCA_Retry_Type stat_id);
const char * ddc_retry_type_description(DDCA_Retry_Type retry_class);

void     ddc_set_default_single_max_tries(DDCA_Retry_Type type_id, uint16_t new_max_tries);
void     ddc_set_default_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]);
void     ddc_set_cur_thread_single_max_tries(DDCA_Retry_Type type_id, uint16_t new_max_tries);
void     ddc_set_cur_thread_all_max_tries(uint16_t new_max_tries[RETRY_TYPE_COUNT]);
uint16_t ddc_get_cur_thread_single_max_tries(DDCA_Retry_Type type_id);

#endif /* DDC_RETRY_MGT_H_ */
