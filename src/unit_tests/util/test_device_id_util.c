/** @file test_device_id_util.c
 *
 *  Standalone unit tests for src/util/device_id_util.c.
 *
 *  Most lookups resolve against the system pci.ids / usb.ids databases, whose
 *  presence and exact contents vary by host.  The checks that do not depend on
 *  those files -- the ENUM_n special case of devid_usage_code_id_name() and the
 *  vendor-defined usage-page range -- are always run.  The database-backed
 *  lookups (well known, highly stable ids) are run only when the databases are
 *  present, detected by probing a known vendor id; otherwise they are skipped
 *  with a printed note rather than failing.
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

#include "util/device_id_util.h"

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

#define CK_HAS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  \"%s\" does not contain \"%s\"\n", __LINE__, \
             _a ? _a : "(null)", _n); } \
} while(0)

int main(int argc, char ** argv) {
   devid_ensure_initialized();

   // --- file-independent behavior ---

   // usage page 0x81 is a special case: id name is "ENUM_<n>"
   CK_STR(devid_usage_code_id_name(0x81, 5), "ENUM_5");
   CK_STR(devid_usage_code_id_name(0x81, 0), "ENUM_0");
   // extended id packs page<<16 | id
   CK_STR(devid_usage_code_name_by_extended_id(0x00810007), "ENUM_7");

   // usage page codes above 0xff00 are vendor defined
   CK_STR(devid_usage_code_page_name(0xff01), "Vendor-defined");
   CK_STR(devid_usage_code_page_name(0xffff), "Vendor-defined");

   // --- database-backed lookups (only if the id files are present) ---

   Pci_Usb_Id_Names intel = devid_get_pci_names(0x8086, 0, 0, 0, 1);
   if (intel.vendor_name) {
      CK_HAS(intel.vendor_name, "Intel");

      Pci_Usb_Id_Names linux_usb = devid_get_usb_names(0x1d6b, 0, 0, 1);
      CK_HAS(linux_usb.vendor_name, "Linux Foundation");

      // HUT page 0x01 is "Generic Desktop Controls"
      CK_HAS(devid_usage_code_page_name(0x01), "Generic Desktop");

      // HID descriptor item tags (usb.ids "R" segment) use the HID spec names
      CK_STR(devid_hid_descriptor_item_type(4), "Usage Page");
      CK_STR(devid_hid_descriptor_item_type(8), "Usage");
   }
   else {
      printf("NOTE  database-backed lookups skipped: pci.ids/usb.ids not found\n");
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
