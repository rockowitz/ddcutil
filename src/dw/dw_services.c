/** @file dw_services.c
 *
 * display watch layer initialization and configuration
 */

// Copyright (C) 2024-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "base/core.h"
#include "base/rtti.h"

#include "dw_common.h"
#include "dw_dref.h"
#include "dw_main.h"
#include "dw_poll.h"
#include "dw_recheck.h"
#include "dw_status_events.h"
#include "dw_udev2.h"
#ifdef USE_X11
#include "dw_xevent.h"
#endif

#include "dw_services.h"


/** Initialize files in dw directory */
void init_dw_services() {
   bool debug = false;
   DBGMSF(debug, "Starting");

   init_dw_common();
   init_dw_dref();
   init_dw_main();
   init_dw_poll();
   init_dw_recheck();
   init_dw_status_events();
   init_dw_udev2();
#ifdef USE_X11
   init_dw_xevent();
#endif

   RTTI_ADD_FUNC(terminate_dw_services);

   DBGMSF(debug, "Done");
}


/** Termination for files in dw directory */
void terminate_dw_services() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "");

   DBGTRC_DONE(debug, DDCA_TRC_CONN, "");
}
