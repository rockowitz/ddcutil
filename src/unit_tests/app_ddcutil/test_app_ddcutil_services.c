/** @file test_app_ddcutil_services.c
 *
 *  Standalone smoke test for src/app_ddcutil/app_ddcutil_services.c:
 *  init_app_ddcutil_services() must run without touching any real
 *  hardware (each init_app_*() it calls only registers RTTI trace names).
 *  There is no corresponding terminate function.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappddcutil/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>

#include "app_ddcutil/app_ddcutil_services.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


int main(int argc, char ** argv) {
   init_app_ddcutil_services();
   init_app_ddcutil_services();   // safe to call again: RTTI registration is idempotent
   CK(true);   // reaching here without crashing is the test

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
