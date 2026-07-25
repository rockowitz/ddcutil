/** @file test_app_capabilities.c
 *
 *  Standalone unit test for the pure subset of src/app_ddcutil/app_capabilities.c:
 *  app_test_capabilities_string(), which parses and reports a caller-supplied
 *  capabilities string with no display handle and no hardware I/O (used by
 *  the ddcutil "capabilities -test" option to analyze a user-submitted string).
 *
 *  app_get_capabilities_string(), app_show_parsed_capabilities(), and
 *  app_capabilities() all require a real open Display_Handle and so are out
 *  of scope for these unit tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappddcutil/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "public/ddcutil_status_codes.h"

#include "app_ddcutil/app_capabilities.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
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


static void test_null_capabilities_string(void) {
   DDCA_Status rc;
   QUIETLY( rc = app_test_capabilities_string(NULL) );
   CK(rc == DDCRC_ARG);
}


static void test_valid_capabilities_string(void) {
   char caps[] = "(prot(monitor)type(lcd)model(TestModel)cmds(01 02 03)vcp(02 04 05 08 10 12 60))";
   DDCA_Status rc;
   QUIETLY( rc = app_test_capabilities_string(caps) );
   CK(rc == 0);
}


static void test_malformed_capabilities_string(void) {
   // pcaps is always returned by parse_capabilities_string(), even when
   // damaged -- app_test_capabilities_string() must not crash reporting it
   char caps[] = "not a well formed capabilities string";
   DDCA_Status rc;
   QUIETLY( rc = app_test_capabilities_string(caps) );
   CK(rc == 0);
}


int main(int argc, char ** argv) {
   test_null_capabilities_string();
   test_valid_capabilities_string();
   test_malformed_capabilities_string();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
