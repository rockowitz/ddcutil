/** \file i2c_sysfs.h
 *
 *  Query /sys file system for information on I2C devices
 */

// Copyright (C) 2020-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_SYS_DRM_CONNECTOR_H_
#define I2C_SYS_DRM_CONNECTOR_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "util/coredefs_base.h"
#include "util/data_structures.h"
#include "util/drm_common.h"
// #include "util/drm_connector_state.h"   // /usr/include/xf86drmMode.h:43:10: fatal errorfatal error drm.h no such file


#include "base/i2c_bus_base.h"

extern GPtrArray * sys_drm_connectors;
extern bool all_drm_connectors_have_connector_id;

typedef struct {
   char * connector_name;
   char * connector_path;
   int    i2c_busno;
   int    connector_id;
   char * name;
   //  char * dev;
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

// Functions that use the persistent array of Sys_Drm_Connector:
GPtrArray*          get_sys_drm_connectors(bool rescan);
// GPtrArray*       get_sys_drm_connectors_sysinfo(bool rescan);
void                report_sys_drm_connectors(bool verbose, int depth);
Sys_Drm_Connector * find_sys_drm_connector(int busno, Byte * raw_edid, const char * connector_name);
Sys_Drm_Connector * find_sys_drm_connector_by_connector_id(int connector_number);
Sys_Drm_Connector * find_sys_drm_connector_by_connector_identifier(Drm_Connector_Identifier dci);
void                free_sys_drm_connector(void * conninfo);
Sys_Drm_Connector * find_sys_drm_connector_by_edid(Byte * raw_edid);
void                free_sys_drm_connectors();
Sys_Drm_Connector * i2c_check_businfo_connector(I2C_Bus_Info * bus_info);
int                 sys_drm_get_busno_by_connector_name(const char * connector_name);
bool                all_sys_drm_connectors_have_connector_id(bool rescan);
Bit_Set_256         buses_having_edid_from_sys_drm_connectors(bool rescan);
char *              find_drm_connector_name_by_busno(int busno);
char *              get_drm_connector_name_by_edid(Byte * edid_bytes);
Sys_Drm_Connector * find_sys_drm_connector_by_connector_name(const char * name);
Sys_Drm_Connector * find_sys_drm_connector_by_busno(int busno);


// Functions that access sysfs connector dirs directly, instead of using the
// persistent array of Sys_Drm_Connector:
Sys_Drm_Connector * one_drm_connector0(const char * dirname, const char * fn, int depth);
Sys_Drm_Connector * get_drm_connector(const char * fn, int depth);

void init_i2c_sysfs();


#endif /* I2C_SYS_DRM_CONNECTOR_H_ */
