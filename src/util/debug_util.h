/** @file debug_util.h
 *
 * Functions for debugging
 */

// Copyright (C) 2016-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DEBUG_UTIL_H_
#define DEBUG_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void set_simple_dbgmsg_min_funcname_size(int new_size);

bool simple_dbgmsg(
        bool              debug_flag,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        const char *      format,
        ...);

#define DBGF(debug_flag, format, ...) \
   do { if (debug_flag) simple_dbgmsg(debug_flag, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)

#define DBG(format, ...) \
   simple_dbgmsg(true, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* DEBUG_UTIL_H_ */
