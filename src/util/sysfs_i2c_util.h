/** @file sysfs_i2c_util.h
 *  i2c specific /sys functions
 */

// Copyright (C) 2020-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_I2C_UTIL_H_
#define SYSFS_I2C_UTIL_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "data_structures.h"

bool is_module_loaded_using_sysfs(const char * module_name);

GPtrArray * get_video_adapter_devices();

#endif /* SYSFS_I2C_UTIL_H_ */

