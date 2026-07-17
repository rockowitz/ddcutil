/** @file test_utilrpt.c
 *
 *  Standalone unit tests for src/util/utilrpt.c.
 *
 *  dbgrpt_buffer() writes a Buffer's fields and a hex dump to the current report
 *  destination.  The test redirects that destination to an in-memory stream and
 *  checks that the field summary and the dumped bytes appear.
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
#include "util/data_structures.h"  // Buffer
#include "util/report_util.h"
#include "util/utilrpt.h"

static int total = 0;
static int failed = 0;

// Checks that the output of `stmt` contains `needle`.
#define CK_OUT_HAS(needle, stmt) do { \
   char * _cap = NULL; size_t _sz = 0; \
   FILE * _ms = open_memstream(&_cap, &_sz); \
   rpt_push_output_dest(_ms); \
   stmt; \
   fflush(_ms); rpt_pop_output_dest(); fclose(_ms); \
   total++; \
   if (!_cap || strstr(_cap, (needle)) == NULL) { failed++; \
      printf("FAIL  line %-4d  output |%s| does not contain |%s|\n", __LINE__, \
             _cap ? _cap : "(null)", (needle)); } \
   g_free(_cap); \
} while(0)

int main(int argc, char ** argv) {
   rpt_set_ornamentation_enabled(false);

   Byte v[] = { 0xde, 0xad, 0x42 };
   Buffer * b = buffer_new_with_value(v, 3, "test");

   // field summary reports the length and capacity
   CK_OUT_HAS("Buffer at",  dbgrpt_buffer(b, 0));
   CK_OUT_HAS("len=3",      dbgrpt_buffer(b, 0));
   CK_OUT_HAS("max_size=3", dbgrpt_buffer(b, 0));
   // the byte contents are hex dumped
   CK_OUT_HAS("de ad 42",   dbgrpt_buffer(b, 0));

   buffer_free(b, "test");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
