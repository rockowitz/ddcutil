/** @file test_usb_services.c
 *
 *  Standalone smoke test for src/usb/usb_services.c: init_usb_services()
 *  (which calls init_usb_base()/init_usb_displays()/init_usb_edid(), each
 *  of which only registers RTTI trace names) and terminate_usb_services()
 *  must run without touching any real hardware.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusb/libbase/
 *  libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>

#include "usb/usb_services.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


int main(int argc, char ** argv) {
   init_usb_services();
   terminate_usb_services();
   terminate_usb_services();   // safe to call again: no hiddevs were ever opened
   CK(true);   // reaching here without crashing is the test

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
