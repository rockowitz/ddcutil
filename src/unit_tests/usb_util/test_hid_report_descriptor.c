/** @file test_hid_report_descriptor.c
 *
 *  Standalone unit tests for src/usb_util/hid_report_descriptor.c: the
 *  full HID report descriptor parsing pipeline (parse_hid_report_desc(),
 *  built on top of base_hid_report_descriptor.c's byte tokenizer),
 *  extended_usage(), hid_report_type_name(), interpret_item_flags_r(),
 *  is_monitor_by_parsed_hid_report_descriptor(), get_vcp_code_reports(),
 *  find_edid_report_descriptor(), and select_parsed_hid_report_descriptors().
 *  All pure byte-stream parsing -- no hardware or file I/O.
 *
 *  test_full_monitor_descriptor_pipeline() hand-builds a HID report
 *  descriptor byte stream describing a monitor exactly as the USB
 *  Monitor Control Class Specification requires: a top-level Application
 *  collection with usage page 0x80 (USB Monitor)/usage id 0x01 (Monitor
 *  Control), containing 2 Feature reports on different report ids -- one
 *  an EDID field (usage page 0x80, usage id 0x02, buffered bytes, 128
 *  bytes) and one a VCP feature field (usage page 0x82, usage id 0x10,
 *  matching how ddcutil reads VCP feature x10 over USB HID) -- then
 *  exercises the query functions a real caller (hidraw_util.c/
 *  hiddev_util.c) would use against the parsed result.
 *
 *  get_vcp_code_from_parsed_hid_report() is not tested: it is dead code
 *  (defined but never called, and not declared in any header), and its
 *  usage-page check (0x80) is inconsistent with the one
 *  get_vcp_code_reports() actually uses (0x0082) -- not something to
 *  paper over with a test that assumes one or the other is "correct".
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusbutil/
 *  libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/coredefs.h"

#include "usb_util/base_hid_report_descriptor.h"
#include "usb_util/hid_report_descriptor.h"

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


static void test_hid_report_type_name(void) {
   CK_STR(hid_report_type_name(HID_REPORT_TYPE_INPUT), "Input");
   CK_STR(hid_report_type_name(HID_REPORT_TYPE_OUTPUT), "Output");
   CK_STR(hid_report_type_name(HID_REPORT_TYPE_FEATURE), "Feature");
   CK_STR(hid_report_type_name(0), "invalid");    // below HID_REPORT_TYPE_MIN
   CK_STR(hid_report_type_name(99), "invalid");   // above HID_REPORT_TYPE_MAX
}


static void test_interpret_item_flags_r(void) {
   char buf[200];
   char * r = interpret_item_flags_r(0x0000, buf, sizeof(buf));
   CK(r == buf);
   CK_STR_CONTAINS(buf, "Data");
   CK_STR_CONTAINS(buf, "Array");
   CK_STR_CONTAINS(buf, "Bitfield");

   interpret_item_flags_r(0x0103, buf, sizeof(buf));   // Constant|Variable|BufferedBytes
   CK_STR_CONTAINS(buf, "Constant");
   CK_STR_CONTAINS(buf, "Variable");
   CK_STR_CONTAINS(buf, "Buffered Bytes");
}


static void test_extended_usage(void) {
   // usage_bsize 3 or 4: usage is already an extended (32-bit) value, returned unchanged
   CK_INT(extended_usage(0x80, 0x00800002, 4), 0x00800002);
   CK_INT(extended_usage(0x80, 0x00800002, 3), 0x00800002);

   // usage_bsize 1 or 2: combine usage_page (high 16 bits) with usage (low 16 bits)
   CK_INT(extended_usage(0x80, 0x02, 1), 0x00800002);
   CK_INT(extended_usage(0x82, 0x10, 2), 0x00820010);

   // usage_bsize 0 (heuristic): usage with high byte set is assumed already extended
   CK_INT(extended_usage(0x80, 0x00820010, 0), 0x00820010);
   // usage_bsize 0, usage with no high byte: combined with usage_page
   CK_INT(extended_usage(0x80, 0x02, 0), 0x00800002);
}


static void test_free_parsed_hid_descriptor_null_safe(void) {
   free_parsed_hid_descriptor(NULL);   // must not crash
   CK(true);
}


// Builds a HID report descriptor byte stream for a monitor per the USB
// Monitor Control Class Specification: a top-level Application collection
// (usage page 0x80 "USB Monitor", usage id 0x01 "Monitor Control")
// containing 2 Feature reports:
//   report id 1: EDID field  (usage page 0x80, usage id 0x02, buffered
//                bytes, report_size=8, report_count=128)
//   report id 2: VCP feature field for feature code 0x10 (usage page
//                0x82, usage id 0x10, report_size=8, report_count=1)
static Byte monitor_descriptor_bytes[] = {
   0x05, 0x80,         // Usage Page (Global, 1 byte) = 0x80
   0x09, 0x01,         // Usage (Local, 1 byte) = 0x01
   0xa1, 0x01,         // Collection (Main, 1 byte) = 0x01 (Application)

   0x85, 0x01,         //   Report ID (Global, 1 byte) = 1
   0x09, 0x02,         //   Usage (Local, 1 byte) = 0x02 (EDID)
   0x75, 0x08,         //   Report Size (Global, 1 byte) = 8
   0x95, 0x80,         //   Report Count (Global, 1 byte) = 128
   0xb2, 0x02, 0x01,   //   Feature (Main, 2 byte) = 0x0102 (Variable | Buffered Bytes)

   0x85, 0x02,         //   Report ID (Global, 1 byte) = 2
   0x05, 0x82,         //   Usage Page (Global, 1 byte) = 0x82
   0x09, 0x10,         //   Usage (Local, 1 byte) = 0x10 (VCP feature code)
   0x75, 0x08,         //   Report Size (Global, 1 byte) = 8
   0x95, 0x01,         //   Report Count (Global, 1 byte) = 1
   0xb1, 0x02,         //   Feature (Main, 1 byte) = 0x02 (Variable)

   0xc0,               // End Collection
};


static void test_full_monitor_descriptor_pipeline(void) {
   Parsed_Hid_Descriptor * phd =
         parse_hid_report_desc(monitor_descriptor_bytes, sizeof(monitor_descriptor_bytes));
   CK(phd != NULL);
   CK(phd->valid_descriptor);
   CK(phd->root_collection != NULL);

   CK(is_monitor_by_parsed_hid_report_descriptor(phd));

   CK(phd->root_collection->child_collections != NULL);
   CK_INT(phd->root_collection->child_collections->len, 1);
   Parsed_Hid_Collection * app_col = g_ptr_array_index(phd->root_collection->child_collections, 0);
   CK_INT(app_col->extended_usage, 0x00800001);
   CK_INT(app_col->collection_type, 0x01);

   CK(app_col->reports != NULL);
   CK_INT(app_col->reports->len, 2);

   // EDID report (report id 1)
   Parsed_Hid_Report * edid_rpt = find_hid_report(app_col, HID_REPORT_TYPE_FEATURE, 1);
   CK(edid_rpt != NULL);
   if (edid_rpt) {
      CK_INT(edid_rpt->hid_fields->len, 1);
      Parsed_Hid_Field * f = g_ptr_array_index(edid_rpt->hid_fields, 0);
      CK_INT(f->usage_page, 0x80);
      CK_INT(f->report_size, 8);
      CK_INT(f->report_count, 128);
      CK(f->item_flags & HID_FIELD_BUFFERED_BYTE);
      CK(f->extended_usages != NULL);
      CK_INT(f->extended_usages->len, 1);
      CK_INT(g_array_index(f->extended_usages, uint32_t, 0), 0x00800002);
   }

   // VCP feature report (report id 2)
   Parsed_Hid_Report * vcp_rpt = find_hid_report(app_col, HID_REPORT_TYPE_FEATURE, 2);
   CK(vcp_rpt != NULL);
   if (vcp_rpt) {
      CK_INT(vcp_rpt->hid_fields->len, 1);
      Parsed_Hid_Field * f = g_ptr_array_index(vcp_rpt->hid_fields, 0);
      CK_INT(f->usage_page, 0x82);
      CK_INT(f->report_size, 8);
      CK_INT(g_array_index(f->extended_usages, uint32_t, 0), 0x00820010);
   }

   // find_edid_report_descriptor() must find the EDID report specifically,
   // not the VCP feature report
   Parsed_Hid_Report * found_edid = find_edid_report_descriptor(phd);
   CK(found_edid == edid_rpt);

   // get_vcp_code_reports() must find exactly the VCP report, with the
   // correct feature code extracted from its extended usage
   GPtrArray * vcp_reports = get_vcp_code_reports(phd);
   CK(vcp_reports != NULL);
   CK_INT(vcp_reports->len, 1);
   if (vcp_reports->len == 1) {
      Vcp_Code_Report * vcr = g_ptr_array_index(vcp_reports, 0);
      CK_INT(vcr->vcp_code, 0x10);
      CK(vcr->rpt == vcp_rpt);
   }
   // caller owns the Vcp_Code_Report structs but not the Parsed_Hid_Report
   // pointers they reference (those belong to phd)
   for (int i = 0; i < vcp_reports->len; i++)
      free(g_ptr_array_index(vcp_reports, i));
   g_ptr_array_free(vcp_reports, true);

   // select_parsed_hid_report_descriptors() must find both Feature reports
   GPtrArray * feature_reports =
         select_parsed_hid_report_descriptors(phd, HIDF_REPORT_TYPE_FEATURE);
   CK_INT(feature_reports->len, 2);
   g_ptr_array_free(feature_reports, true);

   // ... and none when asking for Input/Output only
   GPtrArray * input_reports =
         select_parsed_hid_report_descriptors(phd, HIDF_REPORT_TYPE_INPUT);
   CK_INT(input_reports->len, 0);
   g_ptr_array_free(input_reports, true);

   free_parsed_hid_descriptor(phd);
}


static void test_non_monitor_descriptor(void) {
   // Usage page 0x01 (Generic Desktop), not a monitor
   Byte bytes[] = {
      0x05, 0x01,
      0x09, 0x02,
      0xa1, 0x01,
      0xc0,
   };
   Parsed_Hid_Descriptor * phd = parse_hid_report_desc(bytes, sizeof(bytes));
   CK(phd != NULL);
   CK(!is_monitor_by_parsed_hid_report_descriptor(phd));
   CK(find_edid_report_descriptor(phd) == NULL);

   GPtrArray * vcp_reports = get_vcp_code_reports(phd);
   CK_INT(vcp_reports->len, 0);
   g_ptr_array_free(vcp_reports, true);

   free_parsed_hid_descriptor(phd);
}


static void test_dbgrpt_parsed_hid_descriptor_smoke(void) {
   Parsed_Hid_Descriptor * phd =
         parse_hid_report_desc(monitor_descriptor_bytes, sizeof(monitor_descriptor_bytes));
   QUIETLY( dbgrpt_parsed_hid_descriptor(phd, 0) );
   CK(true);   // reaching here without crashing is the test
   free_parsed_hid_descriptor(phd);
}


int main(int argc, char ** argv) {
   test_hid_report_type_name();
   test_interpret_item_flags_r();
   test_extended_usage();
   test_free_parsed_hid_descriptor_null_safe();
   test_full_monitor_descriptor_pipeline();
   test_non_monitor_descriptor();
   test_dbgrpt_parsed_hid_descriptor_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
