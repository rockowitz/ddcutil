/** @file test_hidraw_util.c
 *
 *  Standalone unit test for the pure subset of src/usb_util/hidraw_util.c:
 *  bus_str(), a simple bus-type-to-name lookup.
 *
 *  Every other function in this file (probe_hidraw_device(),
 *  hidraw_is_monitor_device(), get_hidraw_device_names_using_filesys(),
 *  etc) opens and issues ioctl()s against real /dev/hidraw* devices or
 *  scans the filesystem for them, and so is out of scope for these unit
 *  tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusbutil/
 *  libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usb_util/hidraw_util.h"

static int total = 0;
static int failed = 0;

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)


static void test_bus_str(void) {
   CK_STR(bus_str(BUS_USB), "USB");
   CK_STR(bus_str(BUS_HIL), "HIL");
   CK_STR(bus_str(BUS_BLUETOOTH), "Bluetooth");
   CK_STR(bus_str(BUS_VIRTUAL), "Virtual");
   CK_STR(bus_str(-1), "Other");
}


int main(int argc, char ** argv) {
   test_bus_str();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
