/** @file systemd_util.h
 */

// Copyright (C) 2017-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSTEMD_UTIL_H_
#define SYSTEMD_UTIL_H_

#include <glib-2.0/glib.h>

GPtrArray * get_current_boot_messages(char ** filter_terms, bool ignore_case, int limit);

#endif /* SYSTEMD_UTIL_H_ */
