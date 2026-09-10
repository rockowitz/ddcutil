/** @file test_i2c_bus_core.c
 *
 *  Standalone unit tests for src/i2c/i2c_bus_core.c, restricted to behavior
 *  that does not require a real /dev/i2c device: i2c_edid_exists()'s fast
 *  exit for a nonexistent bus, and is_valid_drm_connector_name() for a
 *  connector name that cannot exist.  Functions that inspect a real,
 *  present bus (i2c_check_bus(), i2c_get_and_check_bus_info(),
 *  i2c_check_open_bus_alive(), i2c_report_active_bus()) are not exercised.
 *
 *  The open and close functions moved to i2c_bus_open_close.c; their checks
 *  are in test_i2c_bus_open_close.c.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libi2c unit test: it links the internal libi2c/libbase/libutil
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

#include "util/error_info.h"

#include "base/execution_stats.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_bus_sysfs.h"

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

// A bus number assumed not to exist on any test host.
#define NONEXISTENT_BUSNO 9999


static void test_is_valid_drm_connector_name(void) {
   CK(!is_valid_drm_connector_name("definitely-not-a-real-connector-xyz123"));
}


static void test_i2c_edid_exists(void) {
   bool eacces = false;
   CK(!i2c_edid_exists(NONEXISTENT_BUSNO, &eacces));
   CK(!eacces);   // bus doesn't exist, so no open was even attempted
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   init_execution_stats();

   test_is_valid_drm_connector_name();
   test_i2c_edid_exists();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
