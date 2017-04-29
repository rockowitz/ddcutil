/* i2c_bus_core.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \file
 *
 */

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

/** \cond */
#include <glib.h>
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


// Retrieve and inspect bus information

#define I2C_BUS_EXISTS        0x80
#define I2C_BUS_ACCESSIBLE    0x40
#define I2C_BUS_ADDR_0X50     0x20      ///< detected I2C bus address 0x50
#define I2C_BUS_ADDR_0X37     0x10
#define I2C_BUS_ADDR_0X30     0x08      // write-only addr to specify EDID block number
#define I2C_BUS_PROBED        0x01      // has bus been checked?

#define BUS_INFO_MARKER "BINF"
typedef
struct {
   char             marker[4];          // always "BINF"
   int              busno;              // n for /dev/i2c-n
   unsigned long    functionality;      // i2c bus functionality flags
   Parsed_Edid *    edid;
   Byte             flags;              // I2C_BUS_ flags
} Bus_Info;

void report_businfo(Bus_Info * bus_info, int depth);

bool i2c_bus_exists(int busno);
int  i2c_get_busct();
Bus_Info * i2c_get_bus_info_by_index(int busndx);
void i2c_report_bus(int busno);
int  i2c_report_buses(bool report_all, int depth);


GPtrArray* i2c_get_displays();

Bus_Info * i2c_get_bus_info(int busno, Byte findopts);
Bus_Info * i2c_check_bus(Bus_Info * bus_info);

Bus_Info * i2c_find_bus_info_by_mfg_model_sn(
              const char * mfg_id,
              const char * model,
              const char * sn,
              Byte findopts);

// void report_businfo(Bus_Info * bus_info);
bool i2c_is_valid_bus(int busno, Call_Options callopts);

void i2c_report_active_display(Bus_Info * businfo, int depth);
void i2c_report_active_display_by_busno(int busno, int depth);


// Basic bus operations

int           i2c_open_bus(int busno, Call_Options callopts);
Status_Errno  i2c_close_bus(int fd, int busno, Call_Options callopts);
Status_Errno  i2c_set_addr(int fd, int addr, Call_Options callopts);

extern bool i2c_force_slave_addr_flag;

// Bus functionality flags

unsigned long i2c_get_functionality_flags_by_fd(int fd);
unsigned long i2c_get_functionality_flags_by_busno(int busno);
char * i2c_interpret_functionality_into_buffer(unsigned long functionality, Buffer * buf);
void i2c_report_functionality_flags(long functionality, int maxline, int depth);
bool i2c_verify_functions_supported(int busno, char * write_func_name, char * read_func_name);


// Retrieve EDID

Public_Status_Code i2c_get_raw_edid_by_fd( int fd,      Buffer * rawedidbuf);
Parsed_Edid * i2c_get_parsed_edid_by_fd(int fd);
Parsed_Edid * i2c_get_parsed_edid_by_busno(int busno);

// new:

int i2c_detect_buses();
Bus_Info * i2c_get_bus_info_new(int busno);

#endif /* I2C_BUS_CORE_H_ */
