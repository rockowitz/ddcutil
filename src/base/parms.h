/*
 * parms.h
 *
 *  Created on: Oct 23, 2015
 *      Author: rock
 *
 *  Tunable parameters.
 */

#ifndef PARMS_H_
#define PARMS_H_

//not very informative
// #define USE_LIBEXPLAIN

// Should this really be in parms?   These values are obtained from the DDC spec.

// Timeout values in microseconds
// n. the DDC spec lists timeout values in milliseconds
#define DDC_TIMEOUT_MILLIS_DEFAULT                 50    // per spec
// #define DDC_TIMEOUT_MILLIS_DEFAULT                 20
#ifdef UNUSED
#define DDC_TIMEOUT_MILLIS_POST_GETVCP_WRITE       40  // per spec
// #define DDC_TIMEOUT_MILLIS_POST_GETVCP_WRITE       20
#endif
#define DDC_TIMEOUT_MILLIS_POST_SETVCP_WRITE       50
#ifdef UNUSED
#define DDC_TIMEOUT_MILLIS_POST_CAPABILITIES_READ  50
#endif
// not part of spec
#define DDC_TIMEOUT_MILLIS_RETRY                  200
#define DDC_TIMEOUT_USE_DEFAULT                    -1
#define DDC_TIMEOUT_NONE                            0


// Default settings in i2c_io.c
// valid write modes: "write", "ioctl_write", "i2c_smbus_write_i2c_block_data"
// valid read modes:  "read",  "ioctl_read",  "i2c_smbus_read_i2c_block_data"
// 11/2015: write modes "write" and "ioctl_write" both work
//          "i2c_smbus_write_i2c_block_data" returns ERRNO EINVAL, invalid argument
//          "read" and "ioctl_read" both work, appear comparable
//          fails: "i2c_smb_read_i2c_block_data"
#define DEFAULT_I2C_WRITE_MODE "write"
// #define DEFAULT_I2C_WRITE_MODE "ioctl_write"
//#define DEFAULT_I2C_WRITE_MODE  "i2c_smbus_write_i2c_block_data"
#define DEFAULT_I2C_READ_MODE  "read"
// #define DEFAULT_I2C_READ_MODE  "ioctl_read"
// i2c_smbus_read_i2c_block_data can't handle capabilities fragments 32 bytes in size, since with
// "envelope" the packet exceeds the i2c_smbus_read_i2c_block_data 32 byte limit

// Affects memory allocation in try_stats:
#define MAX_MAX_TRIES         15

// All MAX_..._TRIES values must be <= MAX_MAX_TRIES
#define MAX_WRITE_ONLY_EXCHANGE_TRIES     4
#define MAX_WRITE_READ_EXCHANGE_TRIES    10
#define MAX_MULTI_EXCHANGE_TRIES          8


#endif /* PARMS_H_ */
