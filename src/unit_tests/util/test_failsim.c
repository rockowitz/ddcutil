/** @file test_failsim.c
 *
 *  Standalone unit tests for src/util/failsim.c, the failure-injection framework.
 *
 *  A simulated failure is registered for a function name with fsim_add_error(),
 *  either recurring (every Nth call) or single (the Nth call only).  These tests
 *  drive fsim_check_failure() across successive calls and confirm the forced
 *  failure fires on exactly the right call, plus the call-count reset, the
 *  per-function and whole-table clears, and the int/errinfo injectors (which are
 *  gated by the failsim_enabled flag).
 *
 *  When a failure is forced, failsim writes a notice and a backtrace to stdout;
 *  those calls are made with stdout redirected to /dev/null so the test log stays
 *  readable.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/error_info.h"
#include "util/failsim.h"

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

// stdout muting, so failsim's notice/backtrace output stays out of the log.
static int saved_stdout = -1;
static void mute(void) {
   fflush(stdout);
   saved_stdout = dup(STDOUT_FILENO);
   int devnull = open("/dev/null", O_WRONLY);
   dup2(devnull, STDOUT_FILENO);
   close(devnull);
}
static void unmute(void) {
   fflush(stdout);
   dup2(saved_stdout, STDOUT_FILENO);
   close(saved_stdout);
   saved_stdout = -1;
}

static void test_single(void) {
   char fn[] = "single_fn";
   fsim_add_error(fn, FSIM_CALL_OCC_SINGLE, 2, -5);   // fail only on call 2

   mute();
   Failsim_Result r1 = fsim_check_failure("f.c", "single_fn");
   Failsim_Result r2 = fsim_check_failure("f.c", "single_fn");
   Failsim_Result r3 = fsim_check_failure("f.c", "single_fn");
   unmute();

   CK(r1.force_failure == false);
   CK(r2.force_failure == true);
   CK_INT(r2.failure_value, -5);
   CK(r3.force_failure == false);

   fsim_clear_error_table();
}

static void test_recurring(void) {
   char fn[] = "recur_fn";
   fsim_add_error(fn, FSIM_CALL_OCC_RECURRING, 2, -7);   // fail every 2nd call

   mute();
   Failsim_Result r1 = fsim_check_failure("f.c", "recur_fn");
   Failsim_Result r2 = fsim_check_failure("f.c", "recur_fn");
   Failsim_Result r3 = fsim_check_failure("f.c", "recur_fn");
   Failsim_Result r4 = fsim_check_failure("f.c", "recur_fn");
   unmute();

   CK(r1.force_failure == false);
   CK(r2.force_failure == true);
   CK_INT(r2.failure_value, -7);
   CK(r3.force_failure == false);
   CK(r4.force_failure == true);

   fsim_clear_error_table();
}

static void test_unknown(void) {
   // a function with no registered error never fails
   Failsim_Result r = fsim_check_failure("f.c", "never_registered");
   CK(r.force_failure == false);
}

static void test_reset_and_clear(void) {
   char fn[] = "reset_fn";
   fsim_add_error(fn, FSIM_CALL_OCC_SINGLE, 1, -9);   // fail on call 1

   mute();
   Failsim_Result r1 = fsim_check_failure("f.c", "reset_fn");
   unmute();
   CK(r1.force_failure == true);

   // resetting the call count makes call 1 fail again
   fsim_reset_callct(fn);
   mute();
   Failsim_Result r2 = fsim_check_failure("f.c", "reset_fn");
   unmute();
   CK(r2.force_failure == true);
   fsim_clear_error_table();

   // clearing errors for a function disables its failures
   char a[] = "a_fn";
   fsim_add_error(a, FSIM_CALL_OCC_SINGLE, 1, -1);
   fsim_clear_errors_for_func(a);
   Failsim_Result r3 = fsim_check_failure("f.c", "a_fn");
   CK(r3.force_failure == false);
   fsim_clear_error_table();
}

static void test_injectors(void) {
   failsim_enabled = true;

   char intf[] = "intf";
   fsim_add_error(intf, FSIM_CALL_OCC_SINGLE, 1, -99);
   mute();
   int r = fsim_int_injector(1, "f.c", "intf");    // call 1: forced
   unmute();
   CK_INT(r, -99);
   // call 2 is not forced; the injector returns 0 when no failure is injected
   int r2 = fsim_int_injector(1, "f.c", "intf");
   CK_INT(r2, 0);
   fsim_clear_error_table();

   char errf[] = "errf";
   fsim_add_error(errf, FSIM_CALL_OCC_SINGLE, 1, -3);
   mute();
   Error_Info * e = fsim_errinfo_injector(NULL, "f.c", "errf");
   unmute();
   CK(e != NULL);
   CK_INT(ERRINFO_STATUS(e), -3);
   errinfo_free(e);

   // no registered error -> no injected Error_Info
   Error_Info * e2 = fsim_errinfo_injector(NULL, "f.c", "errf_none");
   CK(e2 == NULL);

   fsim_clear_error_table();
   failsim_enabled = false;
}

int main(int argc, char ** argv) {
   test_single();
   test_recurring();
   test_unknown();
   test_reset_and_clear();
   test_injectors();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
