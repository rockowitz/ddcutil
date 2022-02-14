/** \file i2c_sysfs.h
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_H_
#define I2C_SYSFS_H_

#include <glib-2.0/glib.h>

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


typedef struct {
   char * connector_name;
   char * connector_path;
   int    i2c_busno;
   char * name;
   char * dev;
   char * ddc_dir_path;
   bool   is_aux_channel;
   int    base_busno;
   char * base_name;
   char * base_dev;
   Byte * edid_bytes;
   gsize  edid_size;
   char * enabled;
   char * status;
#ifdef UNNECESSARY
   // n. can be multiple
   char * conflicting_device;
   char * conflicting_driver;
#endif
} Sys_Drm_Connector;


GPtrArray * scan_sys_drm_connectors(int depth);
GPtrArray* get_sys_drm_connectors(bool rescan);
void report_sys_drm_connectors(int depth);
Sys_Drm_Connector * find_sys_drm_connector_by_busno(int busno);
void free_sys_drm_connectors();


GPtrArray * collect_conflicting_drivers(int busno, int depth);
GPtrArray * collect_conflicting_drivers_for_any_bus(int depth);
void report_conflicting_drivers(GPtrArray * conflicts, int depth);   // for a single busno
void free_conflicting_drivers(GPtrArray* conflicts);
GPtrArray * conflicting_driver_names(GPtrArray * conflicts);
char * conflicting_driver_names_string_t(GPtrArray * conflicts);

GPtrArray * get_all_i2c_info(bool rescan, int depth);
void dbgrpt_all_sysfs_i2c_info(GPtrArray * infos, int depth);
char * get_conflicting_drivers_for_bus(int busno);

#endif /* I2C_SYSFS_H_ */
