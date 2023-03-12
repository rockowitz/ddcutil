/** @file dsa2.h
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA2_H_
#define DSA2_H_

#include "public/ddcutil_types.h"

#include "base/displays.h"

extern bool dsa2_enabled;

void dsa2_reset_multiplier(float multiplier);

void   dsa2_record_ddcrw_status_code(DDCA_IO_Path dpath, int try_ct, DDCA_Status ddcrc, bool retryable);
void   dsa2_record_ddcrw_status_code_by_dh(Display_Handle * dh, int try_ct, DDCA_Status ddcrc, bool retryable);
float  dsa2_get_sleep_multiplier(DDCA_IO_Path dpath);

bool dsa2_restore_persistent_stats();
int dsa2_save_persistent_stats();

void init_dsa2();

#endif /* DSA2_H_ */
