/** @file app_services.c
 *  Initialize files in directory app_ddcutil
 */

// Copyright (C) 2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "app_capabilities.h"
#include "app_dumpload.h"
#include "app_dynamic_features.h"
#include "app_getvcp.h"
#ifdef ENABLE_ENVCMDS
#include "app_interrogate.h"
#endif
#include "app_probe.h"
#include "app_setvcp.h"
#include "app_watch.h"
#include "app_vcpinfo.h"

void init_app_services() {
#ifdef ENABLE_ENVCMDS
   init_app_interrogate();
#endif
   init_app_capabilities();
   init_app_dynamic_features();
   init_app_dumpload();
   init_app_probe();
   init_app_setvcp();
   init_app_watch();
}
