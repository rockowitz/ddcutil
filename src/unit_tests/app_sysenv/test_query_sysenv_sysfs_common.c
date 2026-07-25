/** @file test_query_sysenv_sysfs_common.c
 *
 *  Standalone unit test for src/app_sysenv/query_sysenv_sysfs_common.c:
 *  h2ushort() and h2uint(), simple hex-string-to-integer parsers used
 *  throughout the sysfs scanning code. Pure string parsing, no I/O.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappsysenv/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>

#include "app_sysenv/query_sysenv_sysfs_common.h"

static int total = 0;
static int failed = 0;

#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)


static void test_h2ushort(void) {
   char buf1[] = "0x1a2b";
   CK_INT(h2ushort(buf1), 0x1a2b);

   char buf2[] = "1a2b";
   CK_INT(h2ushort(buf2), 0x1a2b);

   char buf3[] = "0000";
   CK_INT(h2ushort(buf3), 0);
}


static void test_h2uint(void) {
   char buf1[] = "0x0424abcd";
   CK_INT(h2uint(buf1), 0x0424abcdU);

   char buf2[] = "deadbeef";
   CK_INT(h2uint(buf2), 0xdeadbeefU);
}


int main(int argc, char ** argv) {
   test_h2ushort();
   test_h2uint();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
