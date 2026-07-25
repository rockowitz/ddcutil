/** @file test_query_sysenv_xref.c
 *
 *  Standalone unit tests for src/app_sysenv/query_sysenv_xref.c: the
 *  Device_Id_Xref cross-reference table. This module is pure in-memory
 *  bookkeeping (a GPtrArray of Device_Id_Xref records keyed by raw EDID
 *  bytes and I2C bus number) -- no file or hardware I/O of its own; the
 *  scanning code that populates it lives elsewhere.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappsysenv/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/coredefs.h"

#include "app_sysenv/query_sysenv_xref.h"

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

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file
 * whose contents are then read into caller-supplied buffer `outbuf`
 * (size outbufsz), NUL terminated. */
#define CAPTURE(stmt, outbuf, outbufsz) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   rewind(_tmp); \
   size_t _n = fread((outbuf), 1, (outbufsz)-1, _tmp); \
   (outbuf)[_n] = '\0'; \
   fclose(_tmp); \
} while(0)


static void make_edid(Byte * buf, Byte tag_byte) {
   memset(buf, 0, 128);
   buf[124] = tag_byte;   // only the last 4 bytes feed device_xref_edid_tag()
   buf[127] = 0x01;
}


static void test_device_xref_edid_tag(void) {
   Byte edid[128];
   make_edid(edid, 0xAB);
   char * tag = device_xref_edid_tag(edid);
   CK(tag != NULL);
   // last 4 bytes (124..127) as uppercase hex: AB 00 00 01
   CK_STR(tag, "AB000001");
   free(tag);
}


static void test_find_and_new_by_busno(void) {
   device_xref_init();

   CK(device_xref_find_by_busno(3) == NULL);

   Byte edid1[128];
   make_edid(edid1, 0x11);
   Device_Id_Xref * x1 = device_xref_new_with_busno(3, edid1);
   CK(x1 != NULL);
   CK(x1->i2c_busno == 3);

   Byte edid2[128];
   make_edid(edid2, 0x22);
   Device_Id_Xref * x2 = device_xref_new_with_busno(5, edid2);
   CK(x2 != NULL);

   CK(device_xref_find_by_busno(3) == x1);
   CK(device_xref_find_by_busno(5) == x2);
   CK(device_xref_find_by_busno(99) == NULL);

   device_xref_set_i2c_bus_scan_complete();

   CK(device_xref_find_by_edid(edid1) == x1);
   CK(device_xref_find_by_edid(edid2) == x2);
   CK(x1->ambiguous_edid == false);
   CK(x2->ambiguous_edid == false);
}


static void test_duplicate_edid_marked_ambiguous(void) {
   device_xref_init();

   Byte shared_edid[128];
   make_edid(shared_edid, 0x33);
   Byte shared_edid_copy[128];
   memcpy(shared_edid_copy, shared_edid, 128);

   Device_Id_Xref * x1 = device_xref_new_with_busno(7, shared_edid);
   Device_Id_Xref * x2 = device_xref_new_with_busno(8, shared_edid_copy);

   device_xref_set_i2c_bus_scan_complete();   // triggers duplicate-EDID detection

   CK(x1->ambiguous_edid == true);
   CK(x2->ambiguous_edid == true);
}


static void test_device_xref_report_smoke(void) {
   device_xref_init();
   Byte edid[128];
   make_edid(edid, 0x44);
   device_xref_new_with_busno(2, edid);
   device_xref_set_i2c_bus_scan_complete();

   char buf[4000];
   CAPTURE( device_xref_report(0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "Device Identifier Cross Reference Report");
   CK_STR_CONTAINS(buf, "/dev/i2c busno:     2");
}


int main(int argc, char ** argv) {
   test_device_xref_edid_tag();
   test_find_and_new_by_busno();
   test_duplicate_edid_marked_ambiguous();
   test_device_xref_report_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
