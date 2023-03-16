/** @file dsa2.h
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA2_H_
#define DSA2_H_

#include "public/ddcutil_types.h"

#include "base/displays.h"

extern bool dsa2_enabled;

void   dsa2_reset_multiplier(float multiplier);

void   dsa2_record_ddcrw_status_code(DDCA_IO_Path dpath, int retries, DDCA_Status ddcrc, bool retryable);
void   dsa2_record_ddcrw_status_code_by_dh(Display_Handle * dh, int retries, DDCA_Status ddcrc, bool retryable);
float  dsa2_get_sleep_multiplier(DDCA_IO_Path dpath);

bool         dsa2_restore_persistent_stats();
Status_Errno dsa2_save_persistent_stats();
Status_Errno dsa2_erase_persistent_stats();

void init_dsa2();

#define DSA2_RECORD_DDCRW_STATUS_CODE_BY_DH(_dh, _retries, _ddcrc, _retryable) \
   if (dsa2_enabled) dsa2_record_ddcrw_status_code_by_dh(_dh, _retries, _ddcrc, _retryable)

#endif /* DSA2_H_ */
