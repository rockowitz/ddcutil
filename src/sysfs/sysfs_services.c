/** @file sysfs_services.c  */

// Copyright (C) 2022-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "i2c_dpms.h"
#include "i2c_sys_drm_connector.h"
#include "i2c_sysfs_base.h"
#include "i2c_sysfs_conflicting_drivers.h"
#include "i2c_sysfs_i2c_info.h"
#include "i2c_sysfs_i2c_sys_info.h"

#include "sysfs_services.h"

/** Master initializer for directory i2c */
void init_sysfs_services() {
   init_i2c_dpms();
   init_i2c_sysfs_base();
   init_i2c_sysfs();
   init_i2c_sysfs_conflicting_drivers();
   init_i2c_sysfs_i2c_sys_info();
   init_i2c_sysfs_i2c_info();
}

void terminate_sysfs_services() {
   terminate_i2c_sysfs_i2c_info();
}
