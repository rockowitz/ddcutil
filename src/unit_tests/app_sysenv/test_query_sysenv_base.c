/** @file test_query_sysenv_base.c
 *
 *  Standalone unit tests for the pure subset of src/app_sysenv/query_sysenv_base.c:
 *  the static driver-name-string tables (get_known_video_driver_module_names()
 *  etc), the in-memory Driver_Name_Node linked list
 *  (driver_name_list_add()/_find_exact()/_find_prefix()/_free()/_string(),
 *  only_fglrx(), only_nvidia_or_fglrx()), i2c_path_to_busno() (pure string
 *  parsing), the Env_Accumulator allocator/reporter (env_accumulator_new()/
 *  _free()/_report()), and sysenv_show_one_file() exercised against a
 *  nonexistent path (its "file not found" branch never opens anything).
 *
 *  sysenv_rpt_file_first_line() (always reads a real file) and
 *  sysenv_rpt_current_time() (reads the real system clock/sysinfo) are
 *  not tested here.
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

#include "app_sysenv/query_sysenv_base.h"

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


static void test_static_driver_tables(void) {
   char ** known = get_known_video_driver_module_names();
   CK(known != NULL);
   bool found_nvidia = false;
   for (int i = 0; known[i]; i++)
      if (strcmp(known[i], "nvidia") == 0)
         found_nvidia = true;
   CK(found_nvidia);

   char ** prefixes = get_prefix_match_names();
   CK(prefixes != NULL && prefixes[0] != NULL);

   char ** other = get_other_driver_module_names();
   CK(other != NULL && other[0] != NULL);

   char ** all = get_all_driver_module_strings();
   CK(all != NULL);
   bool found_i2c_dev = false;
   for (int i = 0; all[i]; i++)
      if (strcmp(all[i], "i2c_dev") == 0)
         found_i2c_dev = true;
   CK(found_i2c_dev);
}


static void test_driver_name_list(void) {
   Driver_Name_Node * head = NULL;

   CK(driver_name_list_find_exact(head, "nvidia") == NULL);
   CK(driver_name_list_find_prefix(head, "nvi") == NULL);

   driver_name_list_add(&head, "nvidia");
   driver_name_list_add(&head, "i2c_dev");
   driver_name_list_add(&head, "nvidia");   // duplicate, not added again

   CK(driver_name_list_find_exact(head, "nvidia") != NULL);
   CK(driver_name_list_find_exact(head, "bogus") == NULL);
   CK(driver_name_list_find_prefix(head, "i2c") != NULL);

   int ct = 0;
   for (Driver_Name_Node * n = head; n; n = n->next)
      ct++;
   CK_INT(ct, 2);   // "nvidia" duplicate was not added

   char * s = driver_name_list_string(head);
   CK(s != NULL);
   CK_STR_CONTAINS(s, "nvidia");
   CK_STR_CONTAINS(s, "i2c_dev");
   free(s);

   driver_name_list_free(head);
}


static void test_only_fglrx(void) {
   Driver_Name_Node * head = NULL;
   driver_name_list_add(&head, "fglrx");
   CK(only_fglrx(head) == true);
   CK(only_nvidia_or_fglrx(head) == true);

   driver_name_list_add(&head, "nouveau");
   CK(only_fglrx(head) == false);          // no longer the only driver
   CK(only_nvidia_or_fglrx(head) == false);   // nouveau is neither nvidia nor fglrx

   driver_name_list_free(head);
}


static void test_only_nvidia_or_fglrx(void) {
   Driver_Name_Node * head = NULL;
   driver_name_list_add(&head, "nvidia");
   driver_name_list_add(&head, "fglrx");
   CK(only_nvidia_or_fglrx(head) == true);
   CK(only_fglrx(head) == false);   // more than 1 driver

   driver_name_list_free(head);
}


static void test_i2c_path_to_busno(void) {
   CK_INT(i2c_path_to_busno("/dev/i2c-3"), 3);
   CK_INT(i2c_path_to_busno("i2c-11"), 11);
   CK_INT(i2c_path_to_busno("/sys/bus/i2c/devices/i2c-0"), 0);
   CK_INT(i2c_path_to_busno("/dev/not-i2c-3"), -1);
   CK_INT(i2c_path_to_busno(NULL), -1);
}


static void test_env_accumulator(void) {
   Env_Accumulator * accum = env_accumulator_new();
   CK(accum != NULL);
   CK(memcmp(accum->marker, ENV_ACCUMULATOR_MARKER, 4) == 0);
   CK(accum->dev_i2c_devices_required == true);
   CK(accum->cur_user_all_devi2c_rw == true);
   CK(accum->architecture == NULL);

   char buf[4000];
   CAPTURE( env_accumulator_report(accum, 0), buf, sizeof(buf) );
   CK_STR_CONTAINS(buf, "Env_Accumulator:");
   CK_STR_CONTAINS(buf, "dev_i2c_devices_required");

   env_accumulator_free(accum);
}


static void test_sysenv_show_one_file_not_found(void) {
   char buf[500];
   bool result;
   CAPTURE( result = sysenv_show_one_file("/nonexistent/dir/for/testing", "foo.txt",
                                           /*verbose=*/true, 0),
            buf, sizeof(buf) );
   CK(result == false);
   CK_STR_CONTAINS(buf, "File not found");
}


int main(int argc, char ** argv) {
   test_static_driver_tables();
   test_driver_name_list();
   test_only_fglrx();
   test_only_nvidia_or_fglrx();
   test_i2c_path_to_busno();
   test_env_accumulator();
   test_sysenv_show_one_file_not_found();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
