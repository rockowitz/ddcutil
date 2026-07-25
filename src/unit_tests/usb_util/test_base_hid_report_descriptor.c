/** @file test_base_hid_report_descriptor.c
 *
 *  Standalone unit tests for src/usb_util/base_hid_report_descriptor.c:
 *  tokenize_hid_report_descriptor() (the raw byte-stream -> linked-list
 *  tokenizer), is_monitor_by_tokenized_hid_report_descriptor(), and
 *  free_hid_report_item_list(). All pure byte-stream parsing -- no
 *  hardware or file I/O; the "device" is simply a hand-built byte array
 *  representing a HID Report Descriptor, per USB HID Specification v1.11.
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
#include <unistd.h>

#include "util/coredefs.h"

#include "usb_util/base_hid_report_descriptor.h"

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


// A short, well-formed HID report descriptor: 1-byte items throughout,
// declaring a Usage Page (Global, tag 0x04) of 0x80 (USB Monitor page,
// per the USB Monitor Control Class Spec), a Usage (Local, tag 0x08) of
// 0x01, and a Collection (Main, tag 0xa0) of type 0x01 (Application).
// Byte layout per item: [tag|type|size] [data bytes].
static Byte descriptor_bytes[] = {
   0x05, 0x80,   // Usage Page (Global, 1-byte data) = 0x80
   0x09, 0x01,   // Usage (Local, 1-byte data) = 0x01
   0xa1, 0x01,   // Collection (Main, 1-byte data) = 0x01 (Application)
   0xc0,         // End Collection (Main, 0-byte data)
};


static void test_tokenize_basic_fields(void) {
   Hid_Report_Descriptor_Item * head =
         tokenize_hid_report_descriptor(descriptor_bytes, sizeof(descriptor_bytes));
   CK(head != NULL);

   // item 1: Usage Page = 0x80
   CK(head != NULL);
   if (head) {
      CK_INT(head->btag, 0x04);
      CK_INT(head->btype, 1);   // Global
      CK_INT(head->bsize_bytect, 1);
      CK_INT(head->data, 0x80);
   }

   // item 2: Usage = 0x01
   Hid_Report_Descriptor_Item * item2 = head ? head->next : NULL;
   CK(item2 != NULL);
   if (item2) {
      CK_INT(item2->btag, 0x08);
      CK_INT(item2->btype, 2);   // Local
      CK_INT(item2->data, 0x01);
   }

   // item 3: Collection = 0x01 (Application)
   Hid_Report_Descriptor_Item * item3 = item2 ? item2->next : NULL;
   CK(item3 != NULL);
   if (item3) {
      CK_INT(item3->btag, 0xa0);
      CK_INT(item3->btype, 0);   // Main
      CK_INT(item3->data, 0x01);
   }

   // item 4: End Collection, zero-length data
   Hid_Report_Descriptor_Item * item4 = item3 ? item3->next : NULL;
   CK(item4 != NULL);
   if (item4) {
      CK_INT(item4->btag, 0xc0);
      CK_INT(item4->bsize_bytect, 0);
      CK(item4->next == NULL);   // last item in the list
   }

   free_hid_report_item_list(head);
}


static void test_tokenize_multibyte_sizes(void) {
   // size indicator 2 (0b10) means 2 data bytes; size indicator 3 (0b11)
   // means 4 data bytes (per USB HID spec 6.2.2.2 -- 3 is not itself a
   // byte count, it's the special-case encoding for 4).
   Byte bytes[] = {
      0x06, 0x34, 0x12,               // Usage Page (Global, 2-byte data) = 0x1234
      0x27, 0x78, 0x56, 0x34, 0x12,   // Logical Maximum (Global, 4-byte data) = 0x12345678
   };
   Hid_Report_Descriptor_Item * head = tokenize_hid_report_descriptor(bytes, sizeof(bytes));
   CK(head != NULL);
   if (head) {
      CK_INT(head->bsize_bytect, 2);
      CK_INT(head->data, 0x1234);
      CK(memcmp(head->raw_bytes, bytes, 3) == 0);

      Hid_Report_Descriptor_Item * item2 = head->next;
      CK(item2 != NULL);
      if (item2) {
         CK_INT(item2->btag, 0x24);
         CK_INT(item2->bsize_bytect, 4);
         CK_INT(item2->data, 0x12345678);
         CK(item2->next == NULL);
      }
   }
   free_hid_report_item_list(head);
}


static void test_tokenize_empty(void) {
   Hid_Report_Descriptor_Item * head = tokenize_hid_report_descriptor(descriptor_bytes, 0);
   CK(head == NULL);
   free_hid_report_item_list(head);   // must be safe on NULL
}


static void test_free_hid_report_item_list_null_safe(void) {
   free_hid_report_item_list(NULL);   // must not crash
   CK(true);
}


static void test_is_monitor_by_tokenized_hid_report_descriptor(void) {
   Hid_Report_Descriptor_Item * monitor_head =
         tokenize_hid_report_descriptor(descriptor_bytes, sizeof(descriptor_bytes));
   CK(is_monitor_by_tokenized_hid_report_descriptor(monitor_head));
   free_hid_report_item_list(monitor_head);

   // a descriptor whose Usage Page is not 0x80 (USB Monitor)
   Byte non_monitor_bytes[] = {
      0x05, 0x01,   // Usage Page = 0x01 (Generic Desktop)
      0x09, 0x02,
      0xa1, 0x01,
      0xc0,
   };
   Hid_Report_Descriptor_Item * non_monitor_head =
         tokenize_hid_report_descriptor(non_monitor_bytes, sizeof(non_monitor_bytes));
   CK(!is_monitor_by_tokenized_hid_report_descriptor(non_monitor_head));
   free_hid_report_item_list(non_monitor_head);

   // no Usage Page item at all
   Byte no_usage_page_bytes[] = { 0xc0 };
   Hid_Report_Descriptor_Item * no_usage_page_head =
         tokenize_hid_report_descriptor(no_usage_page_bytes, sizeof(no_usage_page_bytes));
   CK(!is_monitor_by_tokenized_hid_report_descriptor(no_usage_page_head));
   free_hid_report_item_list(no_usage_page_head);

   // an empty list (NULL head)
   CK(!is_monitor_by_tokenized_hid_report_descriptor(NULL));
}


static void test_report_hid_report_item_list_smoke(void) {
   Hid_Report_Descriptor_Item * head =
         tokenize_hid_report_descriptor(descriptor_bytes, sizeof(descriptor_bytes));
   QUIETLY( report_hid_report_item_list(head, 0) );
   CK(true);   // reaching here without crashing is the test
   free_hid_report_item_list(head);
}


int main(int argc, char ** argv) {
   test_tokenize_basic_fields();
   test_tokenize_multibyte_sizes();
   test_tokenize_empty();
   test_free_hid_report_item_list_null_safe();
   test_is_monitor_by_tokenized_hid_report_descriptor();
   test_report_hid_report_item_list_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
