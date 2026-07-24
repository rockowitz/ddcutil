/** @file test_i2c_services.c
 *
 *  Standalone smoke test for src/i2c/i2c_services.c: init_i2c_services()
 *  and terminate_i2c_services() must run without touching any real
 *  hardware (they only register RTTI trace names and set the default I2C
 *  IO strategy) and terminate_i2c_services() must tolerate being called
 *  when no buses were ever detected.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libi2c unit test: it links the internal libi2c/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2c/i2c_services.h"
#include "i2c/i2c_strategy_dispatcher.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


int main(int argc, char ** argv) {
   init_i2c_services();

   // init_i2c_strategy_dispatcher(), called by init_i2c_services(), must
   // have set a default (non-NOT_SET) IO strategy.
   CK(i2c_get_io_strategy_id() != I2C_IO_STRATEGY_NOT_SET);

   terminate_i2c_services();
   // safe to call again: no buses were ever detected, so nothing to free
   terminate_i2c_services();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
