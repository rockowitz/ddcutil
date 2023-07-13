/** @file ddc_services.h
 *
 * ddc layer initialization and configuration, statistics management
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SERVICES_H_
#define DDC_SERVICES_H_

#include <stdbool.h>
#include <stdio.h>

#include "public/ddcutil_types.h"

void init_ddc_services();
void terminate_ddc_services();
void ddc_reset_stats_main();
void ddc_report_stats_main(
      DDCA_Stats_Type  stats,
      bool             report_per_display,
      bool             include_dsa_stats,
      bool             stats_to_syslog_only,
      int              depth);

#endif /* DDC_SERVICES_H_ */
