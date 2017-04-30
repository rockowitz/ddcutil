/* parms.h
 *
 * Tunable parameters.
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

/** \file
 * System configuration and tuning
 */

#ifndef PARMS_H_
#define PARMS_H_

//
// *** Timeout values
//

// n. the DDC spec lists timeout values in milliseconds

/** Normal timeout value in DDC spec */
#define DDC_TIMEOUT_MILLIS_DEFAULT                 50

#ifdef UNUSED
/** Timeout between DDC Get Feature Request and Get Feature Reply */
#define DDC_TIMEOUT_MILLIS_POST_GETVCP_WRITE       40  // per spec
#endif

/** Timeout following a DDC Set VCP Feature command, per DDC spec */
#define DDC_TIMEOUT_MILLIS_POST_SETVCP_WRITE       50

/** Timeout following a DDC Save Command Settings command, per DDC spec */
#define DDC_TIMEOUT_POST_SAVE_SETTINGS            200

#ifdef UNUSED
#define DDC_TIMEOUT_MILLIS_POST_CAPABILITIES_READ  50
#endif

// not part of spec
/** Timeout between retries */
#define DDC_TIMEOUT_MILLIS_RETRY                    200

/** Use default timeout */
#define DDC_TIMEOUT_USE_DEFAULT                      -1

/** No timeout */
#define DDC_TIMEOUT_NONE                              0

/** Timeout value used for dynamic tuned sleep in case of DDC Null Message response */
#define DDC_TIMEOUT_MILLIS_NULL_RESPONSE_INCREMENT  100


//
// *** Choose method of low level IC2 communication
//

// Parms used within production portion of code

#define DEFAULT_I2C_IO_STRATEGY  I2C_IO_STRATEGY_FILEIO
// #define DEFAULT_I2C_IO_STRATEGY  I2C_IO_STRATEGY_IOCTL


// Parms used only within testcase portion of code:

#define DEFAULT_I2C_WRITE_MODE "write"
// #define DEFAULT_I2C_WRITE_MODE "ioctl_write"
//#define DEFAULT_I2C_WRITE_MODE  "i2c_smbus_write_i2c_block_data"

#define DEFAULT_I2C_READ_MODE  "read"
// #define DEFAULT_I2C_READ_MODE  "ioctl_read"
// i2c_smbus_read_i2c_block_data can't handle capabilities fragments 32 bytes in size, since with
// "envelope" the packet exceeds the i2c_smbus_read_i2c_block_data 32 byte limit

// Notes on I2C IO strategies
//
// TODO: move comments re smbus problems to low level smbus functions (currently in i2c_base_io.c)
//
// Default settings in i2c_io.c
// valid write modes: "write", "ioctl_write", "i2c_smbus_write_i2c_block_data"
// valid read modes:  "read",  "ioctl_read",  "i2c_smbus_read_i2c_block_data"
// 11/2015: write modes "write" and "ioctl_write" both work
//          "i2c_smbus_write_i2c_block_data" returns ERRNO EINVAL, invalid argument
//          "read" and "ioctl_read" both work, appear comparable
//          fails: "i2c_smb_read_i2c_block_data"

//
// *** Retry Management ***
//

// Affects memory allocation in try_stats:
#define MAX_MAX_TRIES         15

// All MAX_..._TRIES values must be <= MAX_MAX_TRIES
#define MAX_WRITE_ONLY_EXCHANGE_TRIES     4
#define MAX_WRITE_READ_EXCHANGE_TRIES    10
#define MAX_MULTI_EXCHANGE_TRIES          8


//
// *** Miscellaneous
//

/** Maximum numbers of values on setvcp command */
#define MAX_SETVCP_VALUES    50

/** Maximum command arguments */
// #define MAX_ARGS (MAX_SETVCP_VALUES*2)   // causes CMDID_* undefined
#define MAX_ARGS 100        // hack

/** Parallelize display checks during initialization if at least this  number of displays */
// on banner with 4 displays, async  detect: 1.7 sec, non-async 3.4 sec
#define DISPLAY_CHECK_ASYNC_THRESHOLD   2
#define DISPLAY_CHECK_ASYNC_NEVER    0xff

#endif /* PARMS_H_ */
