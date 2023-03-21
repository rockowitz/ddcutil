/** @file base_services.c
 *  Initialize and release base services.
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/error_info.h"

#include "core.h"
#include "ddc_packets.h"
#include "displays.h"
#include "dynamic_features.h"
#include "dynamic_sleep.h"
#include "dsa2.h"
#include "execution_stats.h"
#include "linux_errno.h"
#include "per_display_data.h"
#include "per_thread_data.h"
#include "sleep.h"
#include "display_sleep_data.h"
#include "tuned_sleep.h"

#include <base_services.h>

/** Master initialization function for files in subdirectory base
 */
void init_base_services() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);
   errinfo_init(psc_name, psc_desc);
   init_base_dynamic_features();
   init_ddc_packets();
   init_display_sleep_data();
   init_displays();
   init_dsa2();
   init_dynamic_sleep();
   init_execution_stats();
   // init_linux_errno();
   init_per_display_data();
   init_per_thread_data();
   init_sleep_stats();
   init_status_code_mgt();
   init_tuned_sleep();
   if (debug)
      printf("(%s) Done\n", __func__);
}


void release_base_services() {
   void terminate_dsa2();
}
