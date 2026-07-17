/** @file test_core.c
 *
 *  Standalone unit tests for the pure/table-lookup functions in src/base/core.c:
 *  execution-mode names, the Call_Options interpreter, the syslog-level
 *  name/value conversions and priority mapping, and the report-DDC-errors flag.
 *  The dbgtrc/tracing machinery depends on extensive global state and is not
 *  exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "public/ddcutil_types.h"
#include "base/core.h"

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

#define CK_HAS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  \"%s\" does not contain \"%s\"\n", __LINE__, \
             _a ? _a : "(null)", _n); } \
} while(0)

static void test_execution_mode_name(void) {
   CK_STR(execution_mode_name(MODE_DDCUTIL), "ddcutil");
   CK_STR(execution_mode_name(MODE_LIBDDCUTIL), "libddcutil");
}

static void test_call_options(void) {
   // interpret_call_options_t names each set flag (joined by '|')
   CK_HAS(interpret_call_options_t(CALLOPT_ERR_MSG), "CALLOPT_ERR_MSG");
   char * combined = interpret_call_options_t(CALLOPT_ERR_MSG | CALLOPT_WAIT);
   CK_HAS(combined, "CALLOPT_ERR_MSG");
   CK_HAS(combined, "CALLOPT_WAIT");
   // a flag that is not set is not named
   CK(strstr(interpret_call_options_t(CALLOPT_WAIT), "CALLOPT_RDONLY") == NULL);
}

static void test_syslog_level_name(void) {
   // name is the stringified enum constant
   CK_STR(syslog_level_name(DDCA_SYSLOG_ERROR),   "DDCA_SYSLOG_ERROR");
   CK_STR(syslog_level_name(DDCA_SYSLOG_DEBUG),   "DDCA_SYSLOG_DEBUG");
   CK_STR(syslog_level_name(DDCA_SYSLOG_NEVER),   "DDCA_SYSLOG_NEVER");
   CK_STR(syslog_level_name(DDCA_SYSLOG_NOT_SET), "DDCA_SYSLOG_NOT_SET");
}

static void test_syslog_level_to_value(void) {
   // lookup is by the title field, case-insensitive
   CK_INT(syslog_level_name_to_value("ERROR"),  DDCA_SYSLOG_ERROR);
   CK_INT(syslog_level_name_to_value("error"),  DDCA_SYSLOG_ERROR);
   CK_INT(syslog_level_name_to_value("WARN"),   DDCA_SYSLOG_WARNING);
   CK_INT(syslog_level_name_to_value("DEBUG"),  DDCA_SYSLOG_DEBUG);
   CK_INT(syslog_level_name_to_value("NEVER"),  DDCA_SYSLOG_NEVER);
   CK_INT(syslog_level_name_to_value("bogus"),  DDCA_SYSLOG_NOT_SET);   // default
}

static void test_syslog_importance(void) {
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_ERROR),   LOG_ERR);
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_WARNING), LOG_WARNING);
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_NOTICE),  LOG_NOTICE);
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_INFO),    LOG_INFO);
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_DEBUG),   LOG_DEBUG);
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_NEVER),   -1);
   CK_INT(syslog_importance_from_ddcutil_syslog_level(DDCA_SYSLOG_NOT_SET), -1);
}

static void test_report_ddc_errors_flag(void) {
   // enable_report_ddc_errors returns the previous value; the getter reflects it
   bool orig = is_report_ddc_errors_enabled();
   bool prev = enable_report_ddc_errors(true);
   CK(prev == orig);
   CK(is_report_ddc_errors_enabled() == true);
   prev = enable_report_ddc_errors(false);
   CK(prev == true);
   CK(is_report_ddc_errors_enabled() == false);
   enable_report_ddc_errors(orig);   // restore
}

int main(int argc, char ** argv) {
   test_execution_mode_name();
   test_call_options();
   test_syslog_level_name();
   test_syslog_level_to_value();
   test_syslog_importance();
   test_report_ddc_errors_flag();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
