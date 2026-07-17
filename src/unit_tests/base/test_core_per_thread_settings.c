/** @file test_core_per_thread_settings.c
 *
 *  Standalone unit tests for src/base/core_per_thread_settings.c: the per-thread
 *  output settings (fout/ferr streams and output level), the default-settings
 *  propagation to new threads, and output_level_name().
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

#include "public/ddcutil_types.h"
#include "base/core_per_thread_settings.h"
#include "util/linux_basic_util.h"    // get_thread_id

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

static void test_defaults_and_identity(void) {
   Thread_Output_Settings * s1 = get_thread_settings();
   CK(s1 != NULL);
   CK(get_thread_settings() == s1);          // cached per thread
   CK(s1->tid == get_thread_id());

   // fresh thread inherits the built-in defaults
   CK(fout() == stdout);
   CK(ferr() == stderr);
   CK(get_output_level() == DDCA_OL_NORMAL);
}

static void test_output_level(void) {
   DDCA_Output_Level orig = get_output_level();
   CK(set_output_level(DDCA_OL_VERBOSE) == orig);       // returns previous
   CK(get_output_level() == DDCA_OL_VERBOSE);
   CK(set_output_level(DDCA_OL_TERSE) == DDCA_OL_VERBOSE);
   CK(get_output_level() == DDCA_OL_TERSE);
   set_output_level(orig);                              // restore
}

static void test_fout_ferr(void) {
   FILE * tf = tmpfile();
   CK(tf != NULL);

   set_fout(tf);
   CK(fout() == tf);
   set_ferr(tf);
   CK(ferr() == tf);

   set_fout_to_default();
   CK(fout() == stdout);
   set_ferr(stderr);
   CK(ferr() == stderr);

   if (tf) fclose(tf);
}

static void test_output_level_name(void) {
   CK_STR(output_level_name(DDCA_OL_TERSE),   "Terse");
   CK_STR(output_level_name(DDCA_OL_NORMAL),  "Normal");
   CK_STR(output_level_name(DDCA_OL_VERBOSE), "Verbose");
   CK_STR(output_level_name(DDCA_OL_VV),      "Very Verbose");
}

typedef struct {
   Thread_Output_Settings * settings;
   DDCA_Output_Level        output_level;
   intmax_t                 tid;
} Thread_Probe;

static gpointer thread_func(gpointer data) {
   Thread_Probe * p = data;
   p->settings     = get_thread_settings();
   p->output_level = get_output_level();
   p->tid          = get_thread_id();
   return NULL;
}

static void test_per_thread_isolation(void) {
   Thread_Output_Settings * main_settings = get_thread_settings();
   intmax_t main_tid = get_thread_id();

   // the default set here should be inherited by a thread created afterward
   set_default_thread_output_level(DDCA_OL_TERSE);

   Thread_Probe probe = {0};
   GThread * t = g_thread_new("probe", thread_func, &probe);
   g_thread_join(t);

   CK(probe.settings != NULL);
   CK(probe.settings != main_settings);          // distinct per-thread settings
   CK(probe.tid != main_tid);
   CK(probe.output_level == DDCA_OL_TERSE);       // picked up the new default

   // the main thread's own settings are unchanged by the default change
   CK(get_output_level() == DDCA_OL_NORMAL);
}

int main(int argc, char ** argv) {
   test_defaults_and_identity();
   test_output_level();
   test_fout_ferr();
   test_output_level_name();
   test_per_thread_isolation();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
