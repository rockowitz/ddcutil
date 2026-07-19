/** @file test_per_thread_data.c
 *
 *  Standalone unit test for src/base/per_thread_data.c: obtaining the current
 *  thread's Per_Thread_Data, which is created on demand, cached, and keyed by
 *  the thread id.  The thread-description accessors are compiled out
 *  (#ifdef REMOVED_20) and are not exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/linux_basic_util.h"    // get_thread_id
#include "base/per_thread_data.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)

int main(int argc, char ** argv) {
   init_per_thread_data();

   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   CK(ptd != NULL);
   CK(ptd_get_per_thread_data() == ptd);          // created once, then cached
   CK(ptd_get_per_thread_data() == ptd);          // stable across calls
   CK_INT(ptd->thread_id, get_thread_id());       // keyed by the current thread id

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
