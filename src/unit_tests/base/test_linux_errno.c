/** @file test_linux_errno.c
 *
 *  Standalone unit tests for src/base/linux_errno.c: mapping Linux errno values
 *  to their symbolic names and back, the info accessor, and the formatted
 *  description.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/linux_errno.h"

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

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

#define CK_HAS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  \"%s\" does not contain \"%s\"\n", __LINE__, \
             _a ? _a : "(null)", _n); } \
} while(0)

int main(int argc, char ** argv) {
   init_linux_errno();

   // errno value -> symbolic name (positive errno)
   CK_STR(linux_errno_name(EBUSY),  "EBUSY");
   CK_STR(linux_errno_name(EINVAL), "EINVAL");
   CK_STR(linux_errno_name(ENOENT), "ENOENT");
   CK(linux_errno_name(999999) == NULL);      // not a known errno

   // symbolic name -> errno number (returned negated, as a modulated status code)
   int n = 0;
   CK(errno_name_to_number("EBUSY", &n) == true);
   CK_INT(n, -EBUSY);
   CK(errno_name_to_number("EINVAL", &n) == true);
   CK_INT(n, -EINVAL);
   CK(errno_name_to_number("ENOSUCH", &n) == false);
   CK_INT(n, 0);

   // info accessor
   Status_Code_Info * info = get_errno_info(EBUSY);
   CK(info != NULL);
   if (info)
      CK_STR(info->name, "EBUSY");

   // formatted description includes the symbolic name
   CK_HAS(linux_errno_desc(EBUSY), "EBUSY");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
