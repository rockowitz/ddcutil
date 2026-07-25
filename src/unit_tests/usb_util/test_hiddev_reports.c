/** @file test_hiddev_reports.c
 *
 *  Standalone unit tests for the pure subset of src/usb_util/hiddev_reports.c:
 *  interpret_collection_type(), interpret_field_bits(),
 *  hiddev_interpret_report_id(), hiddev_interpret_usage_code(), and smoke
 *  tests of the dbgrpt_hiddev_*()/report_hiddev_*() functions, each driven
 *  with a hand-built kernel <linux/hiddev.h> struct rather than a real
 *  open HID device -- no hardware or file I/O.
 *
 *  Every other function in this file (get_hiddev_string(),
 *  report_hiddev_strings(), dbgrpt_hiddev_device_by_fd(),
 *  report_report_descriptors_for_report_type(), etc) issues real ioctl()
 *  calls against an open device fd and so is out of scope for these unit
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

#define CK_STR_CONTAINS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected to contain \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _n); } \
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


static void test_interpret_collection_type(void) {
   CK_STR(interpret_collection_type(0x00), "Physical");
   CK_STR(interpret_collection_type(0x01), "Application");
   CK_STR(interpret_collection_type(0x06), "Usage Modifier");
   CK_STR(interpret_collection_type(0x80), "Vendor-defined");
   CK_STR(interpret_collection_type(0xff), "Vendor-defined");
   CK_STR(interpret_collection_type(0x07), "Reserved");
}


static void test_interpret_field_bits(void) {
   CK_STR(interpret_field_bits(0), "");   // no bits set
   char * s = interpret_field_bits(HID_FIELD_CONSTANT | HID_FIELD_VARIABLE);
   CK_STR_CONTAINS(s, "HID_FIELD_CONSTANT");
   CK_STR_CONTAINS(s, "HID_FIELD_VARIABLE");
   // trailing separator must be stripped
   CK(s[strlen(s)-1] != '|');
}


static void test_hiddev_interpret_report_id(void) {
   CK_STR(hiddev_interpret_report_id(HID_REPORT_ID_UNKNOWN), "HID_REPORT_ID_UNKNOWN");
   CK_STR(hiddev_interpret_report_id(5), "5");
   CK_STR(hiddev_interpret_report_id(HID_REPORT_ID_FIRST | 3), "HID_REPORT_ID_FIRST|3");
   CK_STR(hiddev_interpret_report_id(HID_REPORT_ID_NEXT | 7), "HID_REPORT_ID_NEXT|7");
}


static void test_hiddev_interpret_usage_code(void) {
   // usage_code == 0: empty string, special-cased
   CK_STR(hiddev_interpret_usage_code(0), "");

   // manufacturer-specific usage page (>= 0xff00): named directly, no
   // devid_* database lookup attempted
   char * s = hiddev_interpret_usage_code((0xff01 << 16) | 0x0001);
   CK_STR_CONTAINS(s, "page=0xff01");
   CK_STR_CONTAINS(s, "Manufacturer");
}


static void test_dbgrpt_hiddev_devinfo_smoke(void) {
   struct hiddev_devinfo dinfo = {0};
   dinfo.bustype = 3;   // BUS_USB
   dinfo.busnum = 1;
   dinfo.devnum = 2;
   dinfo.ifnum = 0;
   dinfo.vendor = 0x0424;
   dinfo.product = 0x3328;
   dinfo.version = 0x0100;
   dinfo.num_applications = 1;

   QUIETLY( dbgrpt_hiddev_devinfo(&dinfo, /*lookup_names=*/false, 0) );
   CK(true);   // reaching here without crashing is the test
}


static void test_report_hiddev_collection_info_smoke(void) {
   struct hiddev_collection_info cinfo = {0};
   cinfo.index = 0;
   cinfo.type = 0x01;         // Application
   cinfo.usage = 0x00800001;  // USB Monitor / Monitor Control
   cinfo.level = 0;

   QUIETLY( report_hiddev_collection_info(&cinfo, 0) );
   CK(true);
}


static void test_report_hiddev_string_descriptor_smoke(void) {
   struct hiddev_string_descriptor desc = {0};
   desc.index = 1;
   strcpy(desc.value, "Test Monitor");

   QUIETLY( report_hiddev_string_descriptor(&desc, 0) );
   CK(true);
}


static void test_dbgrpt_hiddev_report_info_smoke(void) {
   struct hiddev_report_info rinfo = {0};
   rinfo.report_type = HID_REPORT_TYPE_FEATURE;
   rinfo.report_id = 2;
   rinfo.num_fields = 1;

   QUIETLY( dbgrpt_hiddev_report_info(&rinfo, 0) );
   CK(true);
}


static void test_dbgrpt_hiddev_field_info_smoke(void) {
   struct hiddev_field_info finfo = {0};
   finfo.report_type = HID_REPORT_TYPE_FEATURE;
   finfo.report_id = 2;
   finfo.field_index = 0;
   finfo.maxusage = 1;
   finfo.flags = HID_FIELD_VARIABLE;
   finfo.physical = 0x00820010;
   finfo.logical = 0x00820010;
   finfo.application = 0x00800001;
   finfo.logical_minimum = 0;
   finfo.logical_maximum = 255;
   finfo.physical_minimum = 0;
   finfo.physical_maximum = 255;
   finfo.unit_exponent = 0;
   finfo.unit = 0;

   QUIETLY( dbgrpt_hiddev_field_info(&finfo, 0) );
   CK(true);
}


static void test_dbgrpt_hiddev_usage_ref_smoke(void) {
   struct hiddev_usage_ref uref = {0};
   uref.report_type = HID_REPORT_TYPE_FEATURE;
   uref.report_id = 2;
   uref.field_index = 0;
   uref.usage_index = 0;
   uref.usage_code = 0x00820010;
   uref.value = 50;

   QUIETLY( dbgrpt_hiddev_usage_ref(&uref, 0) );
   CK(true);

   struct hiddev_usage_ref_multi uref_multi = {0};
   uref_multi.uref = uref;
   uref_multi.num_values = 1;
   uref_multi.values[0] = 50;

   QUIETLY( dbgrpt_hiddev_usage_ref_multi(&uref_multi, 0) );
   CK(true);
}


int main(int argc, char ** argv) {
   test_interpret_collection_type();
   test_interpret_field_bits();
   test_hiddev_interpret_report_id();
   test_hiddev_interpret_usage_code();
   test_dbgrpt_hiddev_devinfo_smoke();
   test_report_hiddev_collection_info_smoke();
   test_report_hiddev_string_descriptor_smoke();
   test_dbgrpt_hiddev_report_info_smoke();
   test_dbgrpt_hiddev_field_info_smoke();
   test_dbgrpt_hiddev_usage_ref_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
