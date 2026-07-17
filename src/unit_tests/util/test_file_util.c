/** @file test_file_util.c
 *
 *  Standalone unit tests for src/util/file_util.c.
 *
 *  These tests create temporary files and directories and check the file
 *  existence predicates, the whole-file and first/last-line readers, the binary
 *  reader, the inode accessors, fd-to-name resolution, recursive mkdir, the
 *  Error_Info line reader, and the filtered line reader.
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
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/file_util.h"

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

// Writes content to a fresh temp file, returning its path in path_out.
static void write_tmpfile(char * path_out, const void * content, size_t len) {
   strcpy(path_out, "/tmp/test_fu_XXXXXX");
   int fd = mkstemp(path_out);
   if (fd < 0) { perror("mkstemp"); exit(2); }
   if (len > 0) { ssize_t n = write(fd, content, len); (void) n; }
   close(fd);
}

static void test_exists(void) {
   char path[64];
   write_tmpfile(path, "hi", 2);
   CK(any_file_exists(path) == true);
   CK(regular_file_exists(path) == true);
   CK(directory_exists(path) == false);
   unlink(path);

   char dir[] = "/tmp/test_fu_dir_XXXXXX";
   mkdtemp(dir);
   CK(any_file_exists(dir) == true);
   CK(directory_exists(dir) == true);
   CK(regular_file_exists(dir) == false);
   rmdir(dir);

   CK(any_file_exists("/no/such/path/here") == false);
   CK(regular_file_exists("/no/such/path/here") == false);
   CK(directory_exists("/no/such/path/here") == false);
}

static void test_readers(void) {
   char path[64];

   // first line, trailing newline stripped
   write_tmpfile(path, "line1\nline2\n", 12);
   char * first = file_get_first_line(path, false);
   CK_STR(first, "line1");
   free(first);
   unlink(path);

   // whole file as one string, content preserved verbatim
   write_tmpfile(path, "abc\ndef\n", 8);
   char * whole = read_file_single_string(path, false);
   CK_STR(whole, "abc\ndef\n");
   free(whole);
   unlink(path);

   // binary read preserves every byte, including an embedded NUL
   unsigned char bytes[] = {0x00, 0x01, 0x02, 0xff};
   write_tmpfile(path, bytes, sizeof(bytes));
   GByteArray * gba = read_binary_file(path, 0, false);
   CK(gba != NULL);
   CK_INT(gba->len, 4);
   CK(gba->data[0] == 0x00 && gba->data[1] == 0x01 &&
      gba->data[2] == 0x02 && gba->data[3] == 0xff);
   g_byte_array_free(gba, TRUE);
   unlink(path);

   // last N lines
   write_tmpfile(path, "l1\nl2\nl3\nl4\nl5\n", 15);
   GPtrArray * last = NULL;
   int rc = file_get_last_lines(path, 2, &last, false);
   CK_INT(rc, 2);
   CK_INT(last->len, 2);
   CK_STR((char *) g_ptr_array_index(last, 0), "l4");
   CK_STR((char *) g_ptr_array_index(last, 1), "l5");
   g_ptr_array_free(last, TRUE);
   unlink(path);
}

static void test_getlines_errinfo(void) {
   char path[64];
   write_tmpfile(path, "one\ntwo\n", 8);
   GPtrArray * lines = g_ptr_array_new_with_free_func(g_free);
   Error_Info * ei = file_getlines_errinfo(path, lines);
   CK(ei == NULL);
   CK_INT(lines->len, 2);
   CK_STR((char *) g_ptr_array_index(lines, 0), "one");
   g_ptr_array_free(lines, TRUE);
   unlink(path);

   // missing file -> Error_Info carrying -ENOENT
   GPtrArray * lines2 = g_ptr_array_new_with_free_func(g_free);
   Error_Info * ei2 = file_getlines_errinfo("/no/such/file", lines2);
   CK(ei2 != NULL);
   CK_INT(ei2->status_code, -ENOENT);
   errinfo_free(ei2);
   g_ptr_array_free(lines2, TRUE);
}

static void test_inode_and_fd(void) {
   char path[64];
   write_tmpfile(path, "x", 1);

   long ino1 = get_inode_by_fn(path);
   long ino2 = get_inode_by_fn(path);
   CK(ino1 > 0);
   CK_INT(ino1, ino2);                     // stable across calls

   int fd = open(path, O_RDONLY);
   CK(fd >= 0);
   CK_INT(get_inode_by_fd(fd), ino1);       // same file, same inode

   char * fn = filename_for_fd_t(fd);        // resolve fd back to its path
   CK_STR(fn, path);
   close(fd);

   CK_INT(get_inode_by_fn("/no/such/file"), -1);
   unlink(path);
}

static void test_rek_mkdir(void) {
   char base[] = "/tmp/test_fu_mk_XXXXXX";
   mkdtemp(base);
   char * nested = g_strdup_printf("%s/a/b/c", base);
   int rc = rek_mkdir(nested, NULL);
   CK_INT(rc, 0);
   CK(directory_exists(nested) == true);

   // clean up the created tree
   char * cmd = g_strdup_printf("rm -rf %s", base);
   int n = system(cmd); (void) n;
   g_free(cmd);
   g_free(nested);
}

static void test_filter(void) {
   char path[64];
   write_tmpfile(path, "apple\nbanana\ncherry\napricot\n", 28);

   // keep only lines containing "ap"
   char * terms[] = { "ap", NULL };
   GPtrArray * arr = g_ptr_array_new_with_free_func(g_free);
   int rc = read_file_with_filter(arr, path, terms, false, 0, false);
   CK_INT(rc, 4);                          // lines read from the file
   CK_INT(arr->len, 2);                    // lines surviving the filter
   CK_STR((char *) g_ptr_array_index(arr, 0), "apple");
   CK_STR((char *) g_ptr_array_index(arr, 1), "apricot");
   g_ptr_array_free(arr, TRUE);

   // no filter, limit to the first 2 lines
   GPtrArray * arr2 = g_ptr_array_new_with_free_func(g_free);
   rc = read_file_with_filter(arr2, path, NULL, false, 2, false);
   CK_INT(arr2->len, 2);
   CK_STR((char *) g_ptr_array_index(arr2, 0), "apple");
   CK_STR((char *) g_ptr_array_index(arr2, 1), "banana");
   g_ptr_array_free(arr2, TRUE);

   unlink(path);
}

int main(int argc, char ** argv) {
   test_exists();
   test_readers();
   test_getlines_errinfo();
   test_inode_and_fd();
   test_rek_mkdir();
   test_filter();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
