/** @file i2c_bus_open_close.h
 *
 *  Opening and closing /dev/i2c devices
 *
 *  Functions related to getting a file descriptor for a bus and giving it back,
 *  including the EACCES retry episode that a resume from sleep can provoke
 *  while udev has yet to reapply the device ACLs.
 */
// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_OPEN_CLOSE_H_
#define I2C_BUS_OPEN_CLOSE_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/error_info.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

// Set from options in ddc_common_init.c
extern bool force_failure_i2c_open;                     // --f17
extern int  max_eacces_retry_ms;                        // --i11
extern int  max_eacces_retry_ct;                        // --i12
extern int  rate_limit_eacces_diagnostics_interval_sec; // --i13

// Bus open and close
#ifdef DETERMINED_UNUSED
void             i2c_add_open_failures_reported(Bit_Set_256 failures);
#endif
void             i2c_include_open_failures_reported(int busno);
Error_Info *     i2c_open_bus_basic(const char * filename,  Byte callopts, int* fd_loc);
Error_Info *     i2c_open_bus_basic_by_busno(int busno,  Byte callopts, int* fd_loc);
Error_Info *     i2c_open_bus(int busno, Byte callopts, int * fd_loc);
#ifdef ALT_LOCK_REC
Error_Info *     i2c_open_bus(int busno, Display_Lock_Record lockrec, Byte callopts, int * fd_loc);
#endif
Status_Errno     i2c_close_bus_basic(int busno, int fd, Call_Options callopts);
Status_Errno     i2c_close_bus(int busno, int fd, Call_Options callopts);

// Tests that a /dev/i2c bus can be opened for reading and writing
Error_Info *     simple_rw_test(int busno);

// Initialization
void             init_i2c_bus_open_close();

#endif /* I2C_BUS_OPEN_CLOSE_H_ */
