/** @file test_ddcutil_config_file.c
 *
 *  Standalone unit tests for the functions in src/util/ddcutil_config_file.c.
 *
 *  tokenize_options_line() splits a string the way a shell would (via wordexp),
 *  so it is checked against exact token lists for quoting, whitespace, variable
 *  expansion, and the error cases (unmatched quote, rejected command
 *  substitution).  read_ddcutil_config_file() and apply_config_file() are driven
 *  by pointing $XDG_CONFIG_HOME at a temporary directory holding a synthetic
 *  ddcutilrc, so the merged option string and argv are fully determined.
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

#include "util/ddcutil_config_file.h"
#include "util/string_util.h"     // ntsa_length, ntsa_free, streq

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

// Compares a NULL-terminated string array against an expected NULL-terminated list.
static void ck_ntsa(int line, char ** actual, const char * const * expected) {
   int n = 0;
   while (expected[n]) n++;
   total++;
   bool ok = (actual != NULL) && (ntsa_length(actual) == n);
   for (int i = 0; ok && i < n; i++)
      ok = streq(actual[i], expected[i]);
   if (!ok) {
      failed++;
      printf("FAIL  line %-4d  tokens ->", line);
      if (actual) for (int i = 0; actual[i]; i++) printf(" |%s|", actual[i]);
      printf(", expected");
      for (int i = 0; i < n; i++) printf(" |%s|", expected[i]);
      printf("\n");
   }
}
#define CK_NTSA(arr, ...) \
   ck_ntsa(__LINE__, (arr), (const char * const []){ __VA_ARGS__, NULL })

static void test_tokenize(void) {
   char ** tokens;
   int ct;

   ct = tokenize_options_line("one two three", &tokens);
   CK_INT(ct, 3);
   CK_NTSA(tokens, "one", "two", "three");
   ntsa_free(tokens, true);

   // leading/trailing/collapsed whitespace is ignored
   ct = tokenize_options_line("   a    b  ", &tokens);
   CK_INT(ct, 2);
   CK_NTSA(tokens, "a", "b");
   ntsa_free(tokens, true);

   // empty string -> no tokens
   ct = tokenize_options_line("", &tokens);
   CK_INT(ct, 0);
   ntsa_free(tokens, true);

   // single-quoted string is one token, quotes removed
   ct = tokenize_options_line("'hello world' x", &tokens);
   CK_INT(ct, 2);
   CK_NTSA(tokens, "hello world", "x");
   ntsa_free(tokens, true);

   // double quotes likewise
   ct = tokenize_options_line("\"dq string\"", &tokens);
   CK_INT(ct, 1);
   CK_NTSA(tokens, "dq string");
   ntsa_free(tokens, true);

   // environment variable expansion
   setenv("DDCUTIL_TEST_VAR", "expanded", 1);
   ct = tokenize_options_line("$DDCUTIL_TEST_VAR tail", &tokens);
   CK_INT(ct, 2);
   CK_NTSA(tokens, "expanded", "tail");
   ntsa_free(tokens, true);

   // error cases return 0 and an empty (non-NULL) array; the function writes a
   // diagnostic to stderr, redirected away here to keep the log clean
   int saved = dup(STDERR_FILENO);
   FILE * devnull = fopen("/dev/null", "w");
   if (devnull) dup2(fileno(devnull), STDERR_FILENO);

   ct = tokenize_options_line("'unterminated", &tokens);    // unmatched quote
   CK_INT(ct, 0);
   CK(tokens != NULL && tokens[0] == NULL);
   ntsa_free(tokens, true);

   ct = tokenize_options_line("$(echo hi)", &tokens);        // command substitution rejected
   CK_INT(ct, 0);
   CK(tokens != NULL && tokens[0] == NULL);
   ntsa_free(tokens, true);

   if (devnull) { fflush(stderr); dup2(saved, STDERR_FILENO); fclose(devnull); }
   close(saved);
}

// Creates a temp config dir containing ddcutil/<simple_fn> with `content`,
// points $XDG_CONFIG_HOME at it, and returns the strdup'd dir (caller frees).
static char * make_config(const char * simple_fn, const char * content) {
   char tmpl[] = "/tmp/test_cfg_XXXXXX";
   char * dir = g_strdup(mkdtemp(tmpl));
   char * appdir = g_strdup_printf("%s/ddcutil", dir);
   g_mkdir_with_parents(appdir, 0700);
   char * fn = g_strdup_printf("%s/%s", appdir, simple_fn);
   FILE * f = fopen(fn, "w");
   if (f) { fputs(content, f); fclose(f); }
   setenv("XDG_CONFIG_HOME", dir, 1);
   setenv("XDG_CONFIG_DIRS", "/nonexistent-xdg-dir", 1);   // neutralize /etc/xdg
   g_free(appdir);
   g_free(fn);
   return dir;
}

static void test_read_config(void) {
   const char * rc =
      "[global]\n"
      "options = --verbose\n"
      "\n"
      "[ddcutil]\n"
      "options = --sleep-multiplier 2\n";
   char * dir = make_config("ddcutilrc", rc);

   char * fn = NULL;
   char * opts = NULL;
   GPtrArray * errs = g_ptr_array_new_with_free_func(g_free);
   int rrc = read_ddcutil_config_file("ddcutil", &fn, &opts, errs);
   CK_INT(rrc, 0);
   CK_INT(errs->len, 0);
   // global options precede application options, whitespace trimmed
   CK_STR(opts, "--verbose --sleep-multiplier 2");
   CK(fn != NULL && g_str_has_suffix(fn, "ddcutil/ddcutilrc"));
   free(fn);
   free(opts);
   g_ptr_array_free(errs, TRUE);
   g_free(dir);
}

static void test_apply_config(void) {
   const char * rc =
      "[global]\n"
      "options = --verbose\n"
      "\n"
      "[ddcutil]\n"
      "options = --sleep-multiplier 2\n";
   char * dir = make_config("ddcutilrc", rc);

   char * old_argv[] = { "ddcutil", "--foo", NULL };
   int    old_argc   = 2;
   int    new_argc   = 0;
   char ** new_argv  = NULL;
   char * untok = NULL;
   char * fn    = NULL;
   GPtrArray * errs = g_ptr_array_new_with_free_func(g_free);

   int arc = apply_config_file("ddcutil", old_argc, old_argv,
                               &new_argc, &new_argv, &untok, &fn, errs);
   CK_INT(arc, 0);
   // command name, then the 3 config tokens, then the original argument
   CK_INT(new_argc, 5);
   CK_NTSA(new_argv, "ddcutil", "--verbose", "--sleep-multiplier", "2", "--foo");

   ntsa_free(new_argv, true);
   free(untok);
   free(fn);
   g_ptr_array_free(errs, TRUE);
   g_free(dir);
}

static void test_missing_config(void) {
   char tmpl[] = "/tmp/test_cfg_XXXXXX";
   char * dir = mkdtemp(tmpl);                    // empty: no ddcutil subdir
   setenv("XDG_CONFIG_HOME", dir, 1);
   setenv("XDG_CONFIG_DIRS", "/nonexistent-xdg-dir", 1);

   char * fn = NULL;
   char * opts = NULL;
   GPtrArray * errs = g_ptr_array_new_with_free_func(g_free);
   int rrc = read_ddcutil_config_file("ddcutil", &fn, &opts, errs);
   CK_INT(rrc, -ENOENT);
   CK(opts == NULL);
   CK(fn == NULL);
   g_ptr_array_free(errs, TRUE);
}

int main(int argc, char ** argv) {
   test_tokenize();
   test_read_config();
   test_apply_config();
   test_missing_config();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
