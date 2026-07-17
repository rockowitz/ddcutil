/** @file test_udev_usb_util.c
 *
 *  Standalone unit tests for src/util/udev_usb_util.c.
 *
 *  usb_hiddev_directory() returns a fixed path and is checked exactly.
 *  get_udev_usb_devinfo() queries udev; for a device name that does not exist it
 *  returns NULL, which is checked here.  The functions that read real USB
 *  devices are otherwise not exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/udev_usb_util.h"

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

int main(int argc, char ** argv) {
   CK_STR(usb_hiddev_directory(), "/dev/usb");

   // a device name that cannot match anything yields NULL
   Udev_Usb_Devinfo * info =
         get_udev_usb_devinfo("usbmisc", "nonexistent_device_zzz_999");
   CK(info == NULL);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
