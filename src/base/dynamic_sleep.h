// dynamic_sleep.h

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
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

void   dsa_enable(bool enabled);
bool   dsa_is_enabled();
void   dsa_set_sleep_multiplier_factor(double factor);
void   dsa_record_ddcrw_status_code(int rc);
void   dsa_reset_counts();
double dsa_get_sleep_adjustment();
void   dsa_report_stats(int depth);

#endif /* DYNAMIC_SLEEP_H_ */
