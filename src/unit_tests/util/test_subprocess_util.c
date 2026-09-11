/** @file test_subprocess_util.c
 *
 *  Standalone unit tests for src/util/subprocess_util.c: running a shell command
 *  and collecting its output, the single-line convenience wrapper, and the
 *  command-in-path check.  Uses portable commands (echo, printf) so the output
 *  is deterministic, and a temporary directory placed on $PATH so the
 *  is_command_in_path() cases do not depend on what is installed.
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
#include <sys/stat.h>
#include <unistd.h>

#include "util/subprocess_util.h"

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

int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

   // single-line result is the first line of output
   char * s = execute_shell_cmd_one_line_result("echo hello");
   CK_STR(s, "hello");
   free(s);

   s = execute_shell_cmd_one_line_result("printf 'first\\nsecond\\n'");
   CK_STR(s, "first");
   free(s);

   // collect returns one array entry per output line
   GPtrArray * lines = execute_shell_cmd_collect("printf 'a\\nb\\nc\\n'");
   CK(lines != NULL);
   CK_INT(lines->len, 3);
   CK_STR((char *) g_ptr_array_index(lines, 0), "a");
   CK_STR((char *) g_ptr_array_index(lines, 2), "c");
   g_ptr_array_free(lines, TRUE);

   // command-in-path check

   CK(is_command_in_path("sh") == true);
   CK(is_command_in_path("no_such_command_zqx_999") == false);

   // Degenerate arguments
   CK(is_command_in_path(NULL) == false);
   CK(is_command_in_path("") == false);

   // The remaining cases use a directory of our own on $PATH, so that what
   // they assert does not depend on what happens to be installed.
   char tmpdir[] = "/tmp/test_subprocess_util_XXXXXX";
   if (!mkdtemp(tmpdir)) {
      printf("FAIL  line %-4d  mkdtemp() failed\n", __LINE__);
      failed++; total++;
   }
   else {
      char fq_exec[300], fq_plain[300], fq_subdir[300];
      snprintf(fq_exec,   sizeof(fq_exec),   "%s/zzz_exec",   tmpdir);
      snprintf(fq_plain,  sizeof(fq_plain),  "%s/zzz_plain",  tmpdir);
      snprintf(fq_subdir, sizeof(fq_subdir), "%s/zzz_subdir", tmpdir);

      FILE * f = fopen(fq_exec, "w");
      fputs("#!/bin/sh\nexit 0\n", f);
      fclose(f);
      chmod(fq_exec, 0755);

      f = fopen(fq_plain, "w");        // regular file, deliberately not executable
      fputs("not executable\n", f);
      fclose(f);
      chmod(fq_plain, 0644);

      mkdir(fq_subdir, 0755);          // a directory, executable by mode

      char * saved_path = g_strdup(getenv("PATH"));

      setenv("PATH", tmpdir, 1);
      CK(is_command_in_path("zzz_exec") == true);
      // Regular but not executable.  "which" reported such a file as found;
      // this tests X_OK, which is what the callers need.
      CK(is_command_in_path("zzz_plain") == false);
      // A directory has X_OK set, so access() alone would report it found.
      CK(is_command_in_path("zzz_subdir") == false);

      // Found in a later $PATH segment, not just the first
      char multi[700];
      snprintf(multi, sizeof(multi), "/nonexistent_zqx:%s", tmpdir);
      setenv("PATH", multi, 1);
      CK(is_command_in_path("zzz_exec") == true);

      // An empty segment means the current directory under POSIX, and is
      // deliberately not searched.  cwd is the directory holding zzz_exec.
      char saved_cwd[PATH_MAX];
      if (getcwd(saved_cwd, sizeof(saved_cwd)) && chdir(tmpdir) == 0) {
         setenv("PATH", "", 1);
         CK(is_command_in_path("zzz_exec") == false);
         setenv("PATH", ":", 1);
         CK(is_command_in_path("zzz_exec") == false);
         int rc = chdir(saved_cwd);
         CK_INT(rc, 0);
      }

      // A name containing a separator is tested directly, $PATH not consulted
      setenv("PATH", "/nonexistent_zqx", 1);
      CK(is_command_in_path(fq_exec)   == true);
      CK(is_command_in_path(fq_plain)  == false);
      CK(is_command_in_path(fq_subdir) == false);

      // No $PATH at all falls back to a built-in default, where sh lives
      unsetenv("PATH");
      CK(is_command_in_path("sh") == true);

      if (saved_path)
         setenv("PATH", saved_path, 1);
      g_free(saved_path);

      unlink(fq_exec);
      unlink(fq_plain);
      rmdir(fq_subdir);
      rmdir(tmpdir);
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
