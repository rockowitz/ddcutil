/** @file ddc_services.h
 *
 * ddc layer initialization and configuration, statistics management
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SERVICES_H_
#define DDC_SERVICES_H_

#include <stdio.h>

void init_ddc_services();

// void ddc_reset_ddc_stats();
void ddc_reset_stats_main();

void ddc_report_stats_main(DDCA_Stats_Type stats, bool report_per_thread, int depth);
void ddc_report_max_tries(int depth);

#endif /* DDC_SERVICES_H_ */
