/** @file i2c_bus_core.h
 *
 *  I2C bus detection and inspection
 */
// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_CORE_H_
#define I2C_BUS_CORE_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/error_info.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/status_code_mgt.h"

/** \def I2C_SLAVE_ADDR_MAX Addresses on an I2C bus are 7 bits in size */
#define I2C_SLAVE_ADDR_MAX 128


extern bool try_get_edid_from_sysfs_first;

extern bool cross_instance_locks_enabled;
extern bool edp_always_laptop;

extern bool force_failure_i2c_open;
extern int  pause_after_resume_ms;
extern int  max_eacces_retry_ms;
extern int  max_eacces_retry_ct;
extern bool primitive_sysfs;

// Bus open and close
void             i2c_add_open_failures_reported(Bit_Set_256 failures);
void             i2c_include_open_failures_reported(int busno);
Error_Info *     i2c_open_bus_basic(const char * filename,  Byte callopts, int* fd_loc);
Error_Info *     i2c_open_bus_basic_by_busno(int busno,  Byte callopts, int* fd_loc);
Error_Info *     i2c_open_bus(int busno, Byte callopts, int * fd_loc);
#ifdef ALT_LOCK_REC
Error_Info *     i2c_open_bus(int busno, Display_Lock_Record lockrec, Byte callopts, int * fd_loc);
#endif
Status_Errno     i2c_close_bus_basic(int busno, int fd, Call_Options callopts);
Status_Errno     i2c_close_bus(int busno, int fd, Call_Options callopts);

// Bus inspection
Error_Info *     i2c_check_bus(I2C_Bus_Info * businfo);
I2C_Bus_Info *   i2c_get_and_check_bus_info(int busno);
bool             i2c_edid_exists(int busno);
bool             i2c_check_edid_exists_by_dh(Display_Handle * dh);
Error_Info *     i2c_check_open_bus_alive(Display_Handle * dh);

// Reports
void             i2c_report_active_bus(I2C_Bus_Info * businfo, int depth);

// Miscellaneous
Error_Info *     simple_rw_test(int busno);
// bool is_valid_drm_connector_name(const char * connector_name);

// Initialization
void             subinit_i2c_bus_core();
void             init_i2c_bus_core();

#ifdef USER_DRM_CONNECTOR
void add_busno_connector(int busno, const char * connector_name);
void dbgrpt_busno_connector_table(int depth);
#endif

#endif /* I2C_BUS_CORE_H_ */
