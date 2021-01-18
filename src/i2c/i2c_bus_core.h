/** \file i2c_bus_core.h
 *
 *  I2C bus detection and inspection
 */
// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

/** \cond */
#include <glib-2.0/glib.h>
// #include <glib.h>
#include <stdbool.h>
#include <stdio.h>
/** \endcond */

#include "util/edid.h"
#include "util/data_structures.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/execution_stats.h"
#include "base/status_code_mgt.h"


/** \def I2C_BUS_MAX maximum number of i2c buses this code supports */
#define I2C_BUS_MAX 32

/** \def I2C_SLAVE_ADDR_MAX Addresses on an I2C bus are 7 bits in size */
#define I2C_SLAVE_ADDR_MAX 128

// shared with i2c_bus_selector.c
extern GPtrArray * i2c_buses;

// Controls whether function #i2c_set_addr() retries from EBUSY error by
// changing ioctl op I2C_SLAVE to op I2C_SLAVE_FORCE.
extern bool i2c_force_slave_addr_flag;

extern bool i2c_force_bus;

// Basic I2C bus operations
int           i2c_open_bus(int busno, Call_Options callopts);
Status_Errno  i2c_close_bus(int fd, Call_Options callopts);
Status_Errno  i2c_set_addr(int fd, int addr, Call_Options callopts);

// EDID inspection
Status_Errno_DDC i2c_get_raw_edid_by_fd(int fd, Buffer * rawedid);
Status_Errno_DDC i2c_get_parsed_edid_by_fd(int fd, Parsed_Edid ** edid_ptr_loc);

// Retrieve and inspect bus information

#define I2C_BUS_EXISTS               0x80
#define I2C_BUS_ACCESSIBLE           0x40
#define I2C_BUS_ADDR_0X50            0x20      ///< detected I2C bus address 0x50
#define I2C_BUS_ADDR_0X37            0x10
#define I2C_BUS_ADDR_0X30            0x08      ///< detected write-only addr to specify EDID block number
#define I2C_BUS_EDP                  0x04      ///< bus associated with eDP display
#define I2C_BUS_PROBED               0x01      ///< has bus been checked?
#define I2C_BUS_VALID_NAME_CHECKED 0x0800
#define I2C_BUS_HAS_VALID_NAME     0x0400

#define I2C_BUS_INFO_MARKER "BINF"
/** Information about one I2C bus */
typedef
struct {
   char             marker[4];          ///< always "BINF"
   int              busno;              ///< I2C device number, i.e. N for /dev/i2c-N
   unsigned long    functionality;      ///< i2c bus functionality flags
   Parsed_Edid *    edid;               ///< parsed EDID, if slave address x50 active
   uint16_t         flags;              ///< I2C_BUS_* flags
} I2C_Bus_Info;

void i2c_dbgrpt_bus_info(I2C_Bus_Info * bus_info, int depth);
void i2c_report_active_display(I2C_Bus_Info * businfo, int depth);

// Simple bus detection, no side effects
bool i2c_device_exists(int busno);
int  i2c_device_count();           // simple /dev/i2c-n count, no side effects
Byte_Value_Array get_i2c_devices_by_existence_test();

// Bus inventory - detect and probe buses
int i2c_detect_buses();            // creates internal array of Bus_Info for I2C buses
void i2c_discard_buses();
I2C_Bus_Info * i2c_detect_single_bus(int busno);
void i2c_free_bus_info(I2C_Bus_Info * bus_info);

// Simple Bus_Info retrieval
I2C_Bus_Info * i2c_get_bus_info_by_index(int busndx);
I2C_Bus_Info * i2c_find_bus_info_by_busno(int busno);


// Reports all detected i2c buses:
int  i2c_report_buses(bool report_all, int depth);

void init_i2c_bus_core();

#endif /* I2C_BUS_CORE_H_ */
