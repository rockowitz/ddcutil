/** @file test_xdg_util.c
 *
 *  Standalone unit tests for src/util/xdg_util.c.
 *
 *  The XDG base-directory functions are pure functions of the XDG_* and HOME
 *  environment variables, so the tests set those variables and check the
 *  resulting directory, dirs, path, and file-name strings, plus find_xdg_config_file
 *  against a temporary config tree.  Note the *_home_dir functions append a
 *  trailing '/'.
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
#include <unistd.h>

#include "util/xdg_util.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

// Compares a freshly-allocated string to expected, then frees it.
#define CK_STR_FREE(actual, expected) do { \
   total++; \
   char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
   free(_a); \
} while(0)

int main(int argc, char ** argv) {
   setenv("HOME", "/home/tester", 1);

   // home dirs: env value wins (trailing '/' appended), else $HOME/<subdir>/
   setenv("XDG_CONFIG_HOME", "/tmp/cfg", 1);
   CK_STR_FREE(xdg_config_home_dir(), "/tmp/cfg/");
   setenv("XDG_CONFIG_HOME", "/tmp/cfg/", 1);          // already has slash
   CK_STR_FREE(xdg_config_home_dir(), "/tmp/cfg/");
   unsetenv("XDG_CONFIG_HOME");
   CK_STR_FREE(xdg_config_home_dir(), "/home/tester/.config/");

   unsetenv("XDG_DATA_HOME");
   CK_STR_FREE(xdg_data_home_dir(), "/home/tester/.local/share/");
   setenv("XDG_DATA_HOME", "/x/data", 1);
   CK_STR_FREE(xdg_data_home_dir(), "/x/data/");

   unsetenv("XDG_CACHE_HOME");
   CK_STR_FREE(xdg_cache_home_dir(), "/home/tester/.cache/");

   unsetenv("XDG_STATE_HOME");
   CK_STR_FREE(xdg_state_home_dir(), "/home/tester/.local/state/");

   // dirs: env value verbatim, else the spec default
   unsetenv("XDG_DATA_DIRS");
   CK_STR_FREE(xdg_data_dirs(), "/usr/local/share/:/usr/share");
   setenv("XDG_DATA_DIRS", "/a:/b", 1);
   CK_STR_FREE(xdg_data_dirs(), "/a:/b");

   unsetenv("XDG_CONFIG_DIRS");
   CK_STR_FREE(xdg_config_dirs(), "/etc/xdg");

   // home file: <config home>/<app>/<fn>
   setenv("XDG_CONFIG_HOME", "/tmp/cfg", 1);
   CK_STR_FREE(xdg_config_home_file("ddcutil", "ddcutilrc"), "/tmp/cfg/ddcutil/ddcutilrc");

   // path: config home dir, then the config dirs
   unsetenv("XDG_CONFIG_DIRS");
   CK_STR_FREE(xdg_config_path(), "/tmp/cfg/:/etc/xdg");

   // find_xdg_config_file against a temporary config tree
   char dir[] = "/tmp/test_xdg_XXXXXX";
   if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
   char * appdir = g_strdup_printf("%s/ddcutil", dir);
   g_mkdir_with_parents(appdir, 0700);
   char * fn = g_strdup_printf("%s/ddcutilrc", appdir);
   FILE * f = fopen(fn, "w");
   if (f) { fputs("x\n", f); fclose(f); }
   setenv("XDG_CONFIG_HOME", dir, 1);
   setenv("XDG_CONFIG_DIRS", "/nonexistent-xdg-dir", 1);

   char * found = find_xdg_config_file("ddcutil", "ddcutilrc");
   CK(found != NULL && g_str_has_suffix(found, "ddcutil/ddcutilrc"));
   free(found);
   CK(find_xdg_config_file("ddcutil", "no_such_file") == NULL);

   unlink(fn);
   rmdir(appdir);
   rmdir(dir);
   g_free(appdir);
   g_free(fn);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
