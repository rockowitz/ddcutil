/** @file dsa2.h Dynamic sleep algorithm 2
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DSA2_H_
#define DSA2_H_

#include <stdbool.h>

#include "public/ddcutil_types.h"

#include "util/error_info.h"
#include "base/core.h"
#include "base/status_code_mgt.h"

extern int   dsa2_step_floor;

void             dsa2_enable(bool yesno);
bool             dsa2_is_enabled();

bool             dsa2_set_greatest_tries_upper_bound(int tries);
bool             dsa2_set_average_tries_upper_bound(DDCA_Sleep_Multiplier avg_tries);

int              dsa2_multiplier_to_step(DDCA_Sleep_Multiplier multiplier);
DDCA_Sleep_Multiplier
                 dsa2_step_to_multiplier(int step);
DDCA_Sleep_Multiplier
                 dsa2_get_minimum_multiplier();
struct Results_Table *
                 dsa2_get_results_table_by_busno(int busno, bool create_if_not_found);
bool             dsa2_is_from_cache(struct Results_Table * dpath);
void             dsa2_reset_multiplier(DDCA_Sleep_Multiplier multiplier);
void             dsa2_reset_results_table(int busno, DDCA_Sleep_Multiplier sleep_multiplier);
DDCA_Sleep_Multiplier
                 dsa2_get_adjusted_sleep_mult(struct Results_Table * rtable);
void             dsa2_note_retryable_failure(
                     struct Results_Table * rtable,
                     DDCA_Status            ddcrc,
                     int                    remaining_tries);
void             dsa2_record_final(
                     struct Results_Table * rtable,
                     DDCA_Status            ddcrc,
                     int                    retries,
                     bool                   null_adjustment_occurred);
char *           dsa2_stats_cache_file_name();
Status_Errno     dsa2_save_persistent_stats();
Status_Errno     dsa2_erase_persistent_stats();
Error_Info *     dsa2_restore_persistent_stats();
void             dsa2_report_internal(struct Results_Table * rtable, int depth);
void             dsa2_report_internal_all(int depth);

void             init_dsa2();
void             terminate_dsa2();  // release all resources

#endif /* DSA2_H_ */
