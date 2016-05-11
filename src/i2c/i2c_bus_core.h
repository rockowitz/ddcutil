/* i2c_bus_core.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

#include <stdbool.h>
#include <stdio.h>

#include <util/data_structures.h>

#include <base/core.h>
#include <base/displays.h>
#include <base/edid.h>
#include <base/execution_stats.h>
#include <base/status_code_mgt.h>


// Retrieve and inspect bus information

#define I2C_BUS_EXISTS        0x80
#define I2C_BUS_ACCESSIBLE    0x40
// if I2C_BUS_ADDRS_CHECKED set, then I2C_BUS_ADDR_0X50 and I2C_BUS_ADDR_0X37 are meaningful
// #define I2C_BUS_ADDRS_CHECKED 0x02
#define I2C_BUS_ADDR_0X50     0x20
#define I2C_BUS_ADDR_0X37     0x10
// #define I2C_BUS_EDID_CHECKED  0x04
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

int  i2c_get_busct();
void i2c_report_bus(int busno);
int  i2c_report_buses(bool report_all, int depth);

Display_Info_List i2c_get_valid_displays();

Bus_Info * i2c_get_bus_info(int busno);
Bus_Info * i2c_check_bus(Bus_Info * bus_info);
Bus_Info * i2c_find_bus_info_by_model_sn(const char * model, const char * sn);
Bus_Info * i2c_find_bus_info_by_edid(const Byte * pEdidBytes);
// void report_businfo(Bus_Info * bus_info);
bool i2c_is_valid_bus(int busno, bool emit_error_msg);

void i2c_report_active_display(Bus_Info * businfo, int depth);
void i2c_report_active_display_by_busno(int busno, int depth);


// Basic bus operations

int  i2c_open_bus(int busno, Failure_Action failure_action);
int i2c_open_bus_new(int busno, bool emit_error_msg);
int  i2c_close_bus(int fd, int busno, Failure_Action failure_action);
void i2c_set_addr(int fd, int addr);


// Bus functionality

unsigned long i2c_get_functionality_flags_by_fd(int fd);
unsigned long i2c_get_functionality_flags_by_busno(int busno);
#ifdef OLD
char * interpret_functionality(unsigned long functionality);  // must free returned string
#endif
char * i2c_interpret_functionality_into_buffer(unsigned long functionality, Buffer * buf);
// void i2c_show_functionality(int busno);  // no longer exists
bool i2c_verify_functions_supported(int busno, char * write_func_name, char * read_func_name);


// Retrieve EDID

Global_Status_Code i2c_get_raw_edid_by_fd( int fd,      Buffer * rawedidbuf);
#ifdef UNUSED
Global_Status_Code get_raw_edid_by_busno(int busno, Buffer * rawedidbuf, bool debug);
#endif
Parsed_Edid * i2c_get_parsed_edid_by_fd(int fd);
Parsed_Edid * i2c_get_parsed_edid_by_busno(int busno);

#endif /* I2C_BUS_CORE_H_ */

