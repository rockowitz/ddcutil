/** @file linux_util.c
 *
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2020-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <execinfo.h>      // for segv handler
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <signal.h>        // for segv handler
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

//for acl
#include <sys/acl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <syslog.h>
#endif
/** \endcond */

#include "common_inlines.h"
#include "debug_util.h"
#ifdef USE_DBUS
#include "dbus_util.h"
#endif
#include "file_util.h"
#include "report_util.h"
#include "string_util.h"
#include "subprocess_util.h"
#include "timestamp.h"
#include "traced_function_stack.h"

#include "linux_util.h"


/** Tests whether a file is readable by trying to read from it, as opposed to
  * considering all the rules re permissions, file type, links, etc.
  *
  * \param filename
  * \return true if file can be read from false if not
  */
bool is_readable_file(const char * filename)
{
   // avoid all the rules re permissions, file type, links, ls etc
   // just try to read from the file
   bool result = false;
   int fd = open(filename, O_RDONLY);
   if (fd >= 0) {
      char buf;
      if (read(fd, &buf, 1) > 0)
         result = true;
      close(fd);
   }
   return result;
}


/** Gets the value of a kernel configuration parameter from file
  * /boot/config-KERNEL_RELEASE", where KERNEL_RELEASE is the kernel release name.
  *
  * \param     parm_name   parameter name
  * \param     buffer      buffer in which to return value
  * \param     bufsz       size of buffer
  * \param     1           configuration parm found, value is in buffer
  * \retval    0           configuration parm not found
  * \retval    < 0         error reading configuration file
  */
int get_kernel_config_parm(const char * parm_name, char * buffer, int bufsz)
{
   bool debug = false;
   DBGF(debug, "Starting. parm_name=%s, buffer=%p, bufsz=%d", parm_name, buffer, bufsz);
   buffer[0] = '\0';

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char config_fn[100];
   snprintf(config_fn, 100, "/boot/config-%s", utsbuf.release);

   char search_str[40];
   snprintf(search_str, 40, "%s=", parm_name);
   DBGF(debug, "search_str=|%s|, len=%ld", search_str, (unsigned long) strlen(search_str));

   GPtrArray * lines = g_ptr_array_new_full(15000, g_free);
   char * terms[2];
   terms[0] = search_str;
   terms[1] = NULL;
   int unfiltered_ct = read_file_with_filter(lines, config_fn, terms, false, 0, true);
   DBGF(debug, "read_file_with_filter() returned %d, lines->len=%d", unfiltered_ct, lines->len);
   if (unfiltered_ct < 0) {
      rc = unfiltered_ct;  // -errno
   }
   else if (lines->len == 0) {   // count after filtering
      rc = 0;
   }
   else {
      assert(lines->len == 1);
      char * aline = g_ptr_array_index(lines, 0);
      char * value = aline + strlen(search_str);
      DBGF(debug, "strlen(search_str)=%ld aline=%p->|%s|, value=%p->|%s|",
                   (unsigned long)strlen(search_str), aline, aline, value, value);
      assert(strlen(value) < bufsz);
      // snprintf(buffer, bufsz, "%s", value);
      strcpy(buffer, value);
      rc = 1;
   }
   g_ptr_array_free(lines, true);

   DBGF(debug, "rc=%d, strlen(buffer) = %ld, buffer=|%s|",
                rc, (unsigned long) strlen(buffer), buffer);

    ASSERT_IFF(rc==1, strlen(buffer) > 0);
    DBGF(debug, "Done. parm=%s, returning %d, result=%s", parm_name, rc, buffer);
    return rc;
}


/** Checks whether a module file exists for the current kernel.
 *
 *  Name variants using underscores (_) and hyphens (-) are both checked.
 *
 *  Allows for extension .ko.xz etc. as well as .ko.
 *
 *  @param  module_name  name of module
 *  @retval true  file exists
 *  @retval false file does not exist
 */
bool find_module_ko(const char * module_name) {
   bool debug = false;
   DBGF(debug, "Starting. module_name: %s", module_name);

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char * module_name1 = strdup(module_name);
   char * module_name2 = strdup(module_name);
   str_replace_char(module_name1, '-','_');
   str_replace_char(module_name2, '_','-');

   bool result = false;
   char cmd[200];
   g_snprintf(cmd, 200, "find /lib/modules/%s -name \"%s.ko*\" -o -name \"%s.ko*\"",
         utsbuf.release, module_name1, module_name2);
   DBGF(debug, "cmd |%s|", cmd);
   GPtrArray * cmd_result = execute_shell_cmd_collect(cmd);
   if (cmd_result) {
      DBGF(debug, "len=%d", cmd_result->len);
      if (cmd_result->len > 0) {
         DBGF(debug, "Found: %s", (char*) g_ptr_array_index(cmd_result,0));
         result = true;
      }
      g_ptr_array_free(cmd_result,true);
   }
   free(module_name1);
   free(module_name2);

   DBGF(debug, "Done.  Returning %s", sbool(result));
   return result;
}


/** Examines file /lib/modules/<kernel release>/modules/builtin to determine
 *  whether a module is built into the kernel.
 *
 *  Name variants using underscores (_) and hyphens (-) are both checked.
 *
 *  Allows for extension .ko.xz etc. as well as .ko.
 *
 *  @param  module_name  name of module
 *  @retval true  module is built in
 *  @retval false module is not built in, or modules/builtin file not found
 *
 *  @remark
 *  It is possible that modules/builtin does not exist for some incorrectly
 *  built kernel.
 */
bool is_module_built_in(const char * module_name) {
   bool debug = false;
   DBGF(debug, "Starting. module_name = |%s|", module_name);

   // Look for name variants with either "-" or "_"
   char * module_name1 = g_strdup_printf("%s.ko", module_name);
   char * module_name2 = g_strdup_printf("%s.ko", module_name);
   str_replace_char(module_name1, '-','_');
   str_replace_char(module_name2, '_','-');

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char builtin_fn[PATH_MAX];
   g_snprintf(builtin_fn, PATH_MAX, "/lib/modules/%s/modules.builtin", utsbuf.release);
   bool found = false;
#ifdef ALT
   if ( !regular_file_exists(builtin_fn) ) {
      fprintf(stderr, "File not found: %s\n", builtin_fn);
   }
   else {
      char cmd[200];
      // not everything is under kernel/drivers e.g. fbdev.ko is under kernel/arch/x86/video
      g_snprintf(cmd, 200, "grep  -e \"^kernel/.*/%s\" -e \"^kernel/.*/%s\"  %s ",
                 module_name1, module_name2, builtin_fn);  // allow for .ko.xz etc.
      DBGF(debug, "cmd |%s|", cmd);
      GPtrArray * cmd_result = execute_shell_cmd_collect(cmd);
      if (cmd_result) {
         DBGF(debug, "len=%d", cmd_result->len);
         if (cmd_result->len > 0) {
            found = true;
         }
         g_ptr_array_free(cmd_result,true);
      }
   }
#else
    GPtrArray * lines = g_ptr_array_new_full(400, g_free);
    char * terms[3];
    terms[0] = module_name1;
    terms[1] = module_name2;  // probably same as module_name1, but not worth optimizing
    terms[2] = NULL;
    int unfiltered_ct = read_file_with_filter(lines, builtin_fn, terms, false, 0, false);
    if (unfiltered_ct < 0) {   //  = -errno
       fprintf(stderr, "Error reading file %s: %s\n", builtin_fn, strerror(-unfiltered_ct));
       fprintf(stderr, "Assuming module %s is not built in to kernel\n", module_name);
    }
    else {
       found = (lines->len == 1);
    }
    g_ptr_array_free(lines, true);
#endif

   free(module_name1);
   free(module_name2);

   DBGF(debug, "Done.    module_name=%s, Returning %s", module_name, sbool(found));
   return found;
}


char * kernel_module_types[] = {
      "KERNEL_MODULE_NOT_FOUND",          // 0
      "KERNEL_MODULE_BUILTIN",            // 1
      "KERNEL_MODULE_LOADABLE_FILE"};     // 2

/** Checks if a module is built into the kernel and, if not, checks if a
 *  loadable kernel module file exists.
 *
 *  @param  module name
 *  @retval KERNEL_MODULE_NOT_FOUND      not found
 *  @retval KERNEL_MODULE_BUILTIN        kernel module is built in
 *  @retval KERNEL_MODULE_LOADABLE_FILE  kernel module is a loadable file
 */
int module_status_by_modules_builtin_or_existence(const char * module_name) {
   bool debug = false;
   int result = KERNEL_MODULE_NOT_FOUND;
   if ( is_module_built_in(module_name) )
      result = KERNEL_MODULE_BUILTIN;
   else {
      bool found = find_module_ko(module_name);
      if (found) {
         result = KERNEL_MODULE_LOADABLE_FILE;
      }
   }
   DBGF(debug, "Executed. module_name=%s, returning %d = %s",
               module_name, result, kernel_module_types[result]);
   return result;
}


/** Examines file /boot/config-<kernel version> to determine whether module
 *  i2c-dev exists and if so whether it is built into the kernel or is a
 *  loadable module.
 *
 *  @retval y   built into kernel
 *  @retval m   built as loadable module
 *  @retval n   not built
 *  @retval X   /boot/config file not found, or CONFIG_I2C_CHARDEV line not found
 */
char i2c_dev_status_by_boot_config_file() {
   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char config_fn[PATH_MAX];
   g_snprintf(config_fn, PATH_MAX, "/boot/config-%s", utsbuf.release);

   char status = 'X';
   if ( !regular_file_exists(config_fn) ) {
      fprintf(stderr, "Kernel configuration file not found: %s\n", config_fn);
   }
   else {
      char cmd[100];
      g_snprintf(cmd, 100, "grep CONFIG_I2C_CHARDEV= /boot/config-%s", utsbuf.release);
      int pos = strlen("CONFIG_I2C_CHARDEV=");

      char * cmd_result = execute_shell_cmd_one_line_result(cmd);
      if (!cmd_result) {
         fprintf(stderr, "CONFIG_I2C_CHARDEV not found in %s\n", config_fn);
      }
      else {
         status = cmd_result[pos];
         free(cmd_result);
      }
   }
   return status;
}


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


#ifdef UNUSED
char * get_file_owner(const char * fqfn) {
   struct stat st;
   char * result = NULL;

   if (stat(fqfn, &st) == -1) {
      perror("stat");
   }

   struct passwd *pw = getpwuid(st.st_uid);
   //    struct group  *gr = getgrgid(st.st_gid);

   printf("Owner: %s\n", pw ? pw->pw_name : "unknown");
   // printf("Group: %s\n", gr ? gr->gr_name : "unknown");

   result = (pw) ? pw->pw_name : "unknown";
   return result;
}


char * get_file_group(const char * fqfn) {
   struct stat st;
   char * result = NULL;

   if (stat(fqfn, &st) == -1) {
      perror("stat");
   }

   // struct passwd *pw = getpwuid(st.st_uid);
   struct group  *gr = getgrgid(st.st_gid);

   // printf("Owner: %s\n", pw ? pw->pw_name : "unknown");
   printf("Group: %s\n", gr ? gr->gr_name : "unknown");

   result = (gr) ? gr->gr_name : "unknown";
   return result;
}
#endif


/** Gets the owner and group ids for a file.
 *
 *  @param fqfn     name of file to check
 *  @param uid_loc  where to return owner id
 *  @param gid_loc  where to return group id
 *  @return false if stat() failed, true otherwise
 */
bool get_file_owner_group_ids(const char * fqfn, int * uid_loc, int * gid_loc) {
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


/** Returns the name for a user id
 *
 *  @param uid  user id
 *  @return name of user, or "unknown" if unrecognized
 *
 *  The returned string should not be freed.?
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
 *  The returned string should not be freed.?
 */
char * gid_name(int gid) {
   struct group * gr = getgrgid(gid);
   char*  uid_name = gr ? gr->gr_name : "unknown";
   return uid_name;
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


/** Report processes that have a file open
 *
 *  The report is written to the current output destination.
 *
 *  @param  fqfn  file name
 *  @param  depth logical indentation depth
 */
void rpt_lsof(const char * fqfn, int depth) {
   // rpt_vstring(depth, "Programs with %s open:");
   char cmd[PATH_MAX+20];
   g_snprintf(cmd, PATH_MAX+20, "lsof %s", fqfn);
   execute_shell_cmd_rpt(cmd, depth);
}


/** Collects information about processes that have a file open,
 *  and returns it as an array of lines.
 *
 *  @param  fqfn  file name
 *  @param  collector  if NULL, allocate new GPtrArray
 *  @return GPtrArray of lines
 */
// converge with show_lsof() in flock.c
// to do: tailor the output to what is useful
GPtrArray* rpt_lsof_collect0(const char * fqfn, GPtrArray * collector) {
   if (!collector)
      collector = g_ptr_array_new_with_free_func(g_free);

   char cmd[PATH_MAX+20];
   g_snprintf(cmd, PATH_MAX+20, "lsof %s", fqfn);
   char * emsg_loc = NULL;
   GPtrArray* conflicts = execute_shell_cmd_collect1(cmd, NULL, &emsg_loc);
   if (emsg_loc) {
      g_ptr_array_add(collector, emsg_loc);
   }
   if (conflicts) {
      if (conflicts->len  > 0) {
         g_ptr_array_add(collector,  g_strdup_printf("file %s also open by:", fqfn));
         for (int ndx = 0; ndx < conflicts->len; ndx++) {
            g_ptr_array_add(collector,
                  g_strdup_printf("   %s", (char*)g_ptr_array_index(conflicts, ndx)));
         }
      }
      else
         g_ptr_array_add(collector, g_strdup_printf("No open conflicts found for %s", fqfn));
      g_ptr_array_free(conflicts, true);
   }

   return collector;
}


/** Collects information about processes that have a file open,
 *  and returns it as a newly allocated array of lines.
 *
 *  @param  fqfn  file name
 *  @return GPtrArray of lines
 */
GPtrArray* rpt_lsof_collect(const char * fqfn) {
   return rpt_lsof_collect0(fqfn, NULL);
}


/** Collects information about the access control list (ACL) for a file,
 *  using the libacl API, and returns it as an array of lines.
 *
 *  The output is similar to, but not identical to, that of command **facl**.
 *  In particular, the effective permissons, after mask application, are
 *  not reported.
 *
 *  @param  fqfn  file name
 *  @param  collector   if NULL, allocate new GPtrArray
 *  @param  depth   logical indentation depth for formatting lines
 *  @return GPtrArray of lines
 */
GPtrArray * rpt_facl_collect0(const char * fqfn, GPtrArray * collector, int depth) {
   if (!collector)
      collector = g_ptr_array_new_with_free_func(g_free);

   acl_t acl = acl_get_file(fqfn, ACL_TYPE_ACCESS);
   if (acl == NULL) {
      char * s = g_strdup_printf("acl_get_file(\"%s\") failed. errno=%d", fqfn, errno);
      g_ptr_array_add(collector, s);
      goto bye;
    }

    char *text;
    ssize_t len;
    text = acl_to_text(acl, &len);
    if (text == NULL) {
        char * s = g_strdup_printf("acl_to_text() failed. errno=%d", errno);
        g_ptr_array_add(collector, s);
        acl_free(acl);
        goto bye;
    }

    Null_Terminated_String_Array ntsa = strsplit(text, "\n\r");
    GPtrArray * lines = ntsa_to_g_ptr_array(ntsa);
    // DBG("lines->len = %d", lines->len);
    for (int ndx = 0; ndx < lines->len; ndx++) {
       char * s = g_strdup_printf("%*s%s", depth, " ", ntsa[ndx]);
       // DBG("s: %s", s);
       g_ptr_array_add(collector, s);
    }
    ntsa_free(ntsa,  true);
    g_ptr_array_free(lines, false);

    // free(text); // invalid free(), sample code  had acl_free_text()
    acl_free(acl);

bye:
   return collector;
}


#ifdef UNUSED
GPtrArray * rpt_facl_collect(const char * fqfn) {
   return rpt_facl_collect0(fqfn, NULL, 0);
}
#endif

#ifdef UNUSED
void report_facl(const char * fqfn, int depth) {
   GPtrArray * lines = rpt_facl_collect0(fqfn, NULL, depth);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * s = g_ptr_array_index(lines, ndx);
      rpt_vstring(depth, "%s", s);
   }
   g_ptr_array_free(lines, true);
}
#endif

#ifdef UNUSED
void report_facl_to_syslog(const char * fqfn, int log_level, int depth) {
   GPtrArray * lines = rpt_facl_collect0(fqfn, NULL, depth);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * s = g_ptr_array_index(lines, ndx);
      syslog(log_level, "%s", s);
   }
   g_ptr_array_free(lines, true);
}
#endif


/** Collects information regarding an unexpected open() failure, and
 *  returns it as an array of lines.
 *
 *  @param   fqfn  file name
 *  @param   msg   if non-NULL, start with this message
 *  @return  array of lines (caller must free)
 */
GPtrArray* diagnose_open_failure_collect(const char * fqfn,
                                         const char * msg,
                                         GPtrArray * collector)
{
   bool debug = false;
   DBGF(debug, "Starting.  fqfn=%s, msg=%s, collector=%p", fqfn, msg, collector);

   if (!collector)
      collector = g_ptr_array_new_with_free_func(g_free);
   if (msg)
      g_ptr_array_add(collector, (char*) strdup(msg));


   char * s3 = g_strdup_printf("Elapsed time since start of program execution: %s seconds",
        formatted_elapsed_time_t(6));
   DBGF(debug, "%s", s3);
   g_ptr_array_add(collector, s3);

   int uid  = (int) getuid();
   int euid = (int) geteuid();
   int gid  = (int) getgid();
   int egid = (int) getegid();

   int depth = 5;
   g_ptr_array_add(collector,
         g_strdup_printf("%*suid: %d=%s, euid: %d=%s, gid: %d=%s, egid: %d=%s",
         depth, " ",
         uid,  uid_name(uid),
         euid, uid_name(euid),
         gid,  gid_name(gid),
         egid, gid_name(egid)));

   int file_uid = -1;
   int file_gid = -1;
   bool ok = get_file_owner_group_ids(fqfn, &file_uid, &file_gid);
   if (ok) {
      g_ptr_array_add(collector,
         g_strdup_printf("%*sfile owner: %d=%s, file group: %d=%s",
         depth, " ",
         file_uid, uid_name(file_uid),
         file_gid, gid_name(file_gid)));
   }
   else {
      g_ptr_array_add(collector,
            g_strdup_printf("%*sUnable to determine file ownership", depth, " "));
   }

   g_ptr_array_add(collector, strdup("Using command getfacl: "));
   char cmd[PATH_MAX+20];
   g_snprintf(cmd, PATH_MAX+20, "getfacl %s  --all-effective" , fqfn);
   char * errmsg = NULL;
   execute_shell_cmd_collect1(cmd, collector, &errmsg);
   if (errmsg) {
      g_ptr_array_add(collector, errmsg);
      fprintf(stderr, "%s   (A)\n", errmsg);
   }

   g_ptr_array_add(collector, strdup( "Using acl api:"));
   rpt_facl_collect0(fqfn, collector, depth);
   rpt_lsof_collect0(fqfn, collector);

#ifdef USE_DBUS
#ifdef WRONG
  uint64_t interval_millis = 5000;
  uint64_t resumed_millisec = millisec_since_resumed_from_sleep();

  char * s0 = g_strdup_printf("millisec_since_returned_from_sleep(): %"PRIu64, resumed_millisec);
  DBGF(debug, "%s", s0);
  g_ptr_array_add(collector, s0);
  bool recently_returned =  (resumed_millisec < interval_millis);
  char * s1 = g_strdup_printf("interval_millis: %"PRIu64", recently returned = %s",
        interval_millis, SBOOL(recently_returned));
  DBGF(debug, s1);
  g_ptr_array_add(collector, s1);
  free(s1);
#endif

   uint64_t elapsed_ns = ldbus_elapsed_since_resume_from_sleep_ns();
   char * s2 = NULL;
   if (elapsed_ns == UINT64_MAX)
      s2 = strdup("No resume from sleep recorded");
   else
      s2 = g_strdup_printf(""
           "Time since last resume from sleep: %s seconds = %"PRIu64" millisec (%"PRIu64 "nanosec)",
           formatted_time_t(elapsed_ns), NANOS2MILLIS(elapsed_ns), elapsed_ns);
   DBGF(debug, "%s", s2);
   g_ptr_array_add(collector, strdup(s2));
   free(s2);
#endif

   DBGF(debug, "Done.    returning collector = %p", collector);
   return collector;
}


#ifdef UNUSED
/** Writes a report regarding an unexpected open() failure to the terminal.
 *
 *  @param   fqfn  file name
 *  @param   msg   if non-NULL, first line of the report
 */
void diagnose_open_failure(const char * fqfn, const char * msg) {
   bool debug = false;
   DBGF(debug, "Starting, fqfn=%s, msg=%s", fqfn, msg);
   GPtrArray * lines = diagnose_open_failure_collect(fqfn, msg, NULL);
   // rpt_facl_collect0(fqfn, lines, 1);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * s = g_strdup_printf("%s",
                                 (char*) g_ptr_array_index(lines, ndx));
      rpt_vstring(0, "%s", s);
      // syslog(LOG_DEBUG, "%s", s);
   }
   g_ptr_array_free(lines, true);
   DBGF(debug, "Done.");
}
#endif


/** Writes a report regarding an unexpected open() failure to the system log
 *
 *  @param   fqfn  file name
 *  @param   msg   if non-NULL, first line of the report
 */
void diagnose_open_failure_to_syslog(const char * fqfn, const char * msg) {
   bool debug = false;
   DBGF(debug, "Starting.  fqfn=%s, msg=%s", fqfn, msg);
   int depth = 3;

   GPtrArray * lines = diagnose_open_failure_collect(fqfn, msg, NULL);
   //  rpt_facl_collect0(fqfn, lines, 1);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * cur_line = g_ptr_array_index(lines,ndx);
      // DBG("cur_line |%s|", cur_line);
      char * s = g_strdup_printf("[%6jd] %*s%s",
                                 TID(), depth, " ", cur_line);
      // rpt_vstring(0, "%s", s);
      syslog(LOG_DEBUG, "%s", s);
      g_free(s);
   }
   g_ptr_array_free(lines, true);

   DBGF(debug, "Done");
}


// SEGFAULT handler

static struct sigaction old_segv;

/** Handler for segmentation faults.  Logs the fault and dumps the traced function
 *  stack syslog, then invokes the previous handler.
 *
 *  @param sig      signal number (should be SIGSEGV)
 *  @param info     pointer to siginfo_t structure with details about the signal
 *  @param ucontext pointer to ucontext_t structure with context at time of signal
 */
static void segv_handler(int sig, siginfo_t *info, void *ucontext) {
#ifdef BACKTRACE
   // Show backtrace
    void *frames[32];
    int n = backtrace(frames, 32);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
#endif

    syslog(LOG_ERR, "Segmentation fault (signal %d)", sig);
    current_traced_function_stack_to_syslog(LOG_ERR, TFS_MOST_RECENT_LAST);

    sigaction(SIGSEGV, &old_segv, NULL);

    if (old_segv.sa_flags & SA_SIGINFO) {
        old_segv.sa_sigaction(sig, info, ucontext);
    } else if (old_segv.sa_handler == SIG_DFL) {
        raise(SIGSEGV);
    } else if (old_segv.sa_handler != SIG_IGN) {
        old_segv.sa_handler(sig);
    }
}


void install_segv_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, NULL, &old_segv);
    sigaction(SIGSEGV, &sa, NULL);
}
