/** @file test_query_sysenv_final_analysis.c
 *
 *  Standalone unit tests for src/app_sysenv/query_sysenv.c: final_analysis(),
 *  the ENVIRONMENT command's "Configuration suggestions" report. It is a
 *  pure decision tree driven entirely by fields already collected in its
 *  Env_Accumulator argument -- no file or hardware I/O of its own.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappsysenv/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/data_structures.h"

#include "app_sysenv/query_sysenv.h"
#include "app_sysenv/query_sysenv_base.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR_CONTAINS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected to contain \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _n); } \
} while(0)

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file
 * whose contents are then read into caller-supplied buffer `outbuf`
 * (size outbufsz), NUL terminated. */
#define CAPTURE(stmt, outbuf, outbufsz) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   rewind(_tmp); \
   size_t _n = fread((outbuf), 1, (outbufsz)-1, _tmp); \
   (outbuf)[_n] = '\0'; \
   fclose(_tmp); \
} while(0)


static void test_full_rw_access_skips_checks(void) {
   Env_Accumulator * accum = env_accumulator_new();
   accum->dev_i2c_device_numbers = bva_create();
   bva_append(accum->dev_i2c_device_numbers, 3);
   accum->cur_user_all_devi2c_rw = true;

   char buf[4000];
   CAPTURE( final_analysis(accum, 0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "Current user has RW access to all /dev/i2c-N devices.");
   CK_STR_CONTAINS(buf, "Skipping further group and permission checks.");

   env_accumulator_free(accum);
}


static void test_no_dev_i2c_devices_found(void) {
   Env_Accumulator * accum = env_accumulator_new();
   accum->dev_i2c_device_numbers = bva_create();   // empty
   accum->module_i2c_dev_needed = true;
   accum->i2c_dev_loaded_or_builtin = false;
   accum->sysfs_i2c_devices_exist = false;

   char buf[4000];
   CAPTURE( final_analysis(accum, 0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "No /dev/i2c-N devices found.");
   CK_STR_CONTAINS(buf, "sudo modprobe i2c-dev");

   env_accumulator_free(accum);
}


static void test_group_i2c_missing(void) {
   Env_Accumulator * accum = env_accumulator_new();
   accum->dev_i2c_device_numbers = bva_create();
   bva_append(accum->dev_i2c_device_numbers, 3);
   accum->cur_user_all_devi2c_rw = false;
   accum->cur_user_any_devi2c_rw = false;
   accum->group_i2c_exists = false;

   char buf[4000];
   CAPTURE( final_analysis(accum, 0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "Group i2c does not exist.");
   CK_STR_CONTAINS(buf, "sudo groupadd --system i2c");

   env_accumulator_free(accum);
}


static void test_no_issues_reports_none(void) {
   Env_Accumulator * accum = env_accumulator_new();
   accum->dev_i2c_device_numbers = bva_create();
   bva_append(accum->dev_i2c_device_numbers, 3);
   accum->cur_user_all_devi2c_rw    = false;
   accum->cur_user_any_devi2c_rw    = false;
   accum->group_i2c_exists          = true;
   accum->all_dev_i2c_has_group_i2c = true;
   accum->cur_user_in_group_i2c     = true;
   accum->all_dev_i2c_is_group_rw   = true;
   // dev_i2c_common_group_name left NULL: odd_groups short-circuits to false,
   // and the group-RW-permission check is skipped since all_dev_i2c_is_group_rw is true

   char buf[4000];
   CAPTURE( final_analysis(accum, 0), buf, sizeof(buf) );

   CK_STR_CONTAINS(buf, "Configuration suggestions:");
   CK_STR_CONTAINS(buf, "None");
   CK(strstr(buf, "Issue:") == NULL);

   env_accumulator_free(accum);
}


int main(int argc, char ** argv) {
   test_full_rw_access_skips_checks();
   test_no_dev_i2c_devices_found();
   test_group_i2c_missing();
   test_no_issues_reports_none();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
