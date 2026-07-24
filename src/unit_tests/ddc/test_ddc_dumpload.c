/** @file test_ddc_dumpload.c
 *
 *  Standalone unit tests for src/ddc/ddc_dumpload.c: the DUMPVCP/LOADVCP
 *  text format -- create_dumpload_data_from_g_ptr_array() (parse) and
 *  convert_dumpload_data_to_string_array() (format), plus format_timestamp()
 *  and free_dumpload_data()/dbgrpt_dumpload_data(). The functions that
 *  perform real DDC communication (loadvcp_by_dumpload_data(),
 *  dumpvcp_as_dumpload_data(), dumpvcp_as_string(), ddc_set_multiple()) are
 *  not tested here.
 *
 *  Parsing a "VCP <code> <value>" line looks up the feature in the VCP
 *  feature table, which first checks for a user-defined feature
 *  definition file via $XDG_DATA_HOME/$XDG_DATA_DIRS. main() below points
 *  both at a freshly created, empty temporary directory so this lookup
 *  reliably misses (falling back to the built-in feature table) regardless
 *  of what may exist in the real environment -- no real file I/O depended
 *  on.
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

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util/coredefs.h"
#include "util/error_info.h"

#include "vcp/vcp_feature_values.h"

#include "ddc/ddc_dumpload.h"

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


static GPtrArray * lines_from(const char * const * strs, int ct) {
   GPtrArray * garray = g_ptr_array_new();
   for (int i = 0; i < ct; i++)
      g_ptr_array_add(garray, (gpointer) strs[i]);
   return garray;
}


static void test_create_dumpload_data_valid(void) {
   const char * lines[] = {
      "MFG_ID ACM",
      "MODEL TestModel",
      "SN SN12345",
      "PRODUCT_CODE 4660",
      "VCP_VERSION 2.1",
      "VCP 10 50",
      "VCP 12 75",
   };
   GPtrArray * garray = lines_from(lines, ARRAY_SIZE(lines));

   Dumpload_Data * data = NULL;
   Error_Info * errs = create_dumpload_data_from_g_ptr_array(garray, &data);
   CK(errs == NULL);
   CK(data != NULL);
   if (data) {
      CK_STR(data->mfg_id, "ACM");
      CK_STR(data->model, "TestModel");
      CK_STR(data->serial_ascii, "SN12345");
      CK_INT(data->product_code, 4660);
      CK_INT(data->vcp_version.major, 2);
      CK_INT(data->vcp_version.minor, 1);
      CK_INT(data->vcp_value_ct, 2);
      CK_INT(vcp_value_set_size(data->vcp_values), 2);
      DDCA_Any_Vcp_Value * v0 = vcp_value_set_get(data->vcp_values, 0);
      CK_INT(v0->opcode, 0x10);
      CK_INT(VALREC_CUR_VAL(v0), 50);
      DDCA_Any_Vcp_Value * v1 = vcp_value_set_get(data->vcp_values, 1);
      CK_INT(v1->opcode, 0x12);
      CK_INT(VALREC_CUR_VAL(v1), 75);
      free_dumpload_data(data);
   }
   g_ptr_array_free(garray, true);
}


static void test_create_dumpload_data_comments_and_blanks_ignored(void) {
   const char * lines[] = {
      "# a comment line",
      "",
      "* another comment style",
      "MFG_ID ACM",
      "MODEL M",
      "SN S",
      "VCP 10 30",
   };
   GPtrArray * garray = lines_from(lines, ARRAY_SIZE(lines));

   Dumpload_Data * data = NULL;
   Error_Info * errs = create_dumpload_data_from_g_ptr_array(garray, &data);
   CK(errs == NULL);
   CK(data != NULL);
   if (data) {
      CK_INT(data->vcp_value_ct, 1);
      free_dumpload_data(data);
   }
   g_ptr_array_free(garray, true);
}


static void test_create_dumpload_data_invalid_single_token(void) {
   const char * lines[] = { "GARBAGE" };
   GPtrArray * garray = lines_from(lines, ARRAY_SIZE(lines));

   Dumpload_Data * data = NULL;
   Error_Info * errs = create_dumpload_data_from_g_ptr_array(garray, &data);
   CK(errs != NULL);
   CK(data == NULL);
   if (errs)
      errinfo_free(errs);
   g_ptr_array_free(garray, true);
}


static void test_create_dumpload_data_unexpected_field(void) {
   const char * lines[] = { "NOT_A_REAL_FIELD something" };
   GPtrArray * garray = lines_from(lines, ARRAY_SIZE(lines));

   Dumpload_Data * data = NULL;
   Error_Info * errs = create_dumpload_data_from_g_ptr_array(garray, &data);
   CK(errs != NULL);
   CK(data == NULL);
   if (errs)
      errinfo_free(errs);
   g_ptr_array_free(garray, true);
}


static void test_create_dumpload_data_invalid_vcp_version(void) {
   const char * lines[] = { "VCP_VERSION 9.9" };
   GPtrArray * garray = lines_from(lines, ARRAY_SIZE(lines));

   Dumpload_Data * data = NULL;
   Error_Info * errs = create_dumpload_data_from_g_ptr_array(garray, &data);
   CK(errs != NULL);
   CK(data == NULL);
   if (errs)
      errinfo_free(errs);
   g_ptr_array_free(garray, true);
}


static void test_format_timestamp(void) {
   struct tm tm = {0};
   tm.tm_year = 2024 - 1900;
   tm.tm_mon  = 2;    // 0-based -> March
   tm.tm_mday = 15;
   tm.tm_hour = 13;
   tm.tm_min  = 5;
   tm.tm_sec  = 9;
   tm.tm_isdst = -1;
   time_t t = mktime(&tm);   // interpreted (and later re-formatted) as local time

   char buf[30];
   char * result = format_timestamp(t, buf, sizeof(buf));
   CK(result == buf);
   CK_STR(buf, "20240315-130509");
}


static void test_format_timestamp_allocates_when_no_buffer(void) {
   char * result = format_timestamp(time(NULL), NULL, 0);
   CK(result != NULL);
   CK_INT(strlen(result), 15);   // YYYYMMDD-HHMMSS
   free(result);
}


static void test_convert_dumpload_data_to_string_array(void) {
   Dumpload_Data * data = calloc(1, sizeof(Dumpload_Data));
   data->timestamp_millis = time(NULL);
   strcpy(data->mfg_id, "ACM");
   strcpy(data->model, "TestModel");
   strcpy(data->serial_ascii, "SN12345");
   data->product_code = 4660;
   data->vcp_version.major = 2;
   data->vcp_version.minor = 1;
   data->vcp_values = vcp_value_set_new(2);
   vcp_value_set_add(data->vcp_values, create_cont_vcp_value(0x10, 0, 50));
   data->vcp_value_ct = 1;

   GPtrArray * strings = convert_dumpload_data_to_string_array(data);
   CK(strings != NULL);

   bool found_mfg = false, found_model = false, found_sn = false,
        found_product = false, found_vspec = false, found_vcp = false;
   for (int i = 0; i < strings->len; i++) {
      char * s = g_ptr_array_index(strings, i);
      if (strstr(s, "MFG_ID") && strstr(s, "ACM")) found_mfg = true;
      if (strstr(s, "MODEL") && strstr(s, "TestModel")) found_model = true;
      if (strstr(s, "SN") && strstr(s, "SN12345")) found_sn = true;
      if (strstr(s, "PRODUCT_CODE") && strstr(s, "4660")) found_product = true;
      if (strstr(s, "VCP_VERSION") && strstr(s, "2.1")) found_vspec = true;
      if (strstr(s, "VCP") && strstr(s, "10") && strstr(s, "50")) found_vcp = true;
   }
   CK(found_mfg);
   CK(found_model);
   CK(found_sn);
   CK(found_product);
   CK(found_vspec);
   CK(found_vcp);

   g_ptr_array_free(strings, true);
   free_dumpload_data(data);
}


static void test_free_dumpload_data_null_safe(void) {
   free_dumpload_data(NULL);   // must not crash
   CK(true);
}


static void test_dbgrpt_dumpload_data_smoke(void) {
   Dumpload_Data * data = calloc(1, sizeof(Dumpload_Data));
   strcpy(data->mfg_id, "ACM");
   data->vcp_values = vcp_value_set_new(1);
   vcp_value_set_add(data->vcp_values, create_cont_vcp_value(0x10, 0, 50));

   QUIETLY( dbgrpt_dumpload_data(data, 0) );
   CK(true);   // reaching here without crashing is the test

   free_dumpload_data(data);
}


int main(int argc, char ** argv) {
   char tmpl[] = "/tmp/ddcutil_test_xdg_data_XXXXXX";
   char * tmpdir = mkdtemp(tmpl);
   if (!tmpdir) {
      fprintf(stderr, "mkdtemp() failed: %s\n", strerror(errno));
      return 2;
   }
   setenv("XDG_DATA_HOME", tmpdir, 1);
   setenv("XDG_DATA_DIRS", tmpdir, 1);

   test_create_dumpload_data_valid();
   test_create_dumpload_data_comments_and_blanks_ignored();
   test_create_dumpload_data_invalid_single_token();
   test_create_dumpload_data_unexpected_field();
   test_create_dumpload_data_invalid_vcp_version();
   test_format_timestamp();
   test_format_timestamp_allocates_when_no_buffer();
   test_convert_dumpload_data_to_string_array();
   test_free_dumpload_data_null_safe();
   test_dbgrpt_dumpload_data_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
