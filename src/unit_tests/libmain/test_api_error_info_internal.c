/** @file test_api_error_info_internal.c
 *
 *  Standalone unit tests for src/libmain/api_error_info_internal.c: the
 *  DDCA_Error_Detail lifecycle (new_ddca_error_detail(),
 *  error_info_to_ddca_detail(), dup_error_detail(), free_error_detail(),
 *  report_error_detail()) and the thread-specific error detail slot
 *  (save/get/free_thread_error_detail()). All pure in-memory data
 *  structure conversions -- no hardware, no library initialization, no
 *  file I/O.
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
#include <unistd.h>

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

#include "util/error_info.h"

#include "libmain/api_error_info_internal.h"

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


static void test_new_ddca_error_detail(void) {
   DDCA_Error_Detail * ed = new_ddca_error_detail(DDCRC_ARG, "bad value: %d", 42);
   CK(ed != NULL);
   if (ed) {
      CK_INT(ed->status_code, DDCRC_ARG);
      CK_STR(ed->detail, "bad value: 42");
      CK_INT(ed->cause_ct, 0);
      free_error_detail(ed);
   }
}


static void test_error_info_to_ddca_detail(void) {
   Error_Info * cause1 = errinfo_new(DDCRC_BAD_DATA, __func__, "cause one");
   Error_Info * cause2 = errinfo_new(DDCRC_NOT_FOUND, __func__, "cause two");
   Error_Info * erec = errinfo_new(DDCRC_ARG, __func__, "top level error");
   errinfo_add_cause(erec, cause1);
   errinfo_add_cause(erec, cause2);

   DDCA_Error_Detail * ed = error_info_to_ddca_detail(erec);
   CK(ed != NULL);
   if (ed) {
      CK_INT(ed->status_code, DDCRC_ARG);
      CK_STR(ed->detail, "top level error");
      CK_INT(ed->cause_ct, 2);
      if (ed->cause_ct == 2) {
         CK_INT(ed->causes[0]->status_code, DDCRC_BAD_DATA);
         CK_STR(ed->causes[0]->detail, "cause one");
         CK_INT(ed->causes[1]->status_code, DDCRC_NOT_FOUND);
         CK_STR(ed->causes[1]->detail, "cause two");
      }
      free_error_detail(ed);
   }

   errinfo_free(erec);
}


static void test_error_info_to_ddca_detail_null(void) {
   DDCA_Error_Detail * ed = error_info_to_ddca_detail(NULL);
   CK(ed == NULL);
}


static void test_dup_error_detail(void) {
   Error_Info * cause = errinfo_new(DDCRC_BAD_DATA, __func__, "cause");
   Error_Info * erec = errinfo_new(DDCRC_ARG, __func__, "original");
   errinfo_add_cause(erec, cause);
   DDCA_Error_Detail * orig = error_info_to_ddca_detail(erec);

   DDCA_Error_Detail * copy = dup_error_detail(orig);
   CK(copy != NULL);
   CK(copy != orig);
   if (copy) {
      CK_INT(copy->status_code, DDCRC_ARG);
      CK_STR(copy->detail, "original");
      CK_INT(copy->cause_ct, 1);
      if (copy->cause_ct == 1) {
         CK(copy->causes[0] != orig->causes[0]);   // deep copy, not aliased
         CK_STR(copy->causes[0]->detail, "cause");
      }
      free_error_detail(copy);
   }

   free_error_detail(orig);
   errinfo_free(erec);
}


static void test_dup_error_detail_null(void) {
   DDCA_Error_Detail * copy = dup_error_detail(NULL);
   CK(copy == NULL);
}


static void test_free_error_detail_null_safe(void) {
   free_error_detail(NULL);   // must not crash
   CK(true);
}


static void test_report_error_detail_smoke(void) {
   DDCA_Error_Detail * ed = new_ddca_error_detail(DDCRC_ARG, "top");
   QUIETLY( report_error_detail(ed, 0) );
   QUIETLY( report_error_detail(NULL, 0) );
   CK(true);   // reaching here without crashing is the test
   free_error_detail(ed);
}


static void test_thread_error_detail_lifecycle(void) {
   // starts unset (or at least, free_thread_error_detail() must be safe
   // to call regardless of prior state)
   free_thread_error_detail();
   CK(get_thread_error_detail() == NULL);

   DDCA_Error_Detail * ed = new_ddca_error_detail(DDCRC_ARG, "thread error");
   save_thread_error_detail(ed);
   CK(get_thread_error_detail() == ed);

   // saving a new one frees the old one automatically
   DDCA_Error_Detail * ed2 = new_ddca_error_detail(DDCRC_NOT_FOUND, "second error");
   save_thread_error_detail(ed2);
   CK(get_thread_error_detail() == ed2);

   free_thread_error_detail();
   CK(get_thread_error_detail() == NULL);

   // must be safe to call again when nothing is set
   free_thread_error_detail();
   CK(true);
}


int main(int argc, char ** argv) {
   test_new_ddca_error_detail();
   test_error_info_to_ddca_detail();
   test_error_info_to_ddca_detail_null();
   test_dup_error_detail();
   test_dup_error_detail_null();
   test_free_error_detail_null_safe();
   test_report_error_detail_smoke();
   test_thread_error_detail_lifecycle();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
