/** @file parms.h
 *
 *  System configuration and tuning
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PARMS_H_
#define PARMS_H_

//
// *** Timeout values
//

// n. the DDC spec lists timeout values in milliseconds

#define DDC_TIMEOUT_MILLIS_DEFAULT                      50   ///< Normal timeout in DDC spec
#define DDC_TIMEOUT_MILLIS_BETWEEN_GETVCP_WRITE_READ    40   ///< Between DDC Get Feature Request and Get Feature Reply
#define DDC_TIMEOUT_MILLIS_POST_SETVCP_WRITE            50   ///< Following DDC Set VCP Feature command
#define DDC_TIMEOUT_MILLIS_POST_SAVE_SETTINGS          200   ///< Following DDC Save Settings
#define DDC_TIMEOUT_MILLIS_BETWEEN_CAP_TABLE_FRAGMENTS  50
#define DDC_TIMEOUT_MILLIS_POST_CAP_TABLE_COMMAND       50   ///< needed? spec ambiguous

// Timeouts not part of DDC spec
#define DDC_TIMEOUT_NONE                                 0  ///< No timeout
#define DDC_TIMEOUT_MILLIS_NULL_RESPONSE_INCREMENT      50  ///< Used for dynamic tuned sleep in case of DDC Null Message response


//
// *** Method of low level I2C communication
//

// One of the following 2 defines must be enabled:
#define DEFAULT_I2C_IO_STRATEGY           I2C_IO_STRATEGY_IOCTL ///< Use ioctl() calls
// #define DEFAULT_I2C_IO_STRATEGY           I2C_IO_STRATEGY_FILEIO  ///< Use read() and write()

#define DEFAULT_DDC_READ_BYTEWISE         false       ///< Use single byte reads

#define EDID_BUFFER_SIZE                  256         ///< always 256
#define DEFAULT_EDID_WRITE_BEFORE_READ    true
#define DEFAULT_EDID_READ_SIZE            0           ///< 128, 256, 0=>dynamic
#define DEFAULT_EDID_READ_USES_I2C_LAYER  true
#define DEFAULT_EDID_READ_BYTEWISE        false

// Strategy    Bytewise    read edid uses local i2c call                      read edid uses i2c layer
// FILEIO      false       ok                                                 ok
// FILEIO      true        on P2411h and Acer, reads byes 0. 2, 4 of response EDID ok, getvcp fails
// IOCTL       false       ok                                                 All ok
// IOCTL       true        on P2411h and Acer, returns corrupt data           EDID ok, getvcp fails


//
// *** Retry Management ***
//

// Affects memory allocation in try_stats:
#define MAX_MAX_TRIES         15

// All MAX_..._TRIES values must be <= MAX_MAX_TRIES
#define INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES     4
#define INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES    10
#define INITIAL_MAX_MULTI_EXCHANGE_TRIES          8


//
// *** Cache file names
//

#define DSA_CACHE_FILENAME "dsa"
#define CAPABILITIES_CACHE_FILENAME "capabilities"
#define DISPLAYS_CACHE_FILENAME "displays"


//
// *** Option Defaults
//

#ifdef ENABLE_USB
#define DEFAULT_ENABLE_USB false
#endif
#define DEFAULT_ENABLE_UDF true
#define DEFAULT_ENABLE_CACHED_CAPABILITIES true
#define DEFAULT_ENABLE_CACHED_DISPLAYS false
#define DEFAULT_ENABLE_DSA2 true
#define DEFAULT_ENABLE_FLOCK true

#define DEFAULT_DDCUTIL_SYSLOG_LEVEL DDCA_SYSLOG_WARNING
#define DEFAULT_LIBDDCUTIL_SYSLOG_LEVEL DDCA_SYSLOG_NOTICE


//
// Asynchronous Initialization
//

#define CHECK_ASYNC_NEVER 99
/** Parallelize bus checks if at least this number of checkable /dev/i2c devices exist */
#define DEFAULT_BUS_CHECK_ASYNC_THRESHOLD CHECK_ASYNC_NEVER
/** Parallelize DDC communication checks if three are least this number of /dev/i2c devices having an EDID */
// on banner with 4 displays, async  detect: 1.7 sec, non-async 3.4 sec
#define DEFAULT_DDC_CHECK_ASYNC_THRESHOLD 3


//
// *** Miscellaneous
//

// EDID in /sys can have stale data
#define DEFAULT_TRY_GET_EDID_FROM_SYSFS  false

#define DEFAULT_FLOCK_POLL_MILLISEC      500
#define DEFAULT_FLOCK_MAX_WAIT_MILLISEC 3000

/** Maximum number of i2c buses this code supports */
#define I2C_BUS_MAX 64

/** Maximum number of values on getvcp or vcpinfo */
#define MAX_GETVCP_VALUES    50

/** Maximum number of values on setvcp command */
#define MAX_SETVCP_VALUES    50

/** Maximum command arguments */
// #define MAX_ARGS (MAX_SETVCP_VALUES*2)   // causes CMDID_* undefined
#define MAX_ARGS 100        // hack

#endif /* PARMS_H_ */
