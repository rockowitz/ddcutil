/** \file i2c_bus_sysfs.h
 *  Sysfs based functions for I2C buses
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_SYSFS_H_
#define I2C_BUS_SYSFS_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/coredefs.h"
#include "util/edid.h"

#include "base/i2c_bus_base.h"

/** Result of a search of the /sys/class/drm card-connector directories */
typedef struct {
   char *                 connector_name;
   int                    connector_id;
   Drm_Connector_Found_By found_by;
} Found_Sys_Drm_Connector;

bool             is_displaylink_device(int busno);

Found_Sys_Drm_Connector
                 find_sys_drm_connector_by_busno_or_edid(int busno, Byte * edid_bytes);
void             free_found_sys_drm_connector_result_contents(Found_Sys_Drm_Connector rec);

Byte *           get_connector_edid(const char * connector_name);
Parsed_Edid *    get_parsed_edid_for_businfo_using_sysfs(I2C_Bus_Info * businfo);

bool             is_adapter_class_display_controller(const char * adapter_class);
bool             is_valid_drm_connector_name(const char * connector_name);

// User specified I2C bus/DRM connector associations, i.e. --bus-drm-connector
void             add_busno_connector(int busno, const char * connector_name);
const char *     user_drm_connector_for_busno(int busno);
void             dbgrpt_busno_connector_table(int depth);

void             init_i2c_bus_sysfs();

#endif /* I2C_BUS_SYSFS_H_ */
