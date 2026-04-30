/** \file linux_util.h
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2021-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_UTIL_H_
#define LINUX_UTIL_H_

#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>

int        get_kernel_config_parm(const char * parm_name, char * buffer, int bufsz);
bool       is_module_built_in(const char * module_name);

#define KERNEL_MODULE_NOT_FOUND     0     // not found
#define KERNEL_MODULE_BUILTIN       1     // module is built into kernel
#define KERNEL_MODULE_LOADABLE_FILE 2     // module is a loadable file

int        module_status_by_modules_builtin_or_existence(const char * module_name);

intmax_t   get_thread_id();
intmax_t   get_process_id();
bool       is_valid_thread_or_process(pid_t id);

bool       get_file_owner_group_ids(const char * fqfn, int * uid_loc, int * gid_loc);
char *     uid_name(int uid);
char *     gid_name(int gid);

void       rpt_lsof(                       const char * fqfn, int depth);
GPtrArray* rpt_lsof_collect0(              const char * fqfn, GPtrArray * collector);
GPtrArray* rpt_lsof_collect(               const char * fqfn);
GPtrArray* diagnose_open_failure_collect(  const char * fqfn, const char * msg, GPtrArray* collector);
void       diagnose_open_failure_to_syslog(const char * fqfn, const char * msg);

void       install_segv_handler(void);

void       init_baseline_accumulated_sleep_ns();
bool       recently_resumed_from_sleep();
#endif /* LINUX_UTIL_H_ */
