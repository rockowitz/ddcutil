/** @file test_report_util.c
 *
 *  Standalone unit tests for the functions in src/util/report_util.c.
 *
 *  The rpt_* functions write indented, optionally ornamented text to the
 *  current output destination.  These tests redirect that destination to an
 *  in-memory stream (open_memstream) so the produced bytes can be compared
 *  exactly.  Ornamentation and the message-decoration inputs are disabled so
 *  output is fully determined by the arguments.
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

#include "util/coredefs_base.h"    // Byte
#include "util/report_util.h"

static int total = 0;
static int failed = 0;

static char *  capdata = NULL;   // filled by the capture stream
static size_t  capsize = 0;

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

// Runs `stmt` with output redirected to a fresh memory stream, then compares
// the captured bytes against `expected`.
#define CK_OUT(expected, stmt) do { \
   g_free(capdata); capdata = NULL; capsize = 0; \
   FILE * _ms = open_memstream(&capdata, &capsize); \
   rpt_push_output_dest(_ms); \
   stmt; \
   fflush(_ms); \
   rpt_pop_output_dest(); \
   fclose(_ms); \
   total++; \
   const char * _e = (expected); \
   if (!capdata || strcmp(capdata, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  got |%s| expected |%s|\n", __LINE__, \
             capdata ? capdata : "(null)", _e); } \
} while(0)

// Like CK_OUT but only requires that the captured output contain `needle`.
#define CK_OUT_HAS(needle, stmt) do { \
   g_free(capdata); capdata = NULL; capsize = 0; \
   FILE * _ms = open_memstream(&capdata, &capsize); \
   rpt_push_output_dest(_ms); \
   stmt; \
   fflush(_ms); \
   rpt_pop_output_dest(); \
   fclose(_ms); \
   total++; \
   if (!capdata || strstr(capdata, (needle)) == NULL) { failed++; \
      printf("FAIL  line %-4d  |%s| does not contain |%s|\n", __LINE__, \
             capdata ? capdata : "(null)", (needle)); } \
} while(0)

int main(int argc, char ** argv) {
   rpt_set_ornamentation_enabled(false);

   // indent is depth * 3 spaces by default; negative depth clamps to 0
   CK_INT(rpt_get_indent(0), 0);
   CK_INT(rpt_get_indent(1), 3);
   CK_INT(rpt_get_indent(3), 9);
   CK_INT(rpt_get_indent(-5), 0);

   // ornamentation setter returns the previous value
   CK(rpt_set_ornamentation_enabled(true) == false);
   CK(rpt_get_ornamentation_enabled() == true);
   CK(rpt_set_ornamentation_enabled(false) == true);
   CK(rpt_get_ornamentation_enabled() == false);

   // title / label: indented text plus newline
   CK_OUT("hello\n",       rpt_title("hello", 0));
   CK_OUT("   hello\n",    rpt_title("hello", 1));
   CK_OUT("      x\n",     rpt_label(2, "x"));

   // blank line
   CK_OUT("\n",            rpt_nl());

   // formatted string, with and without indentation
   CK_OUT("n=5\n",         rpt_vstring(0, "n=%d", 5));
   CK_OUT("      a-7\n",   rpt_vstring(2, "%s-%d", "a", 7));

   // named-value helpers share the "%-25s %30s : %s" layout
   char * exp;
   exp = g_strdup_printf("%-25s %30s : %s\n", "Count", "", "42");
   CK_OUT(exp, rpt_int("Count", NULL, 42, 0));
   g_free(exp);

   exp = g_strdup_printf("%-25s %30s : %s\n", "Enabled", "", "true");
   CK_OUT(exp, rpt_bool("Enabled", NULL, true, 0));
   g_free(exp);

   exp = g_strdup_printf("%-25s %30s : %s\n", "Enabled", "", "false");
   CK_OUT(exp, rpt_bool("Enabled", NULL, false, 0));
   g_free(exp);

   // info string is parenthesized when present
   exp = g_strdup_printf("%-25s %30s : %s\n", "Key", "(a description)", "val");
   CK_OUT(exp, rpt_str("Key", "a description", "val", 0));
   g_free(exp);

   // rpt_str indents the whole line
   exp = g_strdup_printf("   %-25s %30s : %s\n", "Key", "", "val");
   CK_OUT(exp, rpt_str("Key", NULL, "val", 1));
   g_free(exp);

   // rpt_2col: first column left-justified to the offset, then second column
   exp = g_strdup_printf("%-*s%s\n", 20, "left", "right");
   CK_OUT(exp, rpt_2col("left", "right", 20, false, 0));
   g_free(exp);

   // hex dump: contains the ruler and the bytes, spaced three columns apart
   Byte data[] = {0xde, 0xad, 0x42};
   CK_OUT_HAS("de ad 42", rpt_hex_dump(data, 3, 0));

   g_free(capdata);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
