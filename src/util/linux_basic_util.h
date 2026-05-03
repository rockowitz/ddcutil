/** \file linux_basic_util.h
 *  Basic Linux user/group/thread utilities
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_BASIC_UTIL_H_
#define LINUX_BASIC_UTIL_H_

#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>

intmax_t   get_thread_id();
intmax_t   get_process_id();
bool       is_valid_thread_or_process(pid_t id);

char *     uid_name(int uid);
char *     gid_name(int gid);
bool       group_i2c_exists();
bool       check_group_i2c_collect(GPtrArray* collector);
bool       cur_user_in_group_i2c();
bool       get_file_owner_group_ids(const char * fqfn, uid_t* uid_loc, gid_t* gid_loc);
bool       is_file_group_i2c(const char * fqfn);

#endif /* LINUX_BASIC_UTIL_H_ */
