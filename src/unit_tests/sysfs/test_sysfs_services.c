/** @file test_sysfs_services.c
 *
 *  Standalone smoke test for src/sysfs/sysfs_services.c:
 *  init_sysfs_services()/terminate_sysfs_services() must run without
 *  crashing (they only register RTTI trace names and free the cached
 *  Sysfs_I2C_Info array, which is fine to free while empty).
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libsysfs unit test: it links the internal
 *  libsysfs/libi2c/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>

#include "sysfs/sysfs_services.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


int main(int argc, char ** argv) {
   init_sysfs_services();
   terminate_sysfs_services();
   terminate_sysfs_services();   // safe to call again: nothing left to free
   CK(true);   // reaching here without crashing is the test

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
