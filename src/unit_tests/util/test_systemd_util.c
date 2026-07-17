/** @file test_systemd_util.c
 *
 *  Standalone unit tests for src/util/systemd_util.c.
 *
 *  get_current_boot_id() reads /proc/sys/kernel/random/boot_id (always present
 *  on Linux) and strips the hyphens, yielding a 32-character lowercase hex
 *  string; that is checked here.  get_current_boot_messages() reads the systemd
 *  journal and depends on the host, so it is not exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <ctype.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/systemd_util.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

int main(int argc, char ** argv) {
   char * boot_id = get_current_boot_id();
   CK(boot_id != NULL);
   if (boot_id) {
      CK(strlen(boot_id) == 32);          // 128-bit UUID, hyphens removed
      bool all_hex = true;
      for (const char * p = boot_id; *p; p++)
         if (!isxdigit((unsigned char) *p)) { all_hex = false; break; }
      CK(all_hex == true);
      // the value is stable across calls within a boot
      char * boot_id2 = get_current_boot_id();
      CK(boot_id2 != NULL && strcmp(boot_id, boot_id2) == 0);
      free(boot_id2);
      free(boot_id);
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
