/** \file acl_util.h
 *  Access Control List utlities
 */

// Copyright (C) 2021-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ACL_UTIL_H_
#define ACL_UTIL_H_

#include <acl/libacl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <sys/types.h>

GPtrArray * rpt_facl_collect1(const char* fqfn, GPtrArray* collector, int depth);
GPtrArray * rpt_facl_collect0(const char* fqfn, GPtrArray* collector, int depth);
char *      get_user_acl(const char* fqfn, uid_t uid);
bool        is_acl_rw(const char* fqfn, acl_tag_t tag, uid_t id);
bool        is_cur_user_acl_rw(const char* fqfn);
bool        is_file_group_acl_rw(const char* fqfn);

#endif /* ACL_UTIL_H_ */
