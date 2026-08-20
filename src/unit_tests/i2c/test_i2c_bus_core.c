/** @file test_i2c_bus_core.c
 *
 *  Standalone unit tests for src/i2c/i2c_bus_core.c, restricted to behavior
 *  that does not require a real /dev/i2c device: the open/close functions'
 *  handling of a bus number that does not exist (-ENOENT, no retry) and of
 *  an already-invalid file descriptor (-EBADF), i2c_edid_exists()'s fast
 *  exit for a nonexistent bus, and is_valid_drm_connector_name() for a
 *  connector name that cannot exist.  Functions that inspect a real,
 *  present bus (i2c_check_bus(), i2c_get_and_check_bus_info(),
 *  i2c_check_open_bus_alive(), i2c_report_active_bus()) are not exercised.
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

#include "base/display_lock.h"
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


static void test_simple_rw_test(void) {
   Error_Info * err = simple_rw_test(NONEXISTENT_BUSNO);
   CK(err != NULL);
   if (err) {
      CK_INT(err->status_code, -ENOENT);
      errinfo_free(err);
   }
}


static void test_open_bus_basic_nonexistent(void) {
   char filename[32];
   g_snprintf(filename, sizeof(filename), "/dev/i2c-%d", NONEXISTENT_BUSNO);

   int fd = -2;   // sentinel, should be set to -1 on failure
   Error_Info * err = i2c_open_bus_basic(filename, 0, &fd);
   CK(err != NULL);
   CK_INT(fd, -1);
   if (err) {
      CK_INT(err->status_code, -ENOENT);
      errinfo_free(err);
   }
}


static void test_open_bus_basic_by_busno_nonexistent(void) {
   int fd = -2;
   Error_Info * err = i2c_open_bus_basic_by_busno(NONEXISTENT_BUSNO, 0, &fd);
   CK(err != NULL);
   CK_INT(fd, -1);
   if (err) {
      CK_INT(err->status_code, -ENOENT);
      errinfo_free(err);
   }
}


static void test_open_bus_nonexistent(void) {
   int fd = -2;
   Error_Info * err = i2c_open_bus(NONEXISTENT_BUSNO, 0, &fd);
   CK(err != NULL);
   CK_INT(fd, -1);
   if (err)
      errinfo_free(err);
}


static void test_close_bus_bad_fd(void) {
   // close(-1) always fails with EBADF; no real device is touched.
   Status_Errno rc = i2c_close_bus_basic(NONEXISTENT_BUSNO, -1, 0);
   CK_INT(rc, -EBADF);

   rc = i2c_close_bus(NONEXISTENT_BUSNO, -1, 0);
   CK_INT(rc, -EBADF);
}


int main(int argc, char ** argv) {
   // i2c_open_bus()/i2c_close_bus() use the display-lock table, which must
   // be initialized before first use (see src/unit_tests/base/test_display_lock.c).
   init_execution_stats();
   init_i2c_display_lock();

   test_is_valid_drm_connector_name();
   test_i2c_edid_exists();
   test_simple_rw_test();
   test_open_bus_basic_nonexistent();
   test_open_bus_basic_by_busno_nonexistent();
   test_open_bus_nonexistent();
   test_close_bus_bad_fd();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
