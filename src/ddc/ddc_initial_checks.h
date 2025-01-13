// ddc_initial_checks.h

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_INITIAL_CHECKS_H_
#define DDC_INITIAL_CHECKS_H_

#include <stdbool.h>

#include "util/error_info.h"
#include "base/displays.h"

extern bool  skip_ddc_checks;
extern bool  monitor_state_tests;

Error_Info * ddc_initial_checks_by_dref(Display_Ref * dref, bool newly_added);
void         explore_monitor_state(Display_Handle* dh);

void init_ddc_initial_checks();

#endif /* DDC_INITIAL_CHECKS_H_ */
