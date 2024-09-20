/** @file i2c_sysfs_base.h */

// Copyright (C) 2020-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYSFS_BASE_H_
#define I2C_SYSFS_BASE_H_

#include <glib-2.0/glib.h>

// predicate functions
// typedef Dir_Filter_Func
bool is_drm_connector(const char * dirname, const char * simple_fn);
bool fn_equal(const char * filename, const char * val);
bool fn_starts_with(const char * filename, const char * val);
bool is_n_nnnn(const char * dirname, const char * simple_fn);

GPtrArray * get_sys_video_devices();
void        dbgrpt_sysfs_basic_connector_attributes(int depth);
char *      get_sys_drm_connector_name_by_connector_id(int connector_id);
char *      get_sys_drm_connector_name_by_busno(int busno);
bool        all_sys_drm_connectors_have_connector_id_direct();

char *  get_driver_for_adapter(char * adapter_path, int depth);
char *  find_adapter(char * path, int depth);
char *  find_adapter_and_get_driver(char * path, int depth);
char *  get_driver_for_busno(int busno);

typedef struct {
   int    i2c_busno;
   int    base_busno;
   int    connector_id;
   char * name;
} Connector_Bus_Numbers;

void dbgrpt_connector_bus_numbers(Connector_Bus_Numbers * cbn, int depth);
void free_connector_bus_numbers(Connector_Bus_Numbers * cbn);
void get_connector_bus_numbers(
      const char *            dirname,    // <device>/drm/cardN
      const char *            fn,         // card0-HDMI-1 etc
      Connector_Bus_Numbers * cbn);

void init_i2c_sysfs_base();

#endif /* I2C_SYSFS_BASE_H_ */
