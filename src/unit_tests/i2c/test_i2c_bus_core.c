/** @file test_i2c_bus_core.c
 *
 *  Standalone unit tests for src/i2c/i2c_bus_core.c, restricted to behavior
 *  that does not require a real /dev/i2c device: i2c_edid_exists()'s fast
 *  exit for a nonexistent bus, is_valid_drm_connector_name() for a
 *  connector name that cannot exist, and i2c_detect_x37_new() over every
 *  combination of the switches that affect it.  Functions that inspect a real,
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
#include <unistd.h>

#include "util/error_info.h"
#include "util/timestamp.h"

#include "base/execution_stats.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_x37.h"
#include "i2c/i2c_execute.h"               // i2c_forceable_slave_addr_flag
#include "i2c/i2c_strategy_dispatcher.h"   // io strategy selection
#include "i2c/i2c_bus_open_close.h"
#include "i2c/i2c_edid.h"
#include "base/i2c_bus_aux.h"                // i2c_device_exists
#include "base/display_lock.h"               // init_i2c_display_lock
#include "base/execution_stats.h"            // init_execution_stats
#include "util/data_structures.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "sysfs/sysfs_simple.h"              // sysfs_is_ignorable_i2c_device
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
#define RUN(f) do { printf("-- %s()\n", #f); f(); } while(0)

/* Runs `stmt` with stdout and stderr redirected, discarding the output.  Needed
 * because i2c_detect_x37_new() traces unconditionally. */
#define QUIETLY(stmt) do { \
   fflush(stdout); fflush(stderr); \
   int _so = dup(fileno(stdout)); int _se = dup(fileno(stderr)); \
   FILE * _tmp = tmpfile(); \
   bool _redirected = (_so >= 0 && _se >= 0 && _tmp); \
   if (_redirected) { dup2(fileno(_tmp), fileno(stdout)); dup2(fileno(_tmp), fileno(stderr)); } \
   stmt; \
   fflush(stdout); fflush(stderr); \
   if (_redirected) { dup2(_so, fileno(stdout)); dup2(_se, fileno(stderr)); } \
   if (_so >= 0) close(_so); \
   if (_se >= 0) close(_se); \
   if (_tmp) fclose(_tmp); \
} while(0)

#define NONEXISTENT_BUSNO 9999


/* Finds an I2C bus that ddcutil itself would probe and that has a monitor on
 * it, and leaves it open.  Returns the bus number, or -1 if none was found, in
 * which case the hardware tests are skipped rather than failed -- these tests
 * must pass on a machine with no monitor, and on one where /dev/i2c is not
 * readable by the user running them.
 *
 * Bus selection uses sysfs_is_ignorable_i2c_device(), the same guard ddcutil
 * uses, so the SMBus and AMDGPU SMU buses are never opened.  Probing those is
 * what hangs some cards.
 */
static int find_bus_with_monitor(int * fd_loc, Byte * edid_out) {
   *fd_loc = -1;
   for (int busno = 0; busno < 32; busno++) {
      if (!i2c_device_exists(busno))
         continue;
      if (sysfs_is_ignorable_i2c_device(busno))
         continue;
      int fd = -1;
      Error_Info * err = NULL;
      QUIETLY( err = i2c_open_bus(busno, CALLOPT_WAIT, &fd) );
      if (err) {
         errinfo_free(err);
         continue;
      }
      Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
      Status_Errno_DDC rc;
      QUIETLY( rc = i2c_get_raw_edid_by_fd(fd, buf) );
      if (rc == 0 && buf->len >= 128) {
         memcpy(edid_out, buf->bytes, 128);
         buffer_free(buf, NULL);
         *fd_loc = fd;
         return busno;
      }
      buffer_free(buf, NULL);
      QUIETLY( i2c_close_bus(busno, fd, CALLOPT_NONE) );
   }
   return -1;
}

/** i2c_detect_x37_new() over the same combinations, against a real bus with a
 *  monitor on it.
 *
 *  With a working file descriptor the property worth asserting is agreement: the
 *  switch settings select how the bytes are moved, not what the monitor answers,
 *  so every combination must reach the same conclusion about x37.  A combination
 *  that disagreed would mean the answer depends on the transport, which is
 *  exactly the kind of thing that makes x37 detection look intermittent.
 */
static void test_detect_x37_new_all_switches_real_bus(void) {
   Byte edid[128];
   int fd = -1;
   int busno = find_bus_with_monitor(&fd, edid);
   if (busno < 0) {
      printf("   no usable bus with a monitor found; hardware sweep skipped\n");
      return;
   }
   printf("   using /dev/i2c-%d\n", busno);

   I2C_IO_Strategy_Id saved_strategy = i2c_get_io_strategy_id();
   bool saved_forceable = i2c_forceable_slave_addr_flag;

   I2C_IO_Strategy_Id strategies[] = {I2C_IO_STRATEGY_FILEIO, I2C_IO_STRATEGY_IOCTL};
   bool forceable_values[] = {false, true};
   char * drivers[] = {NULL, "i915", "nvidia"};

   Status_Errno_DDC first = 0;
   bool have_first = false;
   int disagreed = 0, combinations = 0;
   for (int si = 0; si < 2; si++)
      for (int fi = 0; fi < 2; fi++)
         for (int di = 0; di < 3; di++) {
            i2c_set_io_strategy_by_id(strategies[si]);
            i2c_forceable_slave_addr_flag = forceable_values[fi];

            Status_Errno_DDC rc;
            uint64_t t0 = cur_realtime_nanosec();
            QUIETLY( rc = i2c_detect_x37(fd, drivers[di]) );
            uint64_t elapsed_us = NANOS2MICROS(cur_realtime_nanosec() - t0);

            if (!have_first) { first = rc; have_first = true; }
            bool agrees = (rc == first);
            if (!agrees) disagreed++;
            printf("   %-22s forceable=%d driver=%-7s %7lu us  rc=%d%s\n",
                   i2c_io_strategy_id_name(strategies[si]), forceable_values[fi],
                   drivers[di] ? drivers[di] : "(null)",
                   (unsigned long) elapsed_us, rc,
                   agrees ? "" : "  DISAGREES WITH FIRST");
            combinations++;
         }

   printf("   x37 result %d for all %d combinations, %d disagreed\n",
          first, combinations, disagreed);
   CK_INT(combinations, 2*2*3);
   CK_INT(disagreed, 0);

   i2c_set_io_strategy_by_id((saved_strategy == I2C_IO_STRATEGY_NOT_SET)
                                ? DEFAULT_I2C_IO_STRATEGY : saved_strategy);
   i2c_forceable_slave_addr_flag = saved_forceable;
   QUIETLY( i2c_close_bus(busno, fd, CALLOPT_NONE) );
}


/** i2c_detect_x37_new() over every combination of the switches that reach it.
 *
 *  It issues a one byte read at slave address x37 and, if that fails, a zero
 *  length write, both through the io strategy dispatcher.  The switches are
 *  therefore the active io strategy, and i2c_forceable_slave_addr_flag, which
 *  selects I2C_SLAVE_FORCE over I2C_SLAVE in the fileio path.
 *
 *  What must hold for every combination, with a file descriptor that cannot
 *  work: a negative status.  Zero would report x37 responsive on a bus that was
 *  never reachable, and i2c_check_bus() would set I2C_BUS_ADDR_X37 on it.  The
 *  zero length write is the reason this is worth asserting rather than assuming
 *  -- a write of no bytes is the kind of call an implementation can report as
 *  trivially successful.
 *
 *  The driver argument is varied too, including NULL, because it reaches a
 *  trace format string.
 */
static void test_detect_x37_new_all_switches(void) {
   I2C_IO_Strategy_Id saved_strategy = i2c_get_io_strategy_id();
   bool saved_forceable = i2c_forceable_slave_addr_flag;

   I2C_IO_Strategy_Id strategies[] = {I2C_IO_STRATEGY_FILEIO, I2C_IO_STRATEGY_IOCTL};
   bool forceable_values[] = {false, true};
   char * drivers[] = {NULL, "i915", "nvidia"};

   int combinations = 0;
   for (int si = 0; si < 2; si++) {
      for (int fi = 0; fi < 2; fi++) {
         for (int di = 0; di < 3; di++) {
            i2c_set_io_strategy_by_id(strategies[si]);
            i2c_forceable_slave_addr_flag = forceable_values[fi];

            Status_Errno_DDC rc;
            QUIETLY( rc = i2c_detect_x37(-1, drivers[di]) );
            if (rc >= 0)
               printf("FAIL  strategy=%s forceable=%s driver=%s -> %d, expected < 0\n",
                      i2c_io_strategy_id_name(strategies[si]),
                      forceable_values[fi] ? "true" : "false",
                      drivers[di] ? drivers[di] : "(null)", rc);
            CK(rc < 0);
            combinations++;
         }
      }
   }
   CK_INT(combinations, 2*2*3);

   // No module initialization runs in this test, so the strategy may have been
   // unset on entry; i2c_set_io_strategy_by_id() asserts against NOT_SET.
   i2c_set_io_strategy_by_id((saved_strategy == I2C_IO_STRATEGY_NOT_SET)
                                ? DEFAULT_I2C_IO_STRATEGY : saved_strategy);
   i2c_forceable_slave_addr_flag = saved_forceable;
}


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

   RUN(test_is_valid_drm_connector_name);
   RUN(test_i2c_edid_exists);
   RUN(test_detect_x37_new_all_switches);
   // i2c_open_bus()/i2c_close_bus() use the display lock table
   init_execution_stats();
   init_i2c_display_lock();
   RUN(test_detect_x37_new_all_switches_real_bus);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
