/** @file test_pnp_ids.c
 *
 *  Standalone unit tests for src/util/pnp_ids.c.
 *
 *  pnp_name() maps a 3-character PnP/EDID manufacturer id to a vendor name,
 *  using either the system hwdata pnp.ids file or a built-in fallback table.
 *  The exact strings differ slightly between sources and versions, so well
 *  known vendor ids are checked by a stable substring.  A miss returns "UNK"
 *  (file-based) or NULL (internal table), so both are accepted.
 *
 *  Note: pnp_name() uppercases its argument in place for the internal-table
 *  path, so the tests pass mutable buffers, not string literals.
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

#include "util/pnp_ids.h"

static int total = 0;
static int failed = 0;

// Checks that pnp_name(<code>) contains `needle`.
#define CK_VENDOR(code, needle) do { \
   total++; \
   char _id[] = code; \
   char * _r = pnp_name(_id); \
   if (_r == NULL || strstr(_r, needle) == NULL) { failed++; \
      printf("FAIL  line %-4d  pnp_name(\"%s\") -> \"%s\", expected to contain \"%s\"\n", \
             __LINE__, code, _r ? _r : "(null)", needle); } \
} while(0)

int main(int argc, char ** argv) {
   CK_VENDOR("DEL", "Dell");
   CK_VENDOR("SAM", "Samsung");
   CK_VENDOR("APP", "Apple");
   CK_VENDOR("NEC", "NEC");
   CK_VENDOR("HWP", "Hewlett");
   CK_VENDOR("LEN", "Lenovo");

   // an unassigned code returns the "UNK" sentinel (file) or NULL (internal table)
   total++;
   char miss[] = "QZQ";
   char * r = pnp_name(miss);
   if (!(r == NULL || strcmp(r, "UNK") == 0)) {
      failed++;
      printf("FAIL  line %-4d  pnp_name(\"QZQ\") -> \"%s\", expected NULL or \"UNK\"\n",
             __LINE__, r);
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
