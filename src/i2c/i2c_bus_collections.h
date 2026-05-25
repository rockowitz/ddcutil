// i2c_bus_collections.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef I2C_BUS_COLLECTIONS_H_
#define I2C_BUS_COLLECTIONS_H_

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

#include "sysfs/sysfs_sys_drm_connector.h"

extern int  i2c_businfo_async_threshold;
extern bool force_failure_i2c_all_relevant_i2c_buses_rw;
extern bool force_failure_i2c_all_edids_readable_using_i2c;

// Bus inventory - detect and probe buses
Byte_Value_Array i2c_get_devices_by_existence_test(bool include_ignorable_devices);
Byte_Value_Array i2c_get_device_numbers_using_udev(bool include_ignorable_devices);
Byte_Value_Array i2c_detect_attached_buses();
Bit_Set_256      i2c_buses_bitset_from_businfo_array(GPtrArray * buses, bool only_connected);   // buses: array of I2C_Bus_Info
Bit_Set_256      i2c_nonlaptop_buses_bitset_from_businfo_array(GPtrArray * buses, bool only_connected);   // buses: array of I2C_Bus_Info
GPtrArray *      i2c_detect_buses0();
int              i2c_detect_buses();            // creates internal array of Bus_Info for I2C buses
I2C_Bus_Info *   i2c_detect_single_bus(int busno);

Bit_Set_256      i2c_detect_attached_buses_as_bitset();
Bit_Set_256      i2c_filter_buses_w_edid_as_bitset(Bit_Set_256 bs_all_buses);
Bit_Set_256      i2c_buses_w_edid_as_bitset();

void init_i2c_bus_collections();

#endif /* I2C_BUS_COLLECTIONS_H_ */
