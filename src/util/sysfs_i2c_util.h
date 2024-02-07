/** @file sysfs_i2c_util.h
 *  i2c specific /sys functions
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_I2C_UTIL_H_
#define SYSFS_I2C_UTIL_H_

#include <stdbool.h>

#include "data_structures.h"

char *
get_i2c_sysfs_driver_by_busno(
      int busno);

char *
get_i2c_sysfs_driver_by_device_name(
      char * device_name);

char *
get_i2c_sysfs_driver_by_fd(
      int fd);

uint32_t
get_i2c_device_sysfs_class(
      int busno);

bool
is_module_loaded_using_sysfs(
      const char * module_name);

char *
get_i2c_device_sysfs_name(
      int busno);

bool
sysfs_is_ignorable_i2c_device(
      int busno);

#endif /* SYSFS_I2C_UTIL_H_ */

