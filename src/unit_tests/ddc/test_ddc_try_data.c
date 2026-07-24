/** @file test_ddc_try_data.c
 *
 *  Standalone unit tests for src/ddc/ddc_try_data.c: the maxtries
 *  get/set/init functions, try_data_reset_all(), try_data_record_tries()
 *  (which updates in-memory counters keyed by Retry_Operation), and a
 *  smoke test of the reporting functions. All pure in-memory bookkeeping
 *  guarded by a recursive mutex -- no hardware or file I/O.
 *
 *  try_data_record_tries() also updates a per-display record
 *  (Per_Display_Data), fabricated here via pdd_get_per_display_data() with
 *  a bogus (nonexistent) I2C bus number -- this is pure in-memory
 *  bookkeeping keyed by DDCA_IO_Path, not a real hardware access.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: ddc source files cross-reference each other
 *  and the rest of the ddcutil core extensively, so it links the full
 *  top-level libcommon convenience library (the same aggregate the
 *  ddcutil executable itself links) rather than a minimal per-directory
 *  library set.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_status_codes.h"

#include "util/edid.h"

#include "base/displays.h"
#include "base/dsa2.h"
#include "base/i2c_bus_aux.h"
#include "base/i2c_bus_base.h"
#include "base/per_display_data.h"
#include "base/status_code_mgt.h"

#include "ddc/ddc_try_data.h"

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

#define CK_STR_CONTAINS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected to contain \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _n); } \
} while(0)

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * then reads the captured output into `bufvar` (a char array) so its
 * content can be inspected. */
#define CAPTURE(stmt, bufvar) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   char _template[] = "/tmp/ddcutil_test_capture_XXXXXX"; \
   int _fd = mkstemp(_template); \
   dup2(_fd, fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   lseek(_fd, 0, SEEK_SET); \
   ssize_t _n = read(_fd, bufvar, sizeof(bufvar)-1); \
   bufvar[(_n > 0) ? _n : 0] = '\0'; \
   close(_fd); \
   unlink(_template); \
} while(0)


static void test_default_maxtries(void) {
   CK_INT(try_data_get_maxtries(WRITE_ONLY_TRIES_OP),  4);
   CK_INT(try_data_get_maxtries(WRITE_READ_TRIES_OP), 10);
   CK_INT(try_data_get_maxtries(MULTI_PART_READ_OP),   8);
   CK_INT(try_data_get_maxtries(MULTI_PART_WRITE_OP),  8);
}


static void test_set_maxtries(void) {
   try_data_set_maxtries(WRITE_ONLY_TRIES_OP, 7);
   CK_INT(try_data_get_maxtries(WRITE_ONLY_TRIES_OP), 7);

   try_data_set_maxtries(WRITE_ONLY_TRIES_OP, 4);   // restore
   CK_INT(try_data_get_maxtries(WRITE_ONLY_TRIES_OP), 4);
}


static void test_try_data_init_retry_type(void) {
   try_data_init_retry_type(MULTI_PART_WRITE_OP, 6);
   CK_INT(try_data_get_maxtries(MULTI_PART_WRITE_OP), 6);

   try_data_init_retry_type(MULTI_PART_WRITE_OP, 8);   // restore default
   CK_INT(try_data_get_maxtries(MULTI_PART_WRITE_OP), 8);
}


static void test_try_data_reset_all(void) {
   char buf[4000];

   try_data_set_maxtries(WRITE_ONLY_TRIES_OP, 4);
   CAPTURE( try_data_report(WRITE_ONLY_TRIES_OP, 0), buf );
   CK_STR_CONTAINS(buf, "No tries attempted");

   try_data_reset_all();
   CAPTURE( try_data_report(WRITE_ONLY_TRIES_OP, 0), buf );
   CK_STR_CONTAINS(buf, "No tries attempted");
   // maxtries setting itself is not affected by reset
   CK_INT(try_data_get_maxtries(WRITE_ONLY_TRIES_OP), 4);
}


// Builds a structurally valid 128-byte EDID (correct header + checksum),
// as required by dsa2's get_edid_checkbyte(), which reads
// I2C_Bus_Info.edid->bytes[127] to key its persistent stats.
static Parsed_Edid * make_valid_edid(Byte fill_byte) {
   Byte raw[128];
   memset(raw, fill_byte, 128);
   raw[0] = 0x00; raw[1] = raw[2] = raw[3] = raw[4] = raw[5] = raw[6] = 0xff; raw[7] = 0x00;
   int sum = 0;
   for (int i = 0; i < 127; i++)
      sum += raw[i];
   raw[127] = (Byte)((256 - (sum % 256)) % 256);
   return create_parsed_edid2(raw, "TEST");
}


static void test_try_data_record_tries(void) {
   try_data_reset_all();

   // busno must be <= I2C_BUS_MAX: pdd_get_per_display_data() below asserts
   // this when creating a new record with dynamic sleep adjustment enabled;
   // dsa2's get_edid_checkbyte() further requires the bus to actually be
   // present (with an EDID) in the all_i2c_buses global, so fabricate one.
   const int busno = 63;
   all_i2c_buses = g_ptr_array_new_with_free_func((GDestroyNotify) i2c_free_bus_info);
   I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
   businfo->edid = make_valid_edid(0x63);
   g_ptr_array_add(all_i2c_buses, businfo);

   Display_Ref * dref = create_bus_display_ref(busno);   // never opened, no real I/O
   dref->pdd = pdd_get_per_display_data(dref->io_path, /*create_if_not_found=*/true);
   Display_Handle * dh = create_base_display_handle(-1, dref);

   // 2 tries to succeed
   try_data_record_tries(dh, WRITE_READ_TRIES_OP, 0, 2);
   // 1 fatal failure
   try_data_record_tries(dh, WRITE_READ_TRIES_OP, DDCRC_NULL_RESPONSE, 1);
   // 1 failure due to retries exhausted
   try_data_record_tries(dh, WRITE_READ_TRIES_OP, DDCRC_RETRIES, 10);

   char buf[4000];
   CAPTURE( try_data_report(WRITE_READ_TRIES_OP, 0), buf );
   CK_STR_CONTAINS(buf, "Total successful attempts:          1");
   CK_STR_CONTAINS(buf, "Failed due to max tries exceeded:   1");
   CK_STR_CONTAINS(buf, "Failed due to fatal error:          1");
   CK_STR_CONTAINS(buf, "Total attempts:                     3");

   try_data_reset_all();

   free(dh);
   free_display_ref(dref);   // no-op: dref is not DREF_TRANSIENT
   g_ptr_array_free(all_i2c_buses, true);
   all_i2c_buses = NULL;
}


static void test_report_smoke(void) {
   char buf[8000];
   CAPTURE( ddc_report_max_tries(0), buf );
   CK_STR_CONTAINS(buf, "Maxtries Settings:");

   CAPTURE( ddc_report_ddc_stats(0), buf );
   CK_STR_CONTAINS(buf, "Maxtries Settings:");
   CK(true);   // reaching here without crashing is the primary test
}


int main(int argc, char ** argv) {
   init_dsa2();   // allocates results_tables[], read by pdd_init_pdd()
   init_per_display_data();
   init_ddc_try_data();

   test_default_maxtries();
   test_set_maxtries();
   test_try_data_init_retry_type();
   test_try_data_reset_all();
   test_try_data_record_tries();
   test_report_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
