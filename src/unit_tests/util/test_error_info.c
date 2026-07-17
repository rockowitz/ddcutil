/** @file test_error_info.c
 *
 *  Standalone unit tests for src/util/error_info.c.
 *
 *  Error_Info is a heap record carrying a status code, a function name, an
 *  optional detail string, and an array of contributing "cause" records.  These
 *  tests exercise construction, mutation, cause accumulation, the deep copy, and
 *  the summary/causes-string formatting -- both with the default status-code
 *  formatting (numeric / "unknown") and with a status-name function registered
 *  via errinfo_init().
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/error_info.h"

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

static void test_new(void) {
   Error_Info * e = errinfo_new(-5, "myfunc", "detail %d", 42);
   CK(memcmp(e->marker, ERROR_INFO_MARKER, 4) == 0);
   CK_INT(e->status_code, -5);
   CK_STR(e->func, "myfunc");
   CK_STR(e->detail, "detail 42");
   CK_INT(e->cause_ct, 0);
   errinfo_free(e);

   // NULL detail leaves detail unset
   Error_Info * e2 = errinfo_new(3, "f", NULL);
   CK(e2->detail == NULL);
   CK_INT(e2->status_code, 3);
   errinfo_free(e2);

   // ERRINFO_STATUS macro
   Error_Info * enull = NULL;
   CK_INT(ERRINFO_STATUS(enull), 0);
   Error_Info * e3 = errinfo_new(7, "f", NULL);
   CK_INT(ERRINFO_STATUS(e3), 7);
   errinfo_free(e3);
}

static void test_mutate(void) {
   Error_Info * e = errinfo_new(1, "f", "orig");
   errinfo_set_status(e, -10);
   CK_INT(e->status_code, -10);
   errinfo_set_detail(e, "replaced %s", "detail");
   CK_STR(e->detail, "replaced detail");
   errinfo_free(e);
}

static void test_causes(void) {
   Error_Info * parent = errinfo_new(1, "parent", NULL);
   Error_Info * c1 = errinfo_new(2, "c1", NULL);
   Error_Info * c2 = errinfo_new(2, "c2", NULL);
   Error_Info * c3 = errinfo_new(3, "c3", NULL);
   errinfo_add_cause(parent, c1);
   errinfo_add_cause(parent, c2);
   errinfo_add_cause(parent, c3);
   CK_INT(parent->cause_ct, 3);
   CK(parent->causes[0] == c1);
   CK(parent->causes[2] == c3);

   // not all causes share a status (2,2,3)
   CK(errinfo_all_causes_same_status(parent, 0) == false);
   CK(errinfo_all_causes_same_status(parent, 2) == false);
   errinfo_free(parent);   // frees c1, c2, c3 as well

   // all causes share status 2
   Error_Info * p2 = errinfo_new(1, "p2", NULL);
   errinfo_add_cause(p2, errinfo_new(2, "a", NULL));
   errinfo_add_cause(p2, errinfo_new(2, "b", NULL));
   CK(errinfo_all_causes_same_status(p2, 0) == true);
   CK(errinfo_all_causes_same_status(p2, 2) == true);
   CK(errinfo_all_causes_same_status(p2, 3) == false);
   errinfo_free(p2);

   // NULL and no-cause instances are not "all same"
   CK(errinfo_all_causes_same_status(NULL, 0) == false);
   Error_Info * leaf = errinfo_new(1, "leaf", NULL);
   CK(errinfo_all_causes_same_status(leaf, 0) == false);
   errinfo_free(leaf);

   // errinfo_new_with_cause records exactly one cause
   Error_Info * cause = errinfo_new(9, "cause", NULL);
   Error_Info * wrapped = errinfo_new_with_cause(1, cause, "wrapper", "wrapped");
   CK_INT(wrapped->cause_ct, 1);
   CK(wrapped->causes[0] == cause);
   errinfo_free(wrapped);
}

static void test_causes_string(void) {
   // consecutive identical statuses collapse to "code(count)"
   Error_Info * parent = errinfo_new(1, "p", NULL);
   errinfo_add_cause(parent, errinfo_new(2, "a", NULL));
   errinfo_add_cause(parent, errinfo_new(2, "b", NULL));
   errinfo_add_cause(parent, errinfo_new(3, "c", NULL));
   char * cs = errinfo_causes_string(parent);
   CK_STR(cs, "2(2), 3");
   free(cs);
   errinfo_free(parent);

   // single cause
   Error_Info * p2 = errinfo_new(1, "p", NULL);
   errinfo_add_cause(p2, errinfo_new(5, "a", NULL));
   char * cs2 = errinfo_causes_string(p2);
   CK_STR(cs2, "5");
   free(cs2);
   errinfo_free(p2);

   // no causes -> empty string
   Error_Info * leaf = errinfo_new(1, "p", NULL);
   char * cs3 = errinfo_causes_string(leaf);
   CK_STR(cs3, "");
   free(cs3);
   errinfo_free(leaf);
}

static void test_copy(void) {
   Error_Info * orig = errinfo_new(1, "orig", "od");
   errinfo_add_cause(orig, errinfo_new(2, "c", "cd"));
   Error_Info * cp = errinfo_copy(orig);

   CK_INT(cp->status_code, 1);
   CK_STR(cp->func, "orig");
   CK_STR(cp->detail, "od");
   CK_INT(cp->cause_ct, 1);
   CK(cp->causes[0] != orig->causes[0]);           // deep copy
   CK_INT(cp->causes[0]->status_code, 2);
   CK_STR(cp->causes[0]->func, "c");

   errinfo_free(orig);
   errinfo_free(cp);
}

static void test_summary_default(void) {
   // default: no name function registered -> "unknown"
   Error_Info * e = errinfo_new(-5, "foo", NULL);
   CK_STR(errinfo_summary(e), "Error_Info[unknown in foo]");
   errinfo_free(e);

   Error_Info * parent = errinfo_new(1, "p", NULL);
   errinfo_add_cause(parent, errinfo_new(2, "a", NULL));
   errinfo_add_cause(parent, errinfo_new(2, "b", NULL));
   errinfo_add_cause(parent, errinfo_new(3, "c", NULL));
   CK_STR(errinfo_summary(parent), "Error_Info[unknown in p, causes: 2(2), 3]");
   errinfo_free(parent);

   CK_STR(errinfo_summary(NULL), "NULL");
}

// Simple status-code name function used to exercise the registered-name path.
static char * test_status_name(int code) {
   static char buf[20];
   snprintf(buf, sizeof(buf), "RC[%d]", code);
   return buf;
}

static void test_summary_named(void) {
   errinfo_init(test_status_name, test_status_name);

   Error_Info * e = errinfo_new(-5, "foo", NULL);
   CK_STR(errinfo_summary(e), "Error_Info[RC[-5] in foo]");
   errinfo_free(e);

   Error_Info * parent = errinfo_new(1, "p", NULL);
   errinfo_add_cause(parent, errinfo_new(2, "a", NULL));
   errinfo_add_cause(parent, errinfo_new(3, "b", NULL));
   char * cs = errinfo_causes_string(parent);
   CK_STR(cs, "RC[2], RC[3]");
   free(cs);
   errinfo_free(parent);
}

int main(int argc, char ** argv) {
   test_new();
   test_mutate();
   test_causes();
   test_causes_string();
   test_copy();
   test_summary_default();
   test_summary_named();       // registers a name function; must run last

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
