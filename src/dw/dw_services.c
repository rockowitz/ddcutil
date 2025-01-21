/** @file dw_services.c
 *
 * display watch layer initialization and configuration
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "dw/dw_status_events.h"
#include "dw/dw_dref.h"
#include "dw/dw_xevent.h"
#include "dw/dw_udev.h"
#include "dw/dw_poll.h"
#include "dw/dw_main.h"
#include "dw/dw_common.h"

#include "dw_services.h"


/** Initialize files in dw directory */
void init_dw_services() {
   bool debug = false;
   DBGMSF(debug, "Starting");

   init_dw_dref();
   init_dw_xevent();
   init_dw_udev();
   init_dw_poll();
   init_dw_common();
   init_dw_main();

   DBGMSF(debug, "Done");
}


/** Termination for files in dw directory */
void terminate_dw_services() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "");

   DBGTRC_DONE(debug, DDCA_TRC_CONN, "");
}
