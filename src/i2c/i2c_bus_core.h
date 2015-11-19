/*  i2c_bus_core.h
 *
 *  Created on: Jun 13, 2014
 *      Author: rock
 */

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

#include <stdbool.h>
#include <stdio.h>

#include <util/data_structures.h>

#include <base/execution_stats.h>
#include <base/common.h>
#include <base/displays.h>
#include <base/edid.h>
#include <base/msg_control.h>
#include <base/status_code_mgt.h>
#include <base/util.h>



// DDC IO statistics gathering and reporting

// void init_i2c_bus_stats(I2C_Call_Stats * pstats);
// #ifdef UNUSED
// I2C_Call_Stats * get_i2c_bus_stats();
// #endif


// Retrieve and inspect bus information

#define I2C_BUS_EXISTS        0x80
#define I2C_BUS_ACCESSIBLE    0x40
// if I2C_BUS_ADDRS_CHECKED set, then I2C_BUS_ADDR_0X50 and I2C_BUS_ADDR_0X37 are meaningful
// #define I2C_BUS_ADDRS_CHECKED 0x02
#define I2C_BUS_ADDR_0X50     0x20
#define I2C_BUS_ADDR_0X37     0x10
// #define I2C_BUS_EDID_CHECKED  0x04
#define I2C_BUS_PROBED        0x01      // has bus been checked?



typedef
struct {
   char             marker[4];          // always "BINF"
   int              busno;
   unsigned long    functionality;      // i2c bus functionality flags
   Parsed_Edid *    edid;
   Byte             flags;              // I2C_BUS_ flags
} Bus_Info;

int get_i2c_busct();

#ifdef DEPRECATED
DisplayIdInfo* get_bus_display_id_info(int busno);
#endif
void report_i2c_bus(int busno);
int  report_i2c_buses(bool report_all);

Display_Info_List get_valid_i2c_displays();

Bus_Info * get_bus_info(int busno);
Bus_Info * check_i2c_bus(Bus_Info * bus_info);
Bus_Info * find_bus_info_for_monitor(const char * model, const char * sn);
Bus_Info * find_bus_info_by_edid(const Byte * pEdidBytes);
void report_businfo(Bus_Info * bus_info);
bool is_valid_bus(int busno, bool emit_error_msg);


// Basic bus operations

int open_i2c_bus(int busno, Failure_Action failure_action);
int close_i2c_bus(int fd, int busno, Failure_Action failure_action);
void set_addr(int file, int addr);


// Bus functionality

unsigned long get_i2c_functionality_flags_by_fd(int fd);
unsigned long get_i2c_functionality_flags_by_busno(int busno);
#ifdef OLD
char * interpret_functionality(unsigned long functionality);  // must free returned string
#endif
char * interpret_functionality_into_buffer(unsigned long functionality, Buffer * buf);
void show_functionality(int busno);
bool verify_functions_supported(int busno, char * write_func_name, char * read_func_name);


// Retrieve EDID

Global_Status_Code get_raw_edid_by_fd( int fd,      Buffer * rawedidbuf, bool debug);
#ifdef UNUSED
Global_Status_Code get_raw_edid_by_busno(int busno, Buffer * rawedidbuf, bool debug);
#endif
Parsed_Edid * get_parsed_edid_by_fd(int fd, bool debug);
Parsed_Edid * get_parsed_edid_by_busno(int busno);

#endif /* I2C_BUS_CORE_H_ */

