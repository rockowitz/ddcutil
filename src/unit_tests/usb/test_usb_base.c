/** @file test_usb_base.c
 *
 *  Standalone unit tests for the pure subset of src/usb/usb_base.c: the
 *  module-level ignore lists (usb_ignore_hiddevs()/usb_is_ignored_hiddev(),
 *  usb_ignore_vid_pid_values()/usb_is_ignored_vid_pid()/
 *  usb_is_ignored_vid_pid_value()), plus error-path exercises of
 *  usb_open_hiddev_device()/usb_close_device() that never touch a real
 *  device (a nonexistent path, and an already-closed/invalid fd).
 *
 *  Every hiddev_get_*() ioctl wrapper requires a real open hiddev fd and
 *  so is out of scope for these unit tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusb/libbase/
 *  libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/core.h"
#include "util/data_structures.h"

#include "usb/usb_base.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


static void test_ignore_hiddevs(void) {
   CK(!usb_is_ignored_hiddev(3));

   Bit_Set_32 ignored = bs32_insert(EMPTY_BIT_SET_32, 3);
   ignored = bs32_insert(ignored, 7);
   usb_ignore_hiddevs(ignored);

   CK(usb_is_ignored_hiddev(3));
   CK(usb_is_ignored_hiddev(7));
   CK(!usb_is_ignored_hiddev(4));

   usb_ignore_hiddevs(EMPTY_BIT_SET_32);   // restore for other tests in this process
}


static void test_ignore_vid_pid_values(void) {
   CK(!usb_is_ignored_vid_pid(0x0424, 0x3328));
   CK(!usb_is_ignored_vid_pid_value(VID_PID_VALUE(0x0424, 0x3328)));

   Vid_Pid_Value ignored[2] = {
      VID_PID_VALUE(0x0424, 0x3328),
      VID_PID_VALUE(0x056d, 0x0002),
   };
   usb_ignore_vid_pid_values(2, ignored);

   CK(usb_is_ignored_vid_pid(0x0424, 0x3328));
   CK(usb_is_ignored_vid_pid(0x056d, 0x0002));
   CK(!usb_is_ignored_vid_pid(0x1234, 0x5678));
   CK(usb_is_ignored_vid_pid_value(VID_PID_VALUE(0x056d, 0x0002)));
}


static void test_vid_pid_value_macros(void) {
   Vid_Pid_Value v = VID_PID_VALUE(0x0424, 0x3328);
   CK(VID_PID_VALUE_TO_VID(v) == 0x0424);
   CK(VID_PID_VALUE_TO_PID(v) == 0x3328);
}


static void test_usb_open_hiddev_device_nonexistent(void) {
   int rc = usb_open_hiddev_device("/nonexistent/hiddev/path/for/testing", CALLOPT_NONE);
   CK(rc < 0);
   CK(-rc == ENOENT);
}


static void test_usb_close_device_bad_fd(void) {
   Status_Errno rc = usb_close_device(-1, "test-device", CALLOPT_NONE);
   CK(rc < 0);
   CK(-rc == EBADF);
}


int main(int argc, char ** argv) {
   test_ignore_hiddevs();
   test_ignore_vid_pid_values();
   test_vid_pid_value_macros();
   test_usb_open_hiddev_device_nonexistent();
   test_usb_close_device_bad_fd();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
