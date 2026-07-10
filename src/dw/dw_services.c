/** @file dw_services.c
 *
 * display watch layer initialization and configuration
 */

// Copyright (C) 2024-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "util/debug_util.h"

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

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_CONN;


/** Initialize files in dw directory */
void init_dw_services() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

#ifdef USE_X11
   // Must precede any other Xlib call made by this process, including ones
   // made well before the display watch thread starts (e.g. get_x11_dpms_info()
   // during initial display checks). Without it, Xlib's per-Display data
   // structures are not thread-safe, which matters once Watch_Mode_Xevent's
   // evdata->dpy is later shared between the watch thread and whichever
   // thread stops it. init_dw_services() is called at the start of process
   // initialization (library constructor and app_ddcutil main()), before any
   // display detection, so this is the earliest choke point available.
   XInitThreads();
#endif

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

   RTTI_ADD_FUNC(init_dw_services);
   RTTI_ADD_FUNC(terminate_dw_services);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Termination for files in dw directory */
void terminate_dw_services() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   terminate_dw_recheck();

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}
