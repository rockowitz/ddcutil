/** @file test_api_base.c
 *
 *  Standalone unit tests for src/libmain/api_base.c: the subset of the
 *  public API that neither requires library initialization
 *  (ddca_init()/ddca_init2()) nor a real display -- version/build
 *  metadata, status code name/description lookup, output level and
 *  syslog level name lookup, the verify-setvcp flag, and the
 *  no-display-open path of the sleep multiplier accessors.
 *
 *  Most other functions in api_base.c are wrapped in the API_PROLOG/
 *  API_PROLOGX macros, which -- if the library has not yet been
 *  initialized -- silently perform full implicit initialization
 *  (ddci_init()), including real I2C bus and display detection. That is
 *  exactly the kind of real-hardware side effect these unit tests are
 *  designed to avoid, so only functions confirmed NOT to expand either
 *  macro are exercised here.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon+libsharedlib unit test: it links the internal
 *  libmain/libsharedlib.la convenience library (the intermediate library
 *  that becomes libddcutil.so) together with the top-level libcommon
 *  convenience library it depends on.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

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


static void test_version_and_build_info(void) {
   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   // sanity check only: the concrete version changes release to release
   CK(vspec.major < 255 || vspec.minor < 255 || vspec.micro < 255);

   const char * vstr = ddca_ddcutil_version_string();
   CK(vstr != NULL);
   char buf[20];
   snprintf(buf, sizeof(buf), "%d.%d.%d", vspec.major, vspec.minor, vspec.micro);
   CK_STR(vstr, buf);

   const char * ext = ddca_ddcutil_extended_version_string();
   CK(ext != NULL);
   CK_STR_CONTAINS(ext, vstr);

   DDCA_Build_Option_Flags opts = ddca_build_options();
   // Just confirm the call is well-defined and returns a subset of the
   // documented bits, not a garbage value.
   CK((opts & ~(DDCA_BUILT_WITH_USB | DDCA_BUILT_WITH_FAILSIM)) == 0);
}


static void test_rc_name_and_desc(void) {
   // DDCRC_OK (0) is special-cased to a static "OK"/"success" record
   // rather than looked up in a table, so unlike every other status code
   // its "name" is the plain word "OK", not the macro's symbol text.
   CK_STR(ddca_rc_name(DDCRC_OK), "OK");
   CK_STR(ddca_rc_name(DDCRC_ARG), "DDCRC_ARG");

   // An unassigned code within a real modulation range (DDC: 3000-3999,
   // per RCRANGE_DDC_START/RCRANGE_DDC_MAX in base/status_code_mgt.h) is
   // looked up and gracefully not found. A code outside every configured
   // range (e.g. -99999) is not a supported input: status_code_mgt.c's
   // get_modulation() asserts that some range always matches, so passing
   // one would abort the process rather than return NULL -- consistent
   // with test_status_code_mgt.c (base), which likewise never probes
   // outside a known range.
   int unassigned_ddc_code = -(RCRANGE_DDC_START + 999);   // -3999
   CK(ddca_rc_name(unassigned_ddc_code) == NULL);

   CK(ddca_rc_desc(DDCRC_OK) != NULL);
   CK_STR(ddca_rc_desc(unassigned_ddc_code), "unknown status code");
}


static void test_output_level(void) {
   DDCA_Output_Level saved = ddca_get_output_level();

   DDCA_Output_Level old = ddca_set_output_level(DDCA_OL_VERBOSE);
   CK_INT(old, saved);
   CK_INT(ddca_get_output_level(), DDCA_OL_VERBOSE);

   // output_level_name() returns a descriptive title ("Verbose"), not the
   // enum symbol's stringified name.
   CK_STR(ddca_output_level_name(DDCA_OL_VERBOSE), "Verbose");
   CK_STR(ddca_output_level_name(DDCA_OL_NORMAL),  "Normal");

   ddca_set_output_level(saved);   // restore
   CK_INT(ddca_get_output_level(), saved);
}


static void test_syslog_level_from_name(void) {
   CK_INT(ddca_syslog_level_from_name("NEVER"), DDCA_SYSLOG_NEVER);
   CK_INT(ddca_syslog_level_from_name("ERROR"), DDCA_SYSLOG_ERROR);
   CK_INT(ddca_syslog_level_from_name("not-a-level"), DDCA_SYSLOG_NOT_SET);
}


static void test_enable_verify(void) {
   bool saved = ddca_is_verify_enabled();

   bool old = ddca_enable_verify(true);
   CK_INT(old, saved);
   CK(ddca_is_verify_enabled());

   ddca_enable_verify(false);
   CK(!ddca_is_verify_enabled());

   ddca_enable_verify(saved);   // restore
}


// ddca_set_sleep_multiplier()/ddca_get_sleep_multiplier() are marked
// __attribute__((deprecated)) (kept for 1.x compatibility per their doc
// comments in ddcutil_c_api.h) but are still fully live, compiled, public
// API; deliberately testing them still means calling them.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void test_sleep_multiplier_no_display_open(void) {
   // No display has ever been opened on this thread (ptd->cur_dh is
   // NULL), so both accessors must take their "no current display" path
   // and return -1.0 without touching any Per_Display_Data.
   double result = ddca_get_sleep_multiplier();
   CK(result == -1.0);

   double old = ddca_set_sleep_multiplier(2.0);
   CK(old == -1.0);
   // still no current display, so a later get still reports -1.0
   CK(ddca_get_sleep_multiplier() == -1.0);

   // an out-of-range multiplier is rejected without touching old_value's
   // initial -1.0 default, regardless of current-display state
   double old2 = ddca_set_sleep_multiplier(99.0);
   CK(old2 == -1.0);
}

#pragma GCC diagnostic pop


static void test_elapsed_nanosec(void) {
   uint64_t t1 = ddca_elapsed_nanosec();
   uint64_t t2 = ddca_elapsed_nanosec();
   CK(t2 >= t1);   // monotonic
}


int main(int argc, char ** argv) {
   test_version_and_build_info();
   test_rc_name_and_desc();
   test_output_level();
   test_syslog_level_from_name();
   test_enable_verify();
   test_sleep_multiplier_no_display_open();
   test_elapsed_nanosec();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
