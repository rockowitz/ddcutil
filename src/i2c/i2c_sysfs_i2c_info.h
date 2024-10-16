/** @file i2c_sysfs_i2c_info.h */

// Copyright (C) 2020-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_I2C_INFO_H_
#define I2C_SYSFS_I2C_INFO_H_

#include <glib-2.0/glib.h>

#include "util/data_structures.h"

typedef struct {
   int    busno;
   char * name;
   char * adapter_path;
   char * adapter_class;
   char * driver;
   char * driver_version;
   GPtrArray * conflicting_driver_names;
} Sysfs_I2C_Info;

void        free_sysfs_i2c_info(Sysfs_I2C_Info * info);
Sysfs_I2C_Info *  get_i2c_driver_info(int busno, int depth);
GPtrArray * get_all_sysfs_i2c_info(bool rescan, int depth);
void        dbgrpt_all_sysfs_i2c_info(GPtrArray * infos, int depth);
#ifdef UNUSED
char *      get_conflicting_drivers_for_bus(int busno);
#endif
Bit_Set_256 get_possible_ddc_ci_bus_numbers_using_sysfs_i2c_info();
void        init_i2c_sysfs_i2c_info();
void        terminate_i2c_sysfs_i2c_info();

#endif /* I2C_SYSFS_I2C_INFO_H_ */
