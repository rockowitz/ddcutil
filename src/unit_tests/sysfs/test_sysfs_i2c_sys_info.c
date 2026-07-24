/** @file test_sysfs_i2c_sys_info.c
 *
 *  Standalone unit test for src/sysfs/sysfs_i2c_sys_info.c:
 *  get_i2c_sys_info() for a bus number that does not exist -- it checks
 *  directory_exists() on /sys/bus/i2c/devices/i2c-N before doing anything
 *  else, and returns NULL immediately if the device is not present -- and
 *  the lifecycle function's NULL safety.
 *
 *  Not exercised: the DRM connector/PCI topology walk performed for a bus
 *  number that does exist, which depends on the test host's real hardware
 *  and driver.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libsysfs unit test: it links the internal
 *  libsysfs/libi2c/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "sysfs/sysfs_i2c_sys_info.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


static void test_get_i2c_sys_info_nonexistent_bus(void) {
   I2C_Sys_Info * info = get_i2c_sys_info(9999, -1);
   CK(info == NULL);
   free_i2c_sys_info(info);   // NULL-safe
}


static void test_free_i2c_sys_info_null_safe(void) {
   free_i2c_sys_info(NULL);   // must not crash
}


int main(int argc, char ** argv) {
   test_get_i2c_sys_info_nonexistent_bus();
   test_free_i2c_sys_info_null_safe();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
