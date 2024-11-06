/** @file i2c_bus_core.h
 *
 *  I2C bus detection and inspection
 */
// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/edid.h"
#include "util/error_info.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/execution_stats.h"
#include "base/i2c_bus_base.h"
#include "base/parms.h"
#include "base/status_code_mgt.h"

#include <i2c/i2c_sys_drm_connector.h>

/** \def I2C_SLAVE_ADDR_MAX Addresses on an I2C bus are 7 bits in size */
#define I2C_SLAVE_ADDR_MAX 128

extern bool i2c_force_bus;
extern bool drm_enabled;
extern bool use_drm_connector_states;
extern bool try_get_edid_from_sysfs_first;
extern int  i2c_businfo_async_threshold;
extern bool cross_instance_locks_enabled;

Byte_Value_Array get_i2c_devices_by_existence_test(bool include_ignorable_devices);

// Lifecycle
I2C_Bus_Info *   remove_i2c_bus_info();

// Bus open and close
void             add_open_failures_reported(Bit_Set_256 failures);
void             include_open_failures_reported(int busno);
Error_Info *     open_bus_basic(const char * filename,  Byte callopts, int* fd_loc);
Error_Info *     i2c_open_bus_basic(const char * filename,  Byte callopts, int* fd_loc);
Error_Info *     i2c_open_bus(int busno, Byte callopts, int * fd_loc);
#ifdef ALT_LOCK_REC
Error_Info *     i2c_open_bus(int busno, Display_Lock_Record lockrec, Byte callopts, int * fd_loc);
#endif
Status_Errno     i2c_close_bus(int busno, int fd, Call_Options callopts);

// Bus inspection
I2C_Bus_Info *   i2c_get_and_check_bus_info(int busno);
bool             i2c_edid_exists(int busno);
Status_Errno     i2c_check_bus2(I2C_Bus_Info * businfo);
Error_Info *     i2c_check_open_bus_alive(Display_Handle * dh);

// Bus inventory - detect and probe buses
Bit_Set_256      buses_bitset_from_businfo_array(GPtrArray * buses, bool only_connected);   // buses: array of I2C_Bus_Info
GPtrArray *      i2c_detect_buses0();
int              i2c_detect_buses();            // creates internal array of Bus_Info for I2C buses
I2C_Bus_Info *   i2c_detect_single_bus(int busno);
Byte_Value_Array i2c_detect_attached_buses();
Bit_Set_256      i2c_detect_attached_buses_as_bitset();
Bit_Set_256      i2c_filter_buses_w_edid_as_bitset(BS256 bs_all_buses) ;
Bit_Set_256      i2c_buses_w_edid_as_bitset();

// Reports
void             i2c_report_active_bus(I2C_Bus_Info * businfo, int depth);

// Initialization
void             subinit_i2c_bus_core();
void             init_i2c_bus_core();

#endif /* I2C_BUS_CORE_H_ */
