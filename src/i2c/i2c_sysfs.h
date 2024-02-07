/** \file i2c_sysfs.h
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_H_
#define I2C_SYSFS_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "util/coredefs_base.h"
#include "util/data_structures.h"

#include "base/i2c_bus_base.h"

extern GPtrArray * sys_drm_connectors;
extern GPtrArray * sys_drm_connectors_fixedinfo;

char * find_adapter(char * path, int depth);
char * get_driver_for_adapter(char * adapter_path, int depth);
char * get_driver_for_busno(int busno);

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

// In progress: Simplified I2C_Sys_Info for production as opposed to exploratory use
typedef struct {
   char * pci_device_path;
   char * driver;
   char * connector;
   char * drm_connector_path;
   char * device_name;
   int    busno;
} I2C_Fixed_Sys_Info;

void           free_i2c_sys_info(I2C_Sys_Info * info);
I2C_Sys_Info * get_i2c_sys_info(int busno, int depth);
void           dbgrpt_i2c_sys_info(I2C_Sys_Info * info, int depth);
void           dbgrpt_sys_bus_i2c(int depth);
Bit_Set_256    get_possible_ddc_ci_bus_numbers();
bool           all_sysfs_i2c_info_drm(bool rescan);


#define SYS_DRM_CONNECTOR_MARKER "SDRC"
typedef struct {
   char   marker[4];
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
} Sys_Drm_Connector;

GPtrArray*          get_sys_drm_connectors(bool rescan);
// GPtrArray*          get_sys_drm_connectors_sysinfo(bool rescan);
void                report_sys_drm_connectors(int depth);
Sys_Drm_Connector * find_sys_drm_connector(int busno, Byte * raw_edid, const char * connector_name);
Sys_Drm_Connector * find_sys_drm_connector_by_edid(Byte * raw_edid);
void                free_sys_drm_connectors();
Sys_Drm_Connector * i2c_check_businfo_connector(I2C_Bus_Info * bus_info);
int                 sys_drm_get_busno_by_connector(const char * connector_name);


// Simplified Sys_Drm_Connector for production use
typedef struct {
   char * connector_name;
   char * connector_path;
   int    i2c_busno;

   char * name;
   //  char * dev;
   char * ddc_dir_path;
   bool   is_aux_channel;
   int    base_busno;
   char * base_name;
   char * base_dev;
   Byte * edid_bytes;
   gsize  edid_size;
} Sys_Drm_Connector_FixedInfo;

void                report_sys_drm_connectors_fixedinfo(int depth);


typedef struct {
   int     i2c_busno;
   char *  n_nnnn;
   char *  name;            // n_nnnn/name
   char *  driver_module;   // basename(realpath(n_nnnn/driver/module))
   char *  modalias;        // n_nnnn/modalias
   Byte *  eeprom_edid_bytes;
   gsize   eeprom_edid_size;
} Sys_Conflicting_Driver;

GPtrArray * collect_conflicting_drivers(int busno, int depth);
GPtrArray * collect_conflicting_drivers_for_any_bus(int depth);
void        report_conflicting_drivers(GPtrArray * conflicts, int depth);   // for a single busno
void        free_conflicting_drivers(GPtrArray* conflicts);
GPtrArray * conflicting_driver_names(GPtrArray * conflicts);
char *      conflicting_driver_names_string_t(GPtrArray * conflicts);


typedef struct {
   int    busno;
   char * name;
   char * adapter_path;
   char * adapter_class;
   char * driver;
   char * driver_version;
   GPtrArray * conflicting_driver_names;
} Sysfs_I2C_Info;

GPtrArray * get_all_sysfs_i2c_info(bool rescan, int depth);
void        dbgrpt_all_sysfs_i2c_info(GPtrArray * infos, int depth);
char *      get_conflicting_drivers_for_bus(int busno);

void consolidated_i2c_sysfs_report(int depth);


#ifdef FOR_FUTURE_USE
typedef struct {
   char * connector;
   int    busno;
   Display_Ref * dref;    // currently
}   Connector_Busno_Dref;
extern GPtrArray * cbd_table;
typedef GPtrArray Connector_Busno_Dref_Table;

Connector_Busno_Dref_Table * create_connector_busnfo_dref_table();
Connector_Busno_Dref * new_cbd0(int busno);
Connector_Busno_Dref * new_cbd(const char * connector, int busno);
Connector_Busno_Dref * get_cbd_by_connector(const char * connector);
Connector_Busno_Dref * get_cbd_by_busno(int busno);
// if dref != NULL, replaces, if NULL, just erases
void                   set_cbd_connector(Connector_Busno_Dref * cbd, Display_Ref * dref);
void dbgrpt_cbd_table(Connector_Busno_Dref_Table * cbd_table, int depth);
#endif


GPtrArray * get_sys_video_devices();

char * get_drm_connector_name_by_busno(int busno);
char * get_drm_connector_name_by_edid(Byte * edid_bytes);
Sys_Drm_Connector * find_sys_drm_connector_by_connector_name(const char * name);
bool   is_drm_display_by_busno(int busno);


typedef struct {
   GPtrArray * all_connectors;
   GPtrArray * connectors_having_edid;
} Sysfs_Connector_Names;

// Sysfs_Connector_Names sysfs_drm_connector_names;


Sysfs_Connector_Names get_sysfs_drm_connector_names();
bool sysfs_connector_names_equal(Sysfs_Connector_Names cn1, Sysfs_Connector_Names cn2);

void free_sysfs_connector_names_contents(Sysfs_Connector_Names names_struct);
void dbgrpt_sysfs_connector_names(Sysfs_Connector_Names connector_names, int depth);

gpointer g_string_copy_func(gconstpointer src, gpointer data);
GPtrArray * gaux_deep_copy_string_array(GPtrArray * old_names);
Sysfs_Connector_Names copy_sysfs_connector_names_struct(Sysfs_Connector_Names original);

char * find_sysfs_drm_connector_name_by_edid(GPtrArray* connector_names, Byte * edid);

void init_i2c_sysfs();
void terminate_i2c_sysfs();

#endif /* I2C_SYSFS_H_ */
