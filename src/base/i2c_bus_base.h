/** @file i2c_bus_base.h
 *
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_BASE_H_
#define I2C_BUS_BASE_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>

#include "base/i2c_bus_aux.h"

extern GPtrArray * all_i2c_buses;
extern GPtrArray * removed_i2c_buses;

// Generalized Bus_Info retrieval
I2C_Bus_Info *   i2c_find_bus_info_in_gptrarray_by_busno(GPtrArray * buses, int busno);
int              i2c_find_bus_info_index_in_gptrarray_by_busno(GPtrArray * buses, int busno);

// Bus Info retrieval
I2C_Bus_Info *   i2c_get_bus_info_by_index(guint busndx);
I2C_Bus_Info *   i2c_find_bus_info_by_busno(int busno);
I2C_Bus_Info *   i2c_find_businfo_by_drm_connector_id(int drm_connector_id);
int              i2c_find_bus_info_index_by_busno(int busno);
int              i2c_find_bus_info_index_by_businfo(I2C_Bus_Info * businfo);

// Lifecycle
bool i2c_add_businfo(I2C_Bus_Info * businfo);

#ifdef DETERMINED_UNUSED
I2C_Bus_Info * i2c_add_bus_new_by_busno(int busno);
#endif
bool           i2c_remove_businfo(I2C_Bus_Info * businfo);
void           i2c_remove_businfo_by_busno(int busno);

// void          i2c_add_bus_info(I2C_Bus_Info * businfo);
I2C_Bus_Info *   i2c_get_bus_info(int busno, bool* new_info);

void             i2c_reset_bus_info(I2C_Bus_Info * businfo);

// Reset arrays
void             i2c_discard_buses0(GPtrArray* buses);
void             i2c_discard_buses();

// Debug arrays
int              i2c_dbgrpt_buses(bool report_all, bool include_sysfs_info, int depth);  // Reports all detected i2c buses
void             i2c_dbgrpt_buses_summary(int depth);

// Initialization and termination
void init_i2c_bus_base();
void terminate_i2c_bus_base();

#endif /* I2C_BUS_BASE_H_ */
