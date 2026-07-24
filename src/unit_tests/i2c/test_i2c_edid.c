/** @file test_i2c_edid.c
 *
 *  Standalone unit tests for src/i2c/i2c_edid.c, restricted to the
 *  defensive behavior on an invalid file descriptor: every read path
 *  (ioctl-based and fileio-based) must fail promptly with a negative
 *  errno and must not crash, since a real EDID read requires an actual
 *  /dev/i2c device with a monitor attached.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libi2c unit test: it links the internal libi2c/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/edid.h"

#include "base/parms.h"

#include "i2c/i2c_edid.h"
#include "i2c/i2c_strategy_dispatcher.h"

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


static void test_get_edid_bytes_using_single_ioctl_bad_fd(void) {
   Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
   int rc = i2c_get_edid_bytes_using_single_ioctl(-1, buf, 128);
   CK(rc < 0);
   buffer_free(buf, NULL);
}


static void test_get_raw_edid_by_fd_bad_fd(void) {
   Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
   int rc = i2c_get_raw_edid_by_fd(-1, buf);
   CK(rc < 0);
   CK_INT(buf->len, 0);
   buffer_free(buf, NULL);
}


static void test_get_parsed_edid_by_fd_bad_fd(void) {
   Parsed_Edid * edid = NULL;
   int rc = i2c_get_parsed_edid_by_fd(-1, &edid);
   CK(rc < 0);
   CK(edid == NULL);
}


// Exercise both the IOCTL and FILEIO strategies, and both the
// EDID_Read_Uses_I2C_Layer branches, with the bad-fd path.
static void test_get_raw_edid_across_strategies(void) {
   bool saved_uses_i2c_layer = EDID_Read_Uses_I2C_Layer;

   I2C_IO_Strategy_Id strategies[] = { I2C_IO_STRATEGY_IOCTL, I2C_IO_STRATEGY_FILEIO };
   bool use_i2c_layer_values[] = { false, true };

   for (int s = 0; s < 2; s++) {
      i2c_set_io_strategy_by_id(strategies[s]);
      for (int u = 0; u < 2; u++) {
         EDID_Read_Uses_I2C_Layer = use_i2c_layer_values[u];
         Buffer * buf = buffer_new(EDID_BUFFER_SIZE, NULL);
         int rc = i2c_get_raw_edid_by_fd(-1, buf);
         CK(rc < 0);
         buffer_free(buf, NULL);
      }
   }

   EDID_Read_Uses_I2C_Layer = saved_uses_i2c_layer;
   i2c_set_io_strategy_by_id(DEFAULT_I2C_IO_STRATEGY);
}


int main(int argc, char ** argv) {
   i2c_set_io_strategy_by_id(DEFAULT_I2C_IO_STRATEGY);   // required: asserted non-NOT_SET

   test_get_edid_bytes_using_single_ioctl_bad_fd();
   test_get_raw_edid_by_fd_bad_fd();
   test_get_parsed_edid_by_fd_bad_fd();
   test_get_raw_edid_across_strategies();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
