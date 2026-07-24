/** @file test_sysfs_i2c_info.c
 *
 *  Standalone unit tests for src/sysfs/sysfs_i2c_info.c:
 *  get_i2c_driver_info()/get_basic_i2c_driver_info() for a bus number that
 *  does not exist, the lifecycle function, and smoke tests of the
 *  whole-system scan functions (get_all_sysfs_i2c_info(),
 *  get_possible_ddc_ci_bus_numbers_using_sysfs_i2c_info()) -- these scan
 *  whatever /sys/bus/i2c/devices actually contains on the test host, so
 *  only "does not crash, returns a valid result" is checked.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libsysfs unit test: it links the internal
 *  libsysfs/libi2c/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "sysfs/sysfs_i2c_info.h"

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

#define NONEXISTENT_BUSNO 9999


static void test_get_i2c_driver_info_nonexistent_bus(void) {
   Sysfs_I2C_Info * info = get_i2c_driver_info(NONEXISTENT_BUSNO, -1);
   CK(info != NULL);
   if (info) {
      CK_INT(info->busno, NONEXISTENT_BUSNO);
      CK(info->name == NULL);
      CK(info->adapter_path == NULL);
      free_sysfs_i2c_info(info);
   }

   info = get_basic_i2c_driver_info(NONEXISTENT_BUSNO);
   CK(info != NULL);
   if (info) {
      CK(info->adapter_path == NULL);
      free_sysfs_i2c_info(info);
   }
}


static void test_free_sysfs_i2c_info_null_safe(void) {
   free_sysfs_i2c_info(NULL);   // must not crash
}


static void test_whole_system_scan_smoke(void) {
   GPtrArray * all = get_all_sysfs_i2c_info(true, -1);
   CK(all != NULL);   // caller must not free; owned by the module

   Bit_Set_256 buses = get_possible_ddc_ci_bus_numbers_using_sysfs_i2c_info();
   CK(bs256_count(buses) >= 0);   // trivially true; exercises the call path

   terminate_i2c_sysfs_i2c_info();
}


int main(int argc, char ** argv) {
   test_get_i2c_driver_info_nonexistent_bus();
   test_free_sysfs_i2c_info_null_safe();
   test_whole_system_scan_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
