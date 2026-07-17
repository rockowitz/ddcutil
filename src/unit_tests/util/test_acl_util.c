/** @file test_acl_util.c
 *
 *  Standalone unit tests for the functions in src/util/acl_util.c.
 *
 *  acl_get_file() always synthesizes an access ACL from a file's mode bits, so
 *  the base cases (the collectors, the group-object rw test, and the absence of
 *  a named-user entry) are checked against an ordinary temp file whose mode is
 *  set explicitly.  The named-user cases require setting an extended ACL with
 *  acl_set_file(), which some filesystems reject; those checks are skipped (not
 *  failed) with a printed note when the filesystem does not support ACLs.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <acl/libacl.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/acl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/acl_util.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

// Joins a GPtrArray of strings with newlines into a freshly allocated buffer.
static char * join(GPtrArray * a) {
   GString * gs = g_string_new("");
   for (guint i = 0; i < a->len; i++) {
      g_string_append(gs, (char *) g_ptr_array_index(a, i));
      g_string_append_c(gs, '\n');
   }
   return g_string_free(gs, FALSE);
}

int main(int argc, char ** argv) {
   char path[] = "/tmp/test_acl_XXXXXX";
   int fd = mkstemp(path);
   if (fd < 0) { perror("mkstemp"); return 2; }
   close(fd);

   // --- base cases derived purely from the mode bits ---

   // group r-- (mode 0640): group-object is not rw
   chmod(path, 0640);
   CK(is_file_group_acl_rw(path) == false);

   // group rw- (mode 0660): group-object is rw
   chmod(path, 0660);
   CK(is_file_group_acl_rw(path) == true);

   // collector output lists the standard entries
   GPtrArray * c0 = rpt_facl_collect0(path, NULL, 0);
   char * s0 = join(c0);
   CK(strstr(s0, "user::")  != NULL);
   CK(strstr(s0, "group::") != NULL);
   CK(strstr(s0, "other::") != NULL);
   g_free(s0);
   g_ptr_array_free(c0, TRUE);

   GPtrArray * c1 = rpt_facl_collect1(path, NULL, 0);
   char * s1 = join(c1);
   CK(strstr(s1, "ACL entries for") != NULL);
   CK(strstr(s1, "user") != NULL);
   g_free(s1);
   g_ptr_array_free(c1, TRUE);

   // a plain file has no named-user entry
   char * pu = get_user_acl(path, 4242);
   CK(pu == NULL);
   free(pu);
   CK(is_acl_rw(path, ACL_USER, 4242) == false);
   CK(is_cur_user_acl_rw(path) == false);

   // --- extended cases requiring an on-disk ACL ---

   acl_t acl = acl_from_text("u::rw-,u:4242:rw-,g::r--,m::rw-,o::r--");
   CK(acl != NULL);
   if (acl && acl_set_file(path, ACL_TYPE_ACCESS, acl) == 0) {
      char * p = get_user_acl(path, 4242);
      CK_STR(p, "rw-");
      free(p);
      CK(is_acl_rw(path, ACL_USER, 4242) == true);
      CK(is_acl_rw(path, ACL_USER, 4243) == false);   // different uid
   }
   else {
      printf("NOTE  extended-ACL checks skipped: acl_set_file failed (errno=%d %s)\n",
             errno, strerror(errno));
   }
   if (acl) acl_free(acl);

   // non-existent file: collectors report the failure, predicates are false
   CK(is_cur_user_acl_rw("/no/such/file") == false);
   GPtrArray * cerr = rpt_facl_collect0("/no/such/file", NULL, 0);
   char * serr = join(cerr);
   CK(strstr(serr, "failed") != NULL);
   g_free(serr);
   g_ptr_array_free(cerr, TRUE);

   unlink(path);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
