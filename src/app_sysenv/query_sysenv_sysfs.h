/** @file query_sysenv_sysfs.h
 *
 *  Query environment using /sys file system
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_SYSFS_H_
#define QUERY_SYSENV_SYSFS_H_

#include "query_sysenv_base.h"

typedef struct {
   ushort   vendor_id;
   ushort   device_id;
   ushort   subdevice_id;    // subsystem device id
   ushort   subvendor_id;    // subsystem vendor id
} Device_Ids;
Device_Ids read_device_ids1(char * cur_dir_name);
Device_Ids read_device_ids2(char * cur_dir_name);

void query_card_and_driver_using_sysfs(Env_Accumulator * accum);
void query_loaded_modules_using_sysfs();
void query_sys_bus_i2c(Env_Accumulator * accum);
void query_sys_amdgpu_parameters(int depth);
void query_drm_using_sysfs();
void dump_sysfs_i2c();
void show_relevant_char_major_numbers();

void init_query_sysfs();

#endif /* QUERY_SYSENV_SYSFS_H_ */
