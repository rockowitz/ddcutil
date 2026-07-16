/** @file test_sysfs_util.c
 *
 *  Standalone unit tests for src/util/sysfs_util.c.
 *
 *  A /sys attribute is just a one-line text file in a directory, so the readers
 *  (read_sysfs_attr, read_sysfs_attr_w_default, read_sysfs_attr_w_default_r) are
 *  exercised against a temporary directory holding synthetic attribute files.
 *  get_rpath_basename is checked against a real path.  The rpt_attr_* reporting
 *  functions require a live /sys tree and are not exercised.
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

#include "util/sysfs_util.h"

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

// Writes content to dir/name.
static void write_attr(const char * dir, const char * name, const char * content) {
   char fn[512];
   snprintf(fn, sizeof(fn), "%s/%s", dir, name);
   FILE * f = fopen(fn, "w");
   if (f) { fputs(content, f); fclose(f); }
}

int main(int argc, char ** argv) {
   char dir[] = "/tmp/test_sysfs_XXXXXX";
   if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
   write_attr(dir, "name", "widget\n");
   write_attr(dir, "multi", "line1\nline2\n");

   // read_sysfs_attr returns the first line, trailing newline stripped
   char * v = read_sysfs_attr(dir, "name", false);
   CK_STR(v, "widget");
   free(v);
   v = read_sysfs_attr(dir, "multi", false);
   CK_STR(v, "line1");
   free(v);

   // a missing attribute yields NULL
   v = read_sysfs_attr(dir, "absent", false);
   CK(v == NULL);

   // _w_default returns the value when present, the default when not
   v = read_sysfs_attr_w_default(dir, "name", "DEFAULT", false);
   CK_STR(v, "widget");
   free(v);
   v = read_sysfs_attr_w_default(dir, "absent", "DEFAULT", false);
   CK_STR(v, "DEFAULT");
   free(v);

   // _w_default_r fills a caller buffer
   char buf[64];
   read_sysfs_attr_w_default_r(dir, "name", "DEFAULT", buf, sizeof(buf), false);
   CK_STR(buf, "widget");
   read_sysfs_attr_w_default_r(dir, "absent", "DEFAULT", buf, sizeof(buf), false);
   CK_STR(buf, "DEFAULT");

   // clean up
   char cmd[128];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
   int n = system(cmd); (void) n;

   // get_rpath_basename: realpath then basename (no symlinks in a fresh temp file)
   char path[] = "/tmp/test_rpath_XXXXXX";
   int fd = mkstemp(path);
   if (fd >= 0) close(fd);
   char * expected = g_path_get_basename(path);
   char * base = get_rpath_basename(path);
   CK_STR(base, expected);
   free(base);
   g_free(expected);
   CK(get_rpath_basename("/no/such/path/xyz") == NULL);   // realpath fails
   unlink(path);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
