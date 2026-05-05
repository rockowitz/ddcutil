/** @file backtrace.h */

// Copyright (C) 2016-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BACKTRACE_H_
#define BACKTRACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <glib-2.0/glib.h>

#define ASSERT_WITH_BACKTRACE(_condition) \
do { \
   if ( !(_condition) ) {  \
      show_backtrace(0);   \
      assert(_condition);  \
   }                       \
} while(0)

GPtrArray * get_backtrace(int stack_adjust);
void backtrace_to_syslog(int priority, int stack_adjust);
void show_backtrace(int stack_adjust);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* BACKTRACE_H_ */
