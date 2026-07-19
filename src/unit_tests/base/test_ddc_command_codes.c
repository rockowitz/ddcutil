/** @file test_ddc_command_codes.c
 *
 *  Standalone unit tests for src/base/ddc_command_codes.c: the DDC command-code
 *  name lookup.
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

#include "util/coredefs_base.h"    // Byte
#include "base/ddc_command_codes.h"

static int total = 0;
static int failed = 0;

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

int main(int argc, char ** argv) {
   CK_STR(ddc_cmd_code_name(CMD_VCP_REQUEST), "VCP Request");
   CK_STR(ddc_cmd_code_name(CMD_VCP_SET), "VCP Set");
   CK_STR(ddc_cmd_code_name(CMD_SAVE_SETTINGS), "Save Settings");
   CK_STR(ddc_cmd_code_name(CMD_CAPABILITIES_REQUEST), "Capabilities Request");
   CK_STR(ddc_cmd_code_name(0xab), "Unrecognized operation code");   // unknown

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
