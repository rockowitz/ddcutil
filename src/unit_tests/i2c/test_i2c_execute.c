/** @file test_i2c_execute.c
 *
 *  Standalone unit tests for src/i2c/i2c_execute.c: the
 *  set_i2c_fileio_use_timeout()/get_i2c_fileio_use_timeout() global toggle,
 *  and the defensive behavior of the read/write/set-address functions on an
 *  invalid file descriptor.  Actual I2C bus transfers require a real
 *  /dev/i2c device and are not exercised.
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

#include "i2c/i2c_execute.h"

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


static void test_fileio_use_timeout_toggle(void) {
   set_i2c_fileio_use_timeout(true);
   CK(get_i2c_fileio_use_timeout() == true);
   set_i2c_fileio_use_timeout(false);
   CK(get_i2c_fileio_use_timeout() == false);
}


static void test_set_addr_bad_fd(void) {
   CK(i2c_forceable_slave_addr_flag == false);   // default

   Status_Errno rc = i2c_set_addr(-1, 0x50);
   CK_INT(rc, -EBADF);

   // retry-on-EBUSY path must not be taken for EBADF, and behavior must be
   // the same regardless of the flag
   i2c_forceable_slave_addr_flag = true;
   rc = i2c_set_addr(-1, 0x50);
   CK_INT(rc, -EBADF);
   i2c_forceable_slave_addr_flag = false;
}


static void test_fileio_writer_reader_bad_fd(void) {
   Byte data[4] = {0, 1, 2, 3};
   Status_Errno_DDC rc = i2c_fileio_writer(-1, 0x50, 4, data);
   CK(rc < 0);

   Byte readbuf[4] = {0};
   rc = i2c_fileio_reader(-1, 0x50, false, 4, readbuf);
   CK(rc < 0);

   rc = i2c_fileio_reader(-1, 0x50, true, 4, readbuf);   // bytewise path
   CK(rc < 0);
}


static void test_ioctl_writer_reader_bad_fd(void) {
   Byte data[4] = {0, 1, 2, 3};
   Status_Errno_DDC rc = i2c_ioctl_writer(-1, 0x50, 4, data);
   CK(rc < 0);

   Byte readbuf[4] = {0};
   rc = i2c_ioctl_reader(-1, 0x50, false, 4, readbuf);
   CK(rc < 0);

   rc = i2c_ioctl_reader(-1, 0x50, true, 4, readbuf);   // bytewise path
   CK(rc < 0);
}


int main(int argc, char ** argv) {
   test_fileio_use_timeout_toggle();
   test_set_addr_bad_fd();
   test_fileio_writer_reader_bad_fd();
   test_ioctl_writer_reader_bad_fd();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
