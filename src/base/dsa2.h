/** @file dsa2.h
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA2_H_
#define DSA2_H_

#include <stdbool.h>

#include "public/ddcutil_types.h"

#include "base/displays.h"

extern const bool DSA2_Enabled_Default;
extern bool       dsa2_enabled;

struct Results_Table *
             dsa2_get_results_table_by_busno(int busno, bool create_if_not_found);
bool         dsa2_is_from_cache(struct Results_Table * dpath);
void         dsa2_reset_multiplier(float multiplier);
float        dsa2_get_adjusted_sleep_multiplier(struct Results_Table * rtable);
void         dsa2_note_retryable_failure(struct Results_Table * rtable, int remaining_tries);
void         dsa2_record_final(struct Results_Table * rtable, DDCA_Status ddcrc, int retries);

#ifdef UNUSED
bool         dsa2_is_from_cache_by_dpath(DDCA_IO_Path dpath);
float        dsa2_get_adjusted_sleep_multiplier_by_dpath(DDCA_IO_Path dpath);
void         dsa2_note_retryable_failure_by_dpath(DDCA_IO_Path dpath, int remaining_tries);
void         dsa2_record_final_by_dpath(DDCA_IO_Path dpath, DDCA_Status ddcrc, int retries);
#endif

Status_Errno dsa2_save_persistent_stats();
Status_Errno dsa2_erase_persistent_stats();
bool         dsa2_restore_persistent_stats();
void         dsa2_report_all(int depth);
void         dsa2_reset(struct Results_Table * rtable);
void         dsa2_reset_by_dpath(DDCA_IO_Path dpath);

void         init_dsa2();
void         terminate_dsa2();  // release all resources
#endif /* DSA2_H_ */
