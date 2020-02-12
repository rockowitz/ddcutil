/** @file debug_util.h
 *
 * Functions for debugging
 */

// Copyright (C) 2016-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DEBUG_UTIL_H_
#define DEBUG_UTIL_H_

#include <assert.h>
#include <glib-2.0/glib.h>

#define ASSERT_WITH_BACKTRACE(_condition) \
do { \
   if ( !(_condition) ) {  \
      show_backtrace(2);   \
      assert(_condition);  \
   }                       \
} while(0)

GPtrArray * get_backtrace(int stack_adjust);
void show_backtrace(int stack_adjust);

#endif /* DEBUG_UTIL_H_ */
