/** @file base_init.c
 *  Initialize and release base services.
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/error_info.h"

#include "core.h"
#include "ddc_packets.h"
#include "displays.h"
#include "execution_stats.h"
#include "linux_errno.h"
#include "per_thread_data.h"
#include "sleep.h"

#include "base_init.h"


/** Master initialization function for files in subdirectory base
 */
void init_base_services() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);
   errinfo_init(psc_name, psc_desc);
   init_sleep_stats();
   init_execution_stats();
   init_status_code_mgt();
   // init_linux_errno();
   init_thread_data_module();
   init_displays();
   init_ddc_packets();
   if (debug)
      printf("(%s) Done\n", __func__);
}

void release_base_services() {
   release_thread_data_module();
}
