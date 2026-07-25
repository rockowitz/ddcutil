/** @file test_libusb_util.c
 *
 *  Standalone unit tests for the pure subset of src/usb_util/libusb_util.c
 *  and src/usb_util/libusb_reports.c: make_path(), the Value_Name_Title
 *  lookup wrappers (descriptor_title()/endpoint_direction_title()/
 *  transfer_type_title()/class_code_title()), is_hub_descriptor(), and a
 *  smoke test of report_libusb_device_descriptor() driven with a
 *  hand-built struct libusb_device_descriptor (its libusb_device_handle
 *  parameter is documented as "may be null").
 *
 *  Per libusb_reports.h's own file comment, "libusb is not currently used
 *  by ddcutil. This code is retained for reference" -- ddcutil uses the
 *  hiddev/hidraw interfaces instead. Everything else in these 2 files
 *  opens and communicates with a real USB device via libusb (device
 *  enumeration, control transfers, endpoint/interface/configuration
 *  descriptor reports that walk a live libusb_device's nested
 *  descriptors), and so is out of scope for these unit tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusbutil/
 *  libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <libusb-1.0/libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usb_util/libusb_reports.h"
#include "usb_util/libusb_util.h"

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


static void test_make_path(void) {
   char * p = make_path(1, 2, 0);
   CK(p != NULL);
   CK_STR(p, "0001:0002:00");
   free(p);

   char * p2 = make_path(0x0424, 0x3328, 0x0a);
   CK_STR(p2, "0424:3328:0a");
   free(p2);
}


static void test_descriptor_title(void) {
   CK_STR(descriptor_title(LIBUSB_DT_DEVICE), "Device");
   CK_STR(descriptor_title(LIBUSB_DT_HID), "HID");
   CK(descriptor_title(0xee) == NULL);   // unassigned value
}


static void test_endpoint_direction_title(void) {
   CK_STR(endpoint_direction_title(LIBUSB_ENDPOINT_IN), "IN");
   CK_STR(endpoint_direction_title(LIBUSB_ENDPOINT_OUT), "OUT");
}


static void test_transfer_type_title(void) {
   CK_STR(transfer_type_title(LIBUSB_TRANSFER_TYPE_CONTROL), "Control");
   CK_STR(transfer_type_title(LIBUSB_TRANSFER_TYPE_INTERRUPT), "Interrupt");
}


static void test_class_code_title(void) {
   CK_STR(class_code_title(LIBUSB_CLASS_HID), "Human Interface Device");
   CK_STR(class_code_title(LIBUSB_CLASS_HUB), "Hub");
}


static void test_is_hub_descriptor(void) {
   struct libusb_device_descriptor desc = {0};
   desc.bDeviceClass = LIBUSB_CLASS_HUB;
   CK(is_hub_descriptor(&desc));

   desc.bDeviceClass = LIBUSB_CLASS_HID;
   CK(!is_hub_descriptor(&desc));
}


static void test_report_libusb_device_descriptor_smoke(void) {
   struct libusb_device_descriptor desc = {0};
   desc.bLength = 18;
   desc.bDescriptorType = LIBUSB_DT_DEVICE;
   desc.bcdUSB = 0x0200;
   desc.bDeviceClass = LIBUSB_CLASS_PER_INTERFACE;
   desc.idVendor = 0x0424;
   desc.idProduct = 0x3328;
   desc.bcdDevice = 0x0100;
   desc.bNumConfigurations = 1;

   // dh (libusb_device_handle*) is documented as "may be null"
   QUIETLY( report_libusb_device_descriptor(&desc, NULL, 0) );
   CK(true);   // reaching here without crashing is the test
}


int main(int argc, char ** argv) {
   test_make_path();
   test_descriptor_title();
   test_endpoint_direction_title();
   test_transfer_type_title();
   test_class_code_title();
   test_is_hub_descriptor();
   test_report_libusb_device_descriptor_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
