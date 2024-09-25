/** @file i2c_sysfs_sys_info.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_SYS_INFO_H_
#define I2C_SYSFS_SYS_INFO_H_

#include "util/data_structures.h"

typedef struct {
   int     busno;
   bool    is_amdgpu_display_port;
   char *  pci_device_path;
   char *  drm_connector_path;
   char *  connector;
   char *  ddc_path;
   char *  linked_ddc_filename;
   char *  device_name;
   char *  drm_dp_aux_name;
   char *  drm_dp_aux_dev;
   char *  i2c_dev_name;
   char *  i2c_dev_dev;
   char *  driver;
   char *  ddc_name;
   char *  ddc_i2c_dev_name;
   char *  ddc_i2c_dev_dev;
} I2C_Sys_Info;

#ifdef FUTURE
// In progress: Simplified I2C_Sys_Info for production as opposed to exploratory use
typedef struct {
   char * pci_device_path;
   char * driver;
   char * connector;
   char * drm_connector_path;
   char * device_name;
   int    busno;
} I2C_Fixed_Sys_Info;
#endif

I2C_Sys_Info * get_i2c_sys_info(int busno, int depth);
void           free_i2c_sys_info(I2C_Sys_Info * info);
void           dbgrpt_i2c_sys_info(I2C_Sys_Info * info, int depth);
void           dbgrpt_sys_bus_i2c(int depth);

void           init_i2c_sysfs_i2c_sys_info();


#endif /* I2C_SYSFS_I2C_SYS_INFO_H_ */
