/** @file test_monitor_quirks.c
 *
 *  Standalone unit test for src/base/monitor_quirks.c: get_monitor_quirks looks
 *  up a Monitor_Model_Key in the built-in quirk table.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/monitor_model_key.h"
#include "base/monitor_quirks.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

int main(int argc, char ** argv) {
   // a model in the quirk table ("XMI", "Mi Monitor", 13380) is found.
   // Build the key directly so its model_name matches the table entry verbatim.
   Monitor_Model_Key known;
   memset(&known, 0, sizeof(known));
   g_strlcpy(known.mfg_id, "XMI", sizeof(known.mfg_id));
   g_strlcpy(known.model_name, "Mi Monitor", sizeof(known.model_name));
   known.product_code = 13380;
   known.defined = true;
   CK(get_monitor_quirks(&known) != NULL);

   // a model not in the table returns NULL
   Monitor_Model_Key unknown = mmk_value("DEL", "U2412M", 4660);
   CK(get_monitor_quirks(&unknown) == NULL);

   // an undefined key never matches
   Monitor_Model_Key undef = mmk_undefined_value();
   CK(get_monitor_quirks(&undef) == NULL);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
