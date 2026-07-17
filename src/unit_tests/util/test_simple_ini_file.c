/** @file test_simple_ini_file.c
 *
 *  Standalone unit tests for src/util/simple_ini_file.c: loading an INI file
 *  (validating section/key pairs against an allowed list), retrieving values
 *  case-insensitively, and the -ENOENT result for a missing file.
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

#include "util/simple_ini_file.h"

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
static void write_tmpfile(char * path_out, const char * content) {
   strcpy(path_out, "/tmp/test_ini_XXXXXX");
   int fd = mkstemp(path_out);
   if (fd < 0) { perror("mkstemp"); exit(2); }
   ssize_t n = write(fd, content, strlen(content)); (void) n;
   close(fd);
}

int main(int argc, char ** argv) {
   const char * ini =
      "[sa]\n"
      "k1 = value1\n"
      "k2 = value2\n"
      "\n"
      "[sb]\n"
      "kx = valuex\n";
   char path[64];
   write_tmpfile(path, ini);

   Ini_Valid_Section_Key_Pair valid[] = {
      { "sa", "k1" },
      { "sa", "k2" },
      { "sb", "kx" },
   };
   int validct = sizeof(valid) / sizeof(valid[0]);

   Parsed_Ini_File * parsed = NULL;
   GPtrArray * errs = g_ptr_array_new_with_free_func(g_free);
   int rc = ini_file_load(path, valid, validct, errs, &parsed);
   CK_INT(rc, 0);
   CK(parsed != NULL);
   CK_INT(errs->len, 0);
   CK(memcmp(parsed->marker, PARSED_INI_FILE_MARKER, 4) == 0);

   // values are retrieved by section and key, whitespace around '=' trimmed
   CK_STR(ini_file_get_value(parsed, "sa", "k1"), "value1");
   CK_STR(ini_file_get_value(parsed, "sa", "k2"), "value2");
   CK_STR(ini_file_get_value(parsed, "sb", "kx"), "valuex");

   // lookups are case-insensitive
   CK_STR(ini_file_get_value(parsed, "SA", "K1"), "value1");

   // missing key or section -> NULL
   CK(ini_file_get_value(parsed, "sa", "nokey") == NULL);
   CK(ini_file_get_value(parsed, "nosec", "k1") == NULL);

   ini_file_free(parsed);
   g_ptr_array_free(errs, TRUE);
   unlink(path);

   // a nonexistent file -> -ENOENT
   Parsed_Ini_File * parsed2 = NULL;
   GPtrArray * errs2 = g_ptr_array_new_with_free_func(g_free);
   int rc2 = ini_file_load("/no/such/ini/file", valid, validct, errs2, &parsed2);
   CK_INT(rc2, -ENOENT);
   g_ptr_array_free(errs2, TRUE);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
