/** @file test_trace_control.c
 *
 *  Standalone unit tests for src/base/trace_control.c: the traced-function /
 *  api-call / callstack-call registries (which admit only functions registered
 *  with RTTI), the traced-file registry, the trace-class name lookup, and the
 *  trace-group flag setters.
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
#include "base/rtti.h"
#include "base/trace_control.h"

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

// A stand-in function to register with RTTI so the registries will admit it.
static void sample_traced_func(void) {}

static void test_function_registries(void) {
   rtti_func_name_table_add((void *) sample_traced_func, "sample_traced_func");

   // a registered function can be added and is then reported as traced
   CK(add_traced_function("sample_traced_func") == true);
   CK(is_traced_function("sample_traced_func") == true);
   // an unregistered function is rejected and never traced
   CK(add_traced_function("unregistered_func_xyz") == false);
   CK(is_traced_function("unregistered_func_xyz") == false);

   CK(add_traced_api_call("sample_traced_func") == true);
   CK(is_traced_api_call("sample_traced_func") == true);
   CK(is_traced_api_call("unregistered_func_xyz") == false);

   CK(add_traced_callstack_call("sample_traced_func") == true);
   CK(is_traced_callstack_call("sample_traced_func") == true);
   CK(is_traced_callstack_call("unregistered_func_xyz") == false);
}

static void test_file_registry(void) {
   add_traced_file("core.c");
   CK(is_traced_file("core.c") == true);
   CK(is_traced_file("/some/path/core.c") == true);   // matched by basename
   CK(is_traced_file("other.c") == false);

   // a name without a .c suffix is stored with ".c" appended
   add_traced_file("mymod");
   CK(is_traced_file("mymod.c") == true);
   CK(is_traced_file("mymod") == false);              // lookup uses the basename as-is

   CK(is_traced_file(NULL) == false);
}

static void test_trace_class_name(void) {
   CK_INT(trace_class_name_to_value("BASE"), DDCA_TRC_BASE);
   CK_INT(trace_class_name_to_value("I2C"),  DDCA_TRC_I2C);
   CK_INT(trace_class_name_to_value("i2c"),  DDCA_TRC_I2C);     // case-insensitive
   CK_INT(trace_class_name_to_value("bogus"), DDCA_TRC_NONE);   // default
}

static void test_trace_groups(void) {
   // add_trace_groups ORs flags into the global trace_levels
   trace_levels = DDCA_TRC_NONE;
   add_trace_groups(DDCA_TRC_BASE | DDCA_TRC_I2C);
   CK_INT(trace_levels, (DDCA_TRC_BASE | DDCA_TRC_I2C));
   add_trace_groups(DDCA_TRC_DDC);
   CK_INT(trace_levels, (DDCA_TRC_BASE | DDCA_TRC_I2C | DDCA_TRC_DDC));
   trace_levels = DDCA_TRC_NONE;   // restore
}

int main(int argc, char ** argv) {
   test_function_registries();
   test_file_registry();
   test_trace_class_name();
   test_trace_groups();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
