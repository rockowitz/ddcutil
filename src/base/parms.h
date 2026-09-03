/** @file parms.h
 *
 *  System configuration and tuning
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PARMS_H_
#define PARMS_H_

#include "config.h"

//
// *** Build options that are not otherwise set
//

// STATIC_FUNCTIONS_VISIBLE defined in config.h
// If defined, remove static function qualifier on many functions to
// make them visible to asan, valgrind, backtrace
#ifdef STATIC_FUNCTIONS_VISIBLE
#define STATIC
#else
#define STATIC static
#endif

#define DEFAULT_ENABLE_TRACED_FUNCTION_STACK true


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
#define DEFAULT_SETVCP_VERIFY true
// Attempts, not retries: 1 means verify once and do not try again.  See the
// loop in ddc_set_verified_vcp_value_with_retry().  Set by --i5.
#define DEFAULT_MAX_SETVCP_VERIFY_TRIES 1

#define DEFAULT_DDCUTIL_SYSLOG_LEVEL DDCA_SYSLOG_WARNING
#define DEFAULT_LIBDDCUTIL_SYSLOG_LEVEL DDCA_SYSLOG_NOTICE

//
// Asynchronous Initialization
//

#define CHECK_ASYNC_NEVER 99
/** Parallelize bus checks if at least this number of checkable /dev/i2c devices exist */
// Was CHECK_ASYNC_NEVER.  What the parallelism hides is the I2C EDID read on a
// bus with nothing attached, which times out at roughly 65 ms.  Where those are
// common the saving is most of detect: on a laptop with 17 buses of which 12 are
// empty, i2c_check_bus() accounts for 849 ms of a 900 ms detect.  Where buses are
// cheap to probe there is nothing to hide and the two paths measure the same --
// on banner, 8 buses at ~10 ms each, 76.6 ms serial vs 77.1 ms threaded.  So the
// threshold costs nothing where it does not help.  A floor of 3 avoids spawning
// a thread to do one or two buses' work.  Set it to CHECK_ASYNC_NEVER during
// development: a serial scan makes the trace far easier to follow.
#define DEFAULT_BUS_CHECK_ASYNC_THRESHOLD 3
/** Parallelize DDC communication checks if at least this number of /dev/i2c devices have an EDID */
// on workstation banner with 4 displays, async  detect: 1.7 sec, non-async 3.4 sec
#define DEFAULT_DDC_CHECK_ASYNC_THRESHOLD 3


//
// Display detection
//

// Retry interval for retrying to open display
#define DEFAULT_OPEN_MAX_WAIT_MILLISEC 1000
#define DEFAULT_OPEN_WAIT_INTERVAL_MILLISEC 100

// Retry interval and max tries when checking that a display handle
// is still valid
#define CHECK_OPEN_BUS_ALIVE_RETRY_MILLISEC 100
#define CHECK_OPEN_BUS_ALIVE_MAX_TRIES 2

// During bus detection, retry interval and max tries for X37 detection
#define DETECT_X37_MAX_TRIES 2
#define DETECT_X37_NORMAL_RETRY_MS   200
#define DETECT_X37_NVIDIA_RETRY_MS  1000

//
//  EACCES error recovery
//

#define DEFAULT_PAUSE_AFTER_RESUME_MS   500
// Settling time after a udev add event, while udev applies permissions to the
// device node it has just created.  Separate from the pause after resume,
// which answers the same question for a different event and is tuned from
// different evidence, though the two values start out alike.
#define DEFAULT_PAUSE_AFTER_ADD_MS      500
// Time allowed after a udev event batch for further events to accumulate, so
// dw_udev_drain() takes them together rather than each provoking its own scan.
// Unlike the two pauses above this is not about permissions settling, so it is
// taken whatever grants access to the device node.  With the post-resume scan
// down to tens of milliseconds it is now the largest component of a resume,
// which is why it is tunable: --i16.
// n. reachable only when the settling pauses above do not run, since the
// coalesce pause is their remainder and they are the larger.  See the comment
// at the subtraction in dw_udev_watch().
// Was 200.  On a resume measured with --i16 50, 50 ms sufficed twice, taking
// 18 events each time with a follow-up drain of 0, and a companion run with
// --i16 0 bounded the straggle: the last events of the burst arrived no more
// than 33 ms after the drain that took the first ones, and cost an extra scan
// for want of a pause to absorb them.  100 ms leaves 2x margin over both
// figures, since burst width grows with the number of connectors the driver
// re-probes on thaw and the measurement is from a single laptop.
#define DEFAULT_DRAIN_PAUSE_MS          100
#define DEFAULT_MAX_EACCES_RETRY_MS    3000
// n. the count is a backstop.  With the intervals below, the elapsed time
// limit above is what normally ends the retries.
#define DEFAULT_MAX_EACCES_RETRY_CT       8
#define EACCES_RETRY_INITIAL_INTERVAL_MS 100
#define EACCES_RETRY_MAX_INTERVAL_MS     800
// After this long without any EACCES open failure, a new failure starts a
// new episode with a fresh retry budget.  See i2c_open_bus_basic().
#define EACCES_NEW_EPISODE_QUIET_MS    10000
#define DEFAULT_EACCES_DIAGNOSTIC_INTERVAL_SEC 10
// Minimum seconds between execution statistics reports from the udev watch
// loop.  0 disables the report, which is the default: it is diagnostic output,
// not something every libddcutil client should find in its journal.
#define DEFAULT_UDEV_WATCH_STATS_INTERVAL_SEC   0

#define DEFAULT_ENABLE_EARLY_PERMISSION_CHECKS  true
// #define ENABLE_DDCI_INIT_CHECK_DEV_I2C_DEVICES_RW false
// #define ENABLE_DW_START_CHECK_DEV_I2C_DEVICES_RW  true

//
// *** Watching for display changes
//

#define REPORT_UDEV_EVENTS    true   // report relevant detected UDEV events

#define DEFAULT_WATCH_MODE Watch_Mode_Dynamic
#define START_WATCH_DELAY_MILLISEC 0

// How frequently libddcutil checks for changes to connected displays
#define DEFAULT_UDEV_WATCH_LOOP_MILLISEC 500
#define DEFAULT_POLL_WATCH_LOOP_MILLISEC 2000
#define DEFAULT_XEVENT_WATCH_LOOP_MILLISEC 100

// Once an event is received that possibly indicates a display change,
// libddcutil repeatedly checks /sys/class/drm until the reported displays
// stabilize
/** Extra time to wait before first stabilization check */
#define DEFAULT_INITIAL_STABILIZATION_MILLISEC 0  // 500
/** Polling interval between stabilization checks */
#define DEFAULT_STABILIZATION_POLL_MILLISEC 100

// When checking if DDC communication has become enabled,
// checks occur at increasing multiples of this value.
#define WATCH_RETRY_THREAD_SLEEP_FACTOR_MILLISEC 500

//
// *** Miscellaneous
//

// EDID in /sys can have stale data
#define DEFAULT_TRY_GET_EDID_FROM_SYSFS  true

#define DEFAULT_FLOCK_POLL_MILLISEC      100
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

/** For waiting by QUIESCE operation */
#define QUIESCE_POLL_MAX_MILLISEC      3000
#define QUIESCE_POLL_INTERVAL_MILLISEC  100

#define STD_FUNCNAME_FIELD_SIZE 30

#endif /* PARMS_H_ */
