/** @file test_build_info.c
 *
 *  Standalone unit tests for src/base/build_info.c: the ddcutil version string
 *  accessors.  The exact version varies, so structural properties are checked.
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

#include "base/build_info.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

int main(int argc, char ** argv) {
   const char * base = get_base_ddcutil_version();
   const char * suffix = get_ddcutil_version_suffix();
   const char * full = get_full_ddcutil_version();

   CK(base != NULL && strlen(base) > 0);
   CK(suffix != NULL);                       // may be empty
   CK(full != NULL && strlen(full) > 0);

   // the full version begins with the base version
   CK(strncmp(full, base, strlen(base)) == 0);

   // if there is a suffix, the full version contains it
   if (strlen(suffix) > 0)
      CK(strstr(full, suffix) != NULL);

   // the base version looks like a dotted version number
   CK(base[0] >= '0' && base[0] <= '9');
   CK(strchr(base, '.') != NULL);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
