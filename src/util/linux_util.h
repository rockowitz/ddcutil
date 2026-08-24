/** \file linux_util.h
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2021-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_UTIL_H_
#define LINUX_UTIL_H_

// #include <acl/libacl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>

#include "linux_basic_util.h"

bool       is_readable_file(const char * filename);
int        get_kernel_config_parm(const char * parm_name, char * buffer, int bufsz);
bool       is_module_built_in(const char * module_name);

#define KERNEL_MODULE_NOT_FOUND     0     // not found
#define KERNEL_MODULE_BUILTIN       1     // module is built into kernel
#define KERNEL_MODULE_LOADABLE_FILE 2     // module is a loadable file

int        module_status_by_modules_builtin_or_existence(const char * module_name);
char       i2c_dev_status_by_boot_config_file();

void       rpt_lsof(                       const char * fqfn, int depth);
GPtrArray* rpt_lsof_collect0(              const char * fqfn, GPtrArray * collector);
GPtrArray* rpt_lsof_collect(               const char * fqfn);
GPtrArray* diagnose_open_failure_collect(  const char * fqfn, const char * msg, GPtrArray* collector);
void       diagnose_open_failure_to_syslog(const char * fqfn, const char * msg);

void       install_fatal_signal_handlers(void);

void       init_accumulated_sleep();
bool       recently_resumed_from_sleep_by_clocktime0(bool no_mutate, bool * detected_now_loc);
bool       recently_resumed_from_sleep_by_clocktime(bool * detected_now_loc);
uint64_t   millisec_since_resume_detected_by_clocktime();

/** Which of the detection methods in #recently_resumed_from_sleep() answered.
 *  Reported so that a caller's messages can describe the machine's actual
 *  state: inside an open sleep cycle the system need not have slept yet.
 */
typedef enum {
   RESUME_DETECTED_NONE,            ///< no recent resume
   RESUME_DETECTED_BY_DBUS,         ///< logind PrepareForSleep(false) timestamp
   RESUME_DETECTED_BY_CLOCKTIME,    ///< CLOCK_BOOTTIME/CLOCK_MONOTONIC divergence
   RESUME_DETECTED_IN_SLEEP_CYCLE   ///< running inside an open sleep cycle
} Resume_Detection;

const char * resume_detection_description(Resume_Detection detection);
bool       recently_resumed_from_sleep(int within_ms, uint64_t * millisec_since_loc,
                                       Resume_Detection * detection_loc);
#endif /* LINUX_UTIL_H_ */
