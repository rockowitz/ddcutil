/** @file dsa2.h
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA2_H_
#define DSA2_H_

#include "public/ddcutil_types.h"

#include "base/displays.h"

extern const bool DSA2_Enabled_Default;
extern bool       dsa2_enabled;

void         dsa2_reset_multiplier(float multiplier);
float        dsa2_get_sleep_multiplier(DDCA_IO_Path dpath);
void         dsa2_note_retryable_failure(DDCA_IO_Path dpath, int remaining_tries);
void         dsa2_record_final(DDCA_IO_Path dpath, DDCA_Status ddcrc, int retries);
Status_Errno dsa2_save_persistent_stats();
Status_Errno dsa2_erase_persistent_stats();
bool         dsa2_restore_persistent_stats();

void         init_dsa2();

#endif /* DSA2_H_ */
