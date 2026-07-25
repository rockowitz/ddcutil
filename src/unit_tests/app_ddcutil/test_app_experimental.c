/** @file test_app_experimental.c
 *
 *  Standalone unit test for src/app_ddcutil/app_experimental.c:
 *  report_experimental_options(), which reports the state of the
 *  ddcutil "--f<N>"/"--i<N>" utility option flags from a Parsed_Cmd. It is
 *  pure formatting driven entirely by parsed_cmd->flags2 and compile-time
 *  default constants -- no hardware I/O.
 *
 *  test_display_detection_variants() drives real display detection
 *  (ddc_ensure_displays_detected(), i2c bus scanning) and so is out of
 *  scope for these unit tests.
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
#include <string.h>
#include <unistd.h>

#include "cmdline/parsed_cmd.h"

#include "app_ddcutil/app_experimental.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
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


static void test_report_experimental_options_all_disabled(void) {
   Parsed_Cmd parsed_cmd = {0};
   parsed_cmd.flags2 = 0;

   char buf[8192];
   CAPTURE( report_experimental_options(&parsed_cmd, 0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "Experimental Options:");
   CK_STR_CONTAINS(buf, "--f1");
   CK_STR_CONTAINS(buf, "disabled:");
   CK(strstr(buf, "enabled:") == NULL);
}


static void test_report_experimental_options_some_enabled(void) {
   Parsed_Cmd parsed_cmd = {0};
   parsed_cmd.flags2 = CMD_FLAG2_F1 | CMD_FLAG2_F2;

   char buf[8192];
   CAPTURE( report_experimental_options(&parsed_cmd, 0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "enabled:");
   CK_STR_CONTAINS(buf, "--i1:");   // utility option list also always shown
}


int main(int argc, char ** argv) {
   test_report_experimental_options_all_disabled();
   test_report_experimental_options_some_enabled();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
