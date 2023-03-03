/** @file dynamic_sleep.h
 *
 *  Experimental dynamic sleep adjustment
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYNAMIC_SLEEP_H_
#define DYNAMIC_SLEEP_H_

/** \cond */
#include <inttypes.h>
#include <stdbool.h>
/** \endcond */

#include "util/timestamp.h"

#include "base/displays.h"
#include "base/status_code_mgt.h"

void   dsa_record_ddcrw_status_code(Display_Handle * dh, int rc);
double dsa_update_adjustment_factor(Display_Handle * dh, int spec_sleep_time_millis);
int    dsa_get_sleep_time(Display_Handle * dh, int spec_sleep_time_millis);
void   init_dynamic_sleep();

#endif /* DYNAMIC_SLEEP_H_ */
