/** @file test_i2c_strategy_dispatcher.c
 *
 *  Standalone unit tests for src/i2c/i2c_strategy_dispatcher.c: the strategy
 *  id/name lookup, the set/get strategy round trip, the fast-fail paths of
 *  is_nvidia_einval_bug() that don't require reading real sysfs driver
 *  info, and the defensive behavior of invoke_i2c_writer()/
 *  invoke_i2c_reader() on an invalid file descriptor for both strategies.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "base/parms.h"

#include "i2c/i2c_strategy_dispatcher.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)


// Runs stmt with stdout and stderr redirected to a temporary file.  The calls
// wrapped below deliberately fail -- a bad file descriptor, a nonexistent bus,
// a forced failure hook -- and the code under test reports those failures to
// the terminal, as it should.  Without this the log of a passing test run
// fills with alarming messages, and a real error has nothing to stand out
// against.  Modeled on the QUIETLY macro in the usb_util tests, extended to
// cover stderr, which is where most of these particular messages go.
// If the redirection cannot be set up, the statement still runs, noisily.
#define QUIETLY(stmt) do { \
   fflush(stdout); fflush(stderr); \
   int _saved_out = dup(fileno(stdout)); \
   int _saved_err = dup(fileno(stderr)); \
   FILE * _tmp = tmpfile(); \
   bool _redirected = (_tmp && _saved_out >= 0 && _saved_err >= 0); \
   if (_redirected) { \
      dup2(fileno(_tmp), fileno(stdout)); \
      dup2(fileno(_tmp), fileno(stderr)); \
   } \
   stmt; \
   fflush(stdout); fflush(stderr); \
   if (_redirected) { \
      dup2(_saved_out, fileno(stdout)); \
      dup2(_saved_err, fileno(stderr)); \
   } \
   if (_saved_out >= 0) close(_saved_out); \
   if (_saved_err >= 0) close(_saved_err); \
   if (_tmp) fclose(_tmp); \
} while(0)


static void test_strategy_id_name(void) {
   CK_STR(i2c_io_strategy_id_name(I2C_IO_STRATEGY_NOT_SET), "I2C_IO_STRATEGY_NOT_SET");
   CK_STR(i2c_io_strategy_id_name(I2C_IO_STRATEGY_FILEIO),  "I2C_IO_STRATEGY_FILEIO");
   CK_STR(i2c_io_strategy_id_name(I2C_IO_STRATEGY_IOCTL),   "I2C_IO_STRATEGY_IOCTL");
}


static void test_set_get_strategy_round_trip(void) {
   i2c_set_io_strategy_by_id(I2C_IO_STRATEGY_FILEIO);
   CK(i2c_get_io_strategy_id() == I2C_IO_STRATEGY_FILEIO);

   i2c_set_io_strategy_by_id(I2C_IO_STRATEGY_IOCTL);
   CK(i2c_get_io_strategy_id() == I2C_IO_STRATEGY_IOCTL);

   i2c_set_io_strategy_by_id(DEFAULT_I2C_IO_STRATEGY);   // restore
}


// Only the fast-fail combinations are tested: is_nvidia_einval_bug() short
// circuits (without touching sysfs) unless rc == -EINVAL AND the strategy
// is I2C_IO_STRATEGY_IOCTL.  The true-result path requires a real nvidia
// i2c-dev driver bound to a busno and is not exercised.
static void test_is_nvidia_einval_bug_fast_fail(void) {
   I2C_IO_Strategy_Id saved = i2c_get_io_strategy_id();

   CK(is_nvidia_einval_bug(I2C_IO_STRATEGY_FILEIO, 9999, -EINVAL) == false);
   CK(is_nvidia_einval_bug(I2C_IO_STRATEGY_IOCTL,  9999, -EBADF)  == false);
   CK(is_nvidia_einval_bug(I2C_IO_STRATEGY_IOCTL,  9999, 0)       == false);

   // strategy must be unchanged: none of the above should have triggered
   // the "force I2C_IO_STRATEGY_FILEIO" side effect
   CK(i2c_get_io_strategy_id() == saved);
}


static void test_invoke_writer_reader_bad_fd(void) {
   Byte data[4]    = {0, 1, 2, 3};
   Byte readbuf[4] = {0};

   I2C_IO_Strategy_Id strategies[] = { I2C_IO_STRATEGY_IOCTL, I2C_IO_STRATEGY_FILEIO };
   for (int s = 0; s < 2; s++) {
      i2c_set_io_strategy_by_id(strategies[s]);

      Status_Errno_DDC rc;
      QUIETLY( rc = invoke_i2c_writer(-1, 0x50, 4, data) );
      CK(rc < 0);

      QUIETLY( rc = invoke_i2c_reader(-1, 0x50, false, 4, readbuf) );
      CK(rc < 0);
   }

   i2c_set_io_strategy_by_id(DEFAULT_I2C_IO_STRATEGY);   // restore
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   i2c_set_io_strategy_by_id(DEFAULT_I2C_IO_STRATEGY);   // required: asserted non-NOT_SET

   test_strategy_id_name();
   test_set_get_strategy_round_trip();
   test_is_nvidia_einval_bug_fast_fail();
   test_invoke_writer_reader_bad_fd();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
