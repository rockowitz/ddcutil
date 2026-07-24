/** @file test_persistent_capabilities.c
 *
 *  Standalone unit tests for src/vcp/persistent_capabilities.c: the
 *  capabilities-string disk cache keyed by Monitor_Model_Key.
 *
 *  This module does real file I/O (via xdg_cache_home_file(), which
 *  respects $XDG_CACHE_HOME).  To avoid touching the real user's
 *  ~/.cache/ddcutil/capabilities file, main() below points $XDG_CACHE_HOME
 *  at a freshly created temporary directory before calling any function in
 *  this module, so the cache file used here lives only under that
 *  directory and is never the real user's file.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libvcp unit test: it links the internal libvcp/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/file_util.h"

#include "base/monitor_model_key.h"

#include "vcp/persistent_capabilities.h"

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

#define CK_STR_CONTAINS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected to contain \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _n); } \
} while(0)


static void test_disabled_by_default(void) {
   Monitor_Model_Key mmk = mmk_value("ACM", "UnitTestModel", 0x1234);
   char * got = get_persistent_capabilities(&mmk);
   CK(got == NULL);
}


static void test_enable_disable_returns_old_value(void) {
   bool old1 = enable_capabilities_cache(true);
   CK(old1 == false);

   bool old2 = enable_capabilities_cache(true);
   CK(old2 == true);

   bool old3 = enable_capabilities_cache(false);
   CK(old3 == true);

   // leave enabled for the remaining tests
   enable_capabilities_cache(true);
}


static void test_capabilities_cache_file_name(void) {
   char * fn = capabilities_cache_file_name();
   CK(fn != NULL);
   if (fn) {
      CK_STR_CONTAINS(fn, "ddcutil");
      CK_STR_CONTAINS(fn, "capabilities");
      free(fn);
   }
}


static void test_set_and_get_persistent_capabilities(void) {
   Monitor_Model_Key mmk = mmk_value("ACM", "UnitTestModel", 0x1234);
   const char * caps = "(prot(monitor)type(LCD)model(UnitTestModel)vcp(10 12))";

   set_persistent_capabilites(&mmk, caps);

   char * got = get_persistent_capabilities(&mmk);
   CK_STR(got, caps);

   // the cache file must actually have been written to disk
   char * fn = capabilities_cache_file_name();
   CK(fn != NULL && regular_file_exists(fn));
   free(fn);

   // a different model must not match
   Monitor_Model_Key other = mmk_value("ACM", "OtherModel", 0x5678);
   CK(get_persistent_capabilities(&other) == NULL);
}


static void test_non_unique_model_rejected(void) {
   // "LG IPS FULLHD" with product_code 0 is treated as a non-unique model
   // identifier (LG reuses this generic name across distinct models), so
   // lookups/saves for it must be refused rather than risk returning the
   // wrong monitor's capabilities.
   Monitor_Model_Key mmk = mmk_value("LEN", "LG IPS FULLHD", 0);

   set_persistent_capabilites(&mmk, "(vcp(10))");
   char * got = get_persistent_capabilities(&mmk);
   CK(got == NULL);
}


static void test_delete_capabilities_file(void) {
   char * fn = capabilities_cache_file_name();
   CK(fn != NULL && regular_file_exists(fn));   // written by an earlier test

   delete_capabilities_file();
   CK(!regular_file_exists(fn));

   // must not crash when the file is already gone
   delete_capabilities_file();
   CK(true);

   free(fn);
}


int main(int argc, char ** argv) {
   char tmpl[] = "/tmp/ddcutil_test_xdg_cache_XXXXXX";
   char * tmpdir = mkdtemp(tmpl);
   if (!tmpdir) {
      fprintf(stderr, "mkdtemp() failed: %s\n", strerror(errno));
      return 2;
   }
   setenv("XDG_CACHE_HOME", tmpdir, 1);

   test_disabled_by_default();
   test_enable_disable_returns_old_value();
   test_capabilities_cache_file_name();
   test_set_and_get_persistent_capabilities();
   test_non_unique_model_rejected();
   test_delete_capabilities_file();

   terminate_persistent_capabilities();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
