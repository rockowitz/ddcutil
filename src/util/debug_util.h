/** @file debug_util.h
 *
 * Functions for debugging
 */

// Copyright (C) 2016-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DEBUG_UTIL_H_
#define DEBUG_UTIL_H_

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>

#define ASSERT_WITH_BACKTRACE(_condition) \
do { \
   if ( !(_condition) ) {  \
      show_backtrace(2);   \
      assert(_condition);  \
   }                       \
} while(0)

GPtrArray * get_backtrace(int stack_adjust);
void show_backtrace(int stack_adjust);

bool simple_dbgmsg(
        bool              debug_flag,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        char *            format,
        ...);

#define DBGF(debug_flag, format, ...) \
   do { if (debug_flag) simple_dbgmsg(debug_flag, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)

#endif /* DEBUG_UTIL_H_ */
