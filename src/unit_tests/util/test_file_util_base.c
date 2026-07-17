/** @file test_file_util_base.c
 *
 *  Standalone unit tests for the functions in src/util/file_util_base.c.
 *
 *  file_getlines() reads a text file into a GPtrArray, so the tests write
 *  temporary files with known contents and check the returned lines and count.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/file_util_base.h"

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
   if (strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, _a, _e); } \
} while(0)

// Writes `content` to a fresh temp file; returns its path in path_out (caller unlinks).
static void write_tmpfile(char * path_out, const char * content) {
   strcpy(path_out, "/tmp/test_fgl_XXXXXX");
   int fd = mkstemp(path_out);
   if (fd < 0) { perror("mkstemp"); exit(2); }
   if (content && *content) {
      ssize_t n = write(fd, content, strlen(content));
      (void) n;
   }
   close(fd);
}

int main(int argc, char ** argv) {
   char path[64];

   // three lines, each terminated by a newline: newlines stripped
   write_tmpfile(path, "line1\nline2\nline3\n");
   GPtrArray * a = g_ptr_array_new_with_free_func(g_free);
   CK_INT(file_getlines(path, a, false), 3);
   CK_INT(a->len, 3);
   CK_STR((char *) g_ptr_array_index(a, 0), "line1");
   CK_STR((char *) g_ptr_array_index(a, 2), "line3");
   g_ptr_array_free(a, TRUE);
   unlink(path);

   // last line without a trailing newline is still read
   write_tmpfile(path, "abc\ndef");
   GPtrArray * b = g_ptr_array_new_with_free_func(g_free);
   CK_INT(file_getlines(path, b, false), 2);
   CK_STR((char *) g_ptr_array_index(b, 1), "def");
   g_ptr_array_free(b, TRUE);
   unlink(path);

   // trailing whitespace (not just the newline) is stripped
   write_tmpfile(path, "hello   \nworld\t\n");
   GPtrArray * c = g_ptr_array_new_with_free_func(g_free);
   CK_INT(file_getlines(path, c, false), 2);
   CK_STR((char *) g_ptr_array_index(c, 0), "hello");
   CK_STR((char *) g_ptr_array_index(c, 1), "world");
   g_ptr_array_free(c, TRUE);
   unlink(path);

   // empty file -> 0 lines
   write_tmpfile(path, "");
   GPtrArray * d = g_ptr_array_new_with_free_func(g_free);
   CK_INT(file_getlines(path, d, false), 0);
   CK_INT(d->len, 0);
   g_ptr_array_free(d, TRUE);
   unlink(path);

   // lines are appended, not replaced
   write_tmpfile(path, "new1\nnew2\n");
   GPtrArray * e = g_ptr_array_new_with_free_func(g_free);
   g_ptr_array_add(e, g_strdup("existing"));
   CK_INT(file_getlines(path, e, false), 2);   // returns count added, not total
   CK_INT(e->len, 3);                          // pre-existing entry retained
   CK_STR((char *) g_ptr_array_index(e, 0), "existing");
   CK_STR((char *) g_ptr_array_index(e, 1), "new1");
   g_ptr_array_free(e, TRUE);
   unlink(path);

   // non-existent file -> negative errno
   GPtrArray * f = g_ptr_array_new_with_free_func(g_free);
   CK_INT(file_getlines("/no/such/file/at/all", f, false), -ENOENT);
   CK_INT(f->len, 0);
   g_ptr_array_free(f, TRUE);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
