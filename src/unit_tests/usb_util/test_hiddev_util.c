/** @file test_hiddev_util.c
 *
 *  Standalone unit tests for the pure subset of src/usb_util/hiddev_util.c:
 *  hiddev_report_type_name(), and free_hid_field_locator()/
 *  report_hid_field_locator() driven with a hand-built struct
 *  hid_field_locator rather than a real open HID device.
 *
 *  Every other function in this file (hiddev_find_report(),
 *  hiddev_get_edid(), is_hiddev_monitor(), get_hiddev_device_names(),
 *  etc) either issues real ioctl() calls against an open device fd or
 *  scans the filesystem/udev for real devices, and so is out of scope
 *  for these unit tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusbutil/
 *  libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/hiddev.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"

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

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * discarding the captured output. */
#define QUIETLY(stmt) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   fclose(_tmp); \
} while(0)


static void test_hiddev_report_type_name(void) {
   CK_STR(hiddev_report_type_name(HID_REPORT_TYPE_INPUT), "HID_REPORT_TYPE_INPUT");
   CK_STR(hiddev_report_type_name(HID_REPORT_TYPE_OUTPUT), "HID_REPORT_TYPE_OUTPUT");
   CK_STR(hiddev_report_type_name(HID_REPORT_TYPE_FEATURE), "HID_REPORT_TYPE_FEATURE");
   CK_STR(hiddev_report_type_name(0), "invalid value");
   CK_STR(hiddev_report_type_name(99), "invalid value");
}


static void test_hid_field_locator_lifecycle(void) {
   struct hid_field_locator * loc = calloc(1, sizeof(struct hid_field_locator));
   loc->report_type = HID_REPORT_TYPE_FEATURE;
   loc->report_id = 2;
   loc->field_index = 0;
   loc->finfo = calloc(1, sizeof(struct hiddev_field_info));
   loc->finfo->report_type = HID_REPORT_TYPE_FEATURE;
   loc->finfo->report_id = 2;

   QUIETLY( report_hid_field_locator(loc, 0) );
   CK(true);   // reaching here without crashing is the test

   free_hid_field_locator(loc);   // frees loc->finfo and loc itself
}


static void test_free_hid_field_locator_null_safe(void) {
   free_hid_field_locator(NULL);   // must not crash
   CK(true);
}


static void test_report_hid_field_locator_null_safe(void) {
   QUIETLY( report_hid_field_locator(NULL, 0) );
   CK(true);
}


int main(int argc, char ** argv) {
   test_hiddev_report_type_name();
   test_hid_field_locator_lifecycle();
   test_free_hid_field_locator_null_safe();
   test_report_hid_field_locator_null_safe();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
