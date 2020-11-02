/** \file i2c_sysfs.h
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_H_
#define I2C_SYSFS_H_

typedef struct {
   int     busno;
   bool    is_display_port;
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

void           free_i2c_sys_info(I2C_Sys_Info * info);
I2C_Sys_Info * get_i2c_sys_info(int busno, int depth);
void           report_i2c_sys_info(I2C_Sys_Info * info, int depth);
void           dbgrpt_sys_bus_i2c(int depth);

#endif /* I2C_SYSFS_H_ */
