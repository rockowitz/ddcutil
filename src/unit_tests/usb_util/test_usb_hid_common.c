/** @file test_usb_hid_common.c
 *
 *  Standalone unit tests for src/usb_util/usb_hid_common.c:
 *  collection_type_name(), force_hid_monitor_by_vid_pid(), and
 *  deny_hid_monitor_by_vid_pid(). All pure table lookups -- no hardware
 *  or file I/O.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusbutil/
 *  libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usb_util/usb_hid_common.h"

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


static void test_collection_type_name(void) {
   CK_STR(collection_type_name(0), "Physical");
   CK_STR(collection_type_name(1), "Application");
   CK_STR(collection_type_name(6), "Usage_Modifier");
   CK_STR(collection_type_name(0x80), "Vendor defined");
   CK_STR(collection_type_name(0xff), "Vendor defined");
   CK_STR(collection_type_name(7), "Reserved for future use.");
   CK_STR(collection_type_name(0x7f), "Reserved for future use.");
}


static void test_force_hid_monitor_by_vid_pid(void) {
   // Eizo HID Monitor Controls, from the exceptions table
   CK(force_hid_monitor_by_vid_pid(0x056d, 0x0002));
   // right vid, wrong pid
   CK(!force_hid_monitor_by_vid_pid(0x056d, 0x0003));
   // wrong vid entirely
   CK(!force_hid_monitor_by_vid_pid(0x1234, 0x5678));
}


static void test_deny_hid_monitor_by_vid_pid(void) {
   // ThinkPad USB Keyboard with TrackPoint, the sole entry in this table
   CK(deny_hid_monitor_by_vid_pid(0x17ef, 0x6009));
   CK(!deny_hid_monitor_by_vid_pid(0x17ef, 0x6010));
   CK(!deny_hid_monitor_by_vid_pid(0x0000, 0x0000));
}


int main(int argc, char ** argv) {
   test_collection_type_name();
   test_force_hid_monitor_by_vid_pid();
   test_deny_hid_monitor_by_vid_pid();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
