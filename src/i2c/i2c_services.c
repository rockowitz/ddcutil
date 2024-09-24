/** @file i2c_services.c  */

// Copyright (C) 2022-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "i2c_bus_core.h"
#include "i2c_dpms.h"
#include "i2c_edid.h"
#include "i2c_execute.h"
#include "i2c_strategy_dispatcher.h"
#include "i2c_sys_drm_connector.h"
#include "i2c_sysfs_base.h"
#include "i2c_sysfs_conflicting_drivers.h"
#include "i2c_sysfs_i2c_info.h"
#include "i2c_sysfs_i2c_sys_info.h"

/** Master initializer for directory i2c */
void init_i2c_services() {
   init_i2c_bus_core();
   init_i2c_dpms();
   init_i2c_edid();
   init_i2c_execute();
   init_i2c_strategy_dispatcher();
   init_i2c_sysfs_base();
   init_i2c_sysfs();
   init_i2c_sysfs_conflicting_drivers();

   init_i2c_sysfs_i2c_sys_info();
   init_i2c_sysfs_i2c_info();
}

void terminate_i2c_services() {
   terminate_i2c_sysfs_i2c_info();
}
