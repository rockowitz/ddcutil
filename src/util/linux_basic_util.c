/** @file linux_basic_util.c
 *
 *  Basic Linux user/group/thread utilities
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <glib-2.0/glib.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <syslog.h>
#endif
#include <unistd.h>
/** \endcond */

#include "common_inlines.h"
#include "debug_util.h"
#include "string_util.h"

#include "linux_basic_util.h"


/** Gets the id number of the current thread
 *
 *  \return  thread number
 */
intmax_t get_thread_id() {
   bool debug = false;
   DBGF(debug, "Starting.");

#ifdef TARGET_BSD
   int tid = pthread_getthreadid_np();
#else
   pid_t tid = syscall(SYS_gettid);
#endif
   DBGF(debug, "Done.    Returning %jd", (intmax_t) tid);
   return tid;
}


/** Gets the id number of the current process
 *
 *  \return  process number
 */
intmax_t get_process_id()
{
   pid_t pid = syscall(SYS_getpid);
   return pid;
}


/** Checks that a thread or process id is valid.
 *
 *  @param  id  thread or process id
 *  @return true if valid, false if not
 */
bool is_valid_thread_or_process(pid_t id) {
   bool debug = false;
   struct stat buf;
   char procfn[20];
   snprintf(procfn, 20, "/proc/%d", id);
   int rc = stat(procfn, &buf);
   bool result = (rc == 0);
   DBGF(debug, "File: %s, returning %s", procfn, sbool(result));
   if (!result)
      DBG("!!! Returning: %s", sbool(result));
   return result;
}


/** Returns the name for a user id
 *
 *  @param uid  user id
 *  @return name of user, or "unknown" if unrecognized
 *
 *  The returned string should not be freed.
 */
char * uid_name(int uid) {
   struct passwd * pw = getpwuid(uid);
   char*  uid_name = pw ? pw->pw_name : "unknown";
   return uid_name;
}


/** Returns the name for a group id
 *
 *  @param gid  group id
 *  @return name of group, or "unknown" if unrecognized
 *
 *  The returned string should not be freed.
 */
char * gid_name(int gid) {
   struct group * gr = getgrgid(gid);
   char*  gid_name = gr ? gr->gr_name : "unknown";
   return gid_name;
}


bool group_i2c_exists() {
   bool debug = true;
   bool result = false;

   struct group * pgi2c = getgrnam("i2c");
   if (pgi2c) {
      result = true;
   }
   DBGF(debug, "Returning %s", sbool(result));
   return result;
}


/** Reports whether group i2c exists and whether the current user is a
 *  member of the group.
 *
 *  @param collector  GPtrArray to which report lines are appended
 *  @return true if current user is in group i2c
 */
bool check_group_i2c_collect(GPtrArray* collector) {
   bool debug = true;
   bool result = false;
   bool grp_i2c_exists = false;
   bool cur_user_in_group_i2c = false;
   bool cur_euser_in_group_i2c = false;

   g_ptr_array_add(collector, strdup("Checking for group i2c..."));

   int uid  = (int) getuid();
   int euid = (int) geteuid();
   char * cur_uname  = uid_name(uid);
   char * cur_euname = uid_name(euid);

   struct group * pgi2c = getgrnam("i2c");
   if (pgi2c) {
      g_ptr_array_add(collector, strdup("Group i2c exists"));
      grp_i2c_exists = true;
      int ndx = 0;
      char * curname;
      while ( (curname = pgi2c->gr_mem[ndx]) ) {
         rtrim_in_place(curname);
         if (streq(curname, cur_uname)) {
            cur_user_in_group_i2c = true;
         }
         if (euid != uid && streq(curname, cur_euname)) {
            cur_euser_in_group_i2c = true;
         }
         ndx++;
      }
      bool in_group_i2c = false;
      if (cur_user_in_group_i2c) {
         in_group_i2c = true;
         char * s = g_strdup_printf("Current user %s is a member of group i2c", cur_uname);
         g_ptr_array_add(collector, s);
      }
      else if (uid != euid && cur_euser_in_group_i2c) {
         in_group_i2c = true;
         char * s = g_strdup_printf("Current euser %s is a member of group i2c", cur_euname);
         g_ptr_array_add(collector, s);
      }
      if (!in_group_i2c) {
         if (streq(cur_uname, "root")) {
            g_ptr_array_add(collector, strdup("Current user is root, membership in group i2c not needed"));
         }
         else {
            char * s = g_strdup_printf("WARNING: Current user %s is NOT a member of group i2c", cur_uname);
            g_ptr_array_add(collector, s);
            if (uid != euid) {
               char * s1 = g_strdup_printf("WARNING: Current euser %s is NOT a member of group i2c", cur_euname);
               g_ptr_array_add(collector, s1);
            }
         }
      }
   }
   else {
      g_ptr_array_add(collector, strdup("Group i2c does not exist"));
   }
   result = grp_i2c_exists && cur_user_in_group_i2c;
   DBGF(debug, "Returning: %s", sbool(result));
   return result;
}


bool cur_user_in_group_i2c() {
   bool debug = true;
   GPtrArray * collector = g_ptr_array_new_with_free_func(g_free);
   bool result = check_group_i2c_collect(collector);
   if (debug) {
      for (int ndx = 0; ndx < collector->len; ndx++) {
         DBG("%s", g_ptr_array_index(collector, ndx));
      }
   }
   g_ptr_array_free(collector, true);
   return result;
}


/** Gets the owner and group ids for a file.
 *
 *  @param fqfn     name of file to check
 *  @param uid_loc  where to return owner id
 *  @param gid_loc  where to return group id
 *  @return false if stat() failed, true otherwise
 */
bool get_file_owner_group_ids(const char * fqfn, uid_t* uid_loc, gid_t* gid_loc) {
   *uid_loc = -1;
   *gid_loc = -1;

   struct stat st;
   bool ok = true;

   if (stat(fqfn, &st) == -1) {
      perror("stat");
      ok = false;
   }
   else {
      *uid_loc = st.st_uid;
      *gid_loc = st.st_gid;
   }
   return ok;
}


bool is_file_group_i2c(const char * fqfn) {
   bool result = false;
   uid_t uid;
   gid_t gid;

   bool ok = get_file_owner_group_ids(fqfn, &uid, &gid);
   if (ok) {
      if (streq(gid_name(gid), "i2c"))
         result = true;
   }
   return result;
}
