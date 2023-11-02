/** @file i2c_bus_core.h
 *
 *  I2C bus detection and inspection
 */
// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
/** \endcond */

#include "util/edid.h"
#include "util/data_structures.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/execution_stats.h"
#include "base/i2c_bus_base.h"
#include "base/parms.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_sysfs.h"

/** \def I2C_SLAVE_ADDR_MAX Addresses on an I2C bus are 7 bits in size */
#define I2C_SLAVE_ADDR_MAX 128

extern bool i2c_force_bus;
extern bool all_video_drivers_implement_drm;

// DPMS Detection
#define DPMS_STATE_X11_CHECKED 0x01
#define DPMS_STATE_X11_ASLEEP  0x02
#define DPMS_SOME_DRM_ASLEEP   0x04
#define DPMS_ALL_DRM_ASLEEP    0x08
typedef Byte Dpms_State;

extern Dpms_State dpms_state;

char *           interpret_dpms_state_t(Dpms_State state);
void             dpms_check_x11_asleep();
bool             dpms_check_drm_asleep(I2C_Bus_Info * businfo);

// Basic I2C bus operations
Sys_Drm_Connector *
                 i2c_check_businfo_connector(I2C_Bus_Info * bus_info);
char *           i2c_get_drm_connector_name(I2C_Bus_Info * bus_info);
void             i2c_check_bus(I2C_Bus_Info * bus_info);
void             i2c_reset_bus_info(I2C_Bus_Info * bus_info);
int              i2c_open_bus(int busno, Call_Options callopts);
Status_Errno     i2c_close_bus(int fd, Call_Options callopts);

void             i2c_report_active_display(I2C_Bus_Info * businfo, int depth);

bool             i2c_device_exists(int busno); // Simple bus detection, no side effects
int              i2c_device_count();           // simple /dev/i2c-n count, no side effects
Byte_Value_Array get_i2c_devices_by_existence_test();

void             add_open_failures_reported(Bit_Set_256 failures);
void             include_open_failures_reported(int busno);

// Bus inventory - detect and probe buses
int              i2c_detect_buses();            // creates internal array of Bus_Info for I2C buses
void             i2c_discard_buses();
I2C_Bus_Info *   i2c_detect_single_bus(int busno);

void             init_i2c_bus_core();

#endif /* I2C_BUS_CORE_H_ */
