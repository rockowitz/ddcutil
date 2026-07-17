/** @file test_execution_stats.c
 *
 *  Standalone unit tests for src/base/execution_stats.c: the IO-event and
 *  sleep-event name lookups, and the status-code loggers (which return the
 *  status code unchanged).  The elapsed/count reports depend on accumulated
 *  runtime state and are not exercised.
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

#include "public/ddcutil_status_codes.h"
#include "base/execution_stats.h"

static int total = 0;
static int failed = 0;

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

static void test_io_event_name(void) {
   CK_STR(io_event_name(IE_FILEIO_WRITE), "IE_FILEIO_WRITE");
   CK_STR(io_event_name(IE_IOCTL_WRITE),  "I2_IOCTL_WRITE");   // note the table's spelling
   CK_STR(io_event_name(IE_OPEN),         "IE_OPEN");
   CK_STR(io_event_name(IE_OTHER),        "IE_OTHER");
}

static void test_sleep_event_name(void) {
   CK_STR(sleep_event_name(SE_WRITE_TO_READ), "SE_WRITE_TO_READ");
   CK_STR(sleep_event_name(SE_POST_READ),     "SE_POST_READ");
   CK_STR(sleep_event_name(SE_SPECIAL),       "SE_SPECIAL");
}

static void test_status_code_loggers(void) {
   init_execution_stats();   // allocates the status-code count tables

   // the loggers return the status code unchanged
   CK_INT(log_status_code(DDCRC_DDC_DATA, "test"), DDCRC_DDC_DATA);
   CK_INT(log_status_code(DDCRC_DDC_DATA, "test"), DDCRC_DDC_DATA);   // idempotent return
   CK_INT(log_retryable_status_code(DDCRC_RETRIES, "test"), DDCRC_RETRIES);
}

int main(int argc, char ** argv) {
   test_io_event_name();
   test_sleep_event_name();
   test_status_code_loggers();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
