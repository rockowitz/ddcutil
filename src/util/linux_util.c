/** @file linux_util.c
 *
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2020-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <signal.h>        // for segv handler
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <syslog.h>
#endif
/** \endcond */

#include "common_inlines.h"
#include "acl_util.h"
#include "debug_util.h"
#ifdef USE_DBUS
#include "dbus_util.h"
#endif
#include "file_util.h"
#include "linux_basic_util.h"
#include "report_util.h"
#include "string_util.h"
#include "subprocess_util.h"
#include "syslog_util.h"
#include "timestamp.h"
#include "traced_function_stack.h"

#include "linux_util.h"

//
// Miscellaneous
//

/** Tests whether a file is readable by trying to read from it, as opposed to
  * considering all the rules re permissions, file type, links, etc.
  *
  * \param filename
  * \return true if file can be read from false if not
  */
bool is_readable_file(const char * filename) {
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
 *  @param  collector - if NULL, allocate new GPtrArray
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


//
// Checking for kernel module i2c-dev existence and status
//

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
static bool find_module_ko(const char * module_name) {
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
 *  loadable module. It does so by checking the value of CONFFIG_I2C_CHARDEV.
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


//
// Diagnose open() failure
//

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

   G_PTR_ARRAY_ADD_STRING(collector,
         "Elapsed time since start of program execution: %s seconds",
         formatted_elapsed_time_t(6));

#ifdef USE_DBUS
   uint64_t elapsed_ns = ldbus_elapsed_since_resume_from_sleep_ns();
   if (elapsed_ns == UINT64_MAX)
      g_ptr_array_add(collector, strdup("No resume from sleep recorded"));
   else
      G_PTR_ARRAY_ADD_STRING(collector,
         "Time since last resume from sleep: %s seconds = %"PRIu64" millisec (%"PRIu64 "nanosec)",
         formatted_time_t(elapsed_ns), NANOS2MILLIS(elapsed_ns), elapsed_ns);
#endif

   // n.b. this reporting call mutates the clocktime detector's per-thread
   // state: it consumes the detection and opens the 5 second grace window.
   // It was formerly harmless because i2c_open_bus_basic() paused, and so
   // consulted the detector, before attempting any open; that pre-open pause
   // has since been removed in favor of retrying after EACCES, so on the
   // EACCES path this may now be the first call on the thread and may open
   // the window rather than extend one.  Still harmless, because the window
   // only makes a subsequent caller pause the remainder of its interval, but
   // a diagnostic should not be deciding that.  Do not rely on the ordering.
   bool recent =  recently_resumed_from_sleep_by_clocktime(NULL);
   G_PTR_ARRAY_ADD_STRING(collector, "recently_returned_from_sleep() returned %s", sbool(recent));

   int uid  = (int) getuid();
   int euid = (int) geteuid();
   int gid  = (int) getgid();
   int egid = (int) getegid();

   int depth = 0;
   g_ptr_array_add(collector,
         g_strdup_printf("%*suid: %d=%s, euid: %d=%s, gid: %d=%s, egid: %d=%s",
         depth, " ",
         uid,  uid_name(uid),
         euid, uid_name(euid),
         gid,  gid_name(gid),
         egid, gid_name(egid)));

   check_group_i2c_collect(collector);

   G_PTR_ARRAY_ADD_STRING(collector, "Permissions for %s:", fqfn);
   uid_t file_uid = -1;
   gid_t file_gid = -1;
   bool ok = get_file_owner_group_ids(fqfn, &file_uid, &file_gid);
   if (ok) {
      G_PTR_ARRAY_ADD_STRING(collector,
         "%*sfile owner: %d=%s, file group: %d=%s",
         depth, " ",
         file_uid, uid_name(file_uid),
         file_gid, gid_name(file_gid));
   }
   else {
      G_PTR_ARRAY_ADD_STRING(collector,
            "%*sUnable to determine file ownership", depth, " ");
   }

   int rc = access(fqfn, R_OK|W_OK);
   if (rc < 0)
      G_PTR_ARRAY_ADD_STRING(collector, "access(%s) failed, errno=%d", fqfn, errno);
   else
      G_PTR_ARRAY_ADD_STRING(collector, "access(%s) succeeded", fqfn);
   rc = faccessat(0,fqfn, R_OK|W_OK, AT_EACCESS);
   if (rc < 0)
      G_PTR_ARRAY_ADD_STRING(collector, "faccessat(0,%s,R_OK|W_OK, AT_EADCESS) failed, errno=%d", fqfn, errno);
   else
      G_PTR_ARRAY_ADD_STRING(collector, "faccessat(%s) succeeded", fqfn);

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
   g_ptr_array_add(collector, strdup( "Using low level acl api:"));
   rpt_facl_collect1(fqfn, collector, depth);
   rpt_lsof_collect0(fqfn, collector);

   char * sacl = get_user_acl(fqfn, uid);
   G_PTR_ARRAY_ADD_STRING(collector, "acl for user %d: %s", uid, sacl);
   free(sacl);


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


//
// SEGFAULT handler
//

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

   SIMPLE_STD_SYSLOG(LOG_ERR, "Segmentation fault (signal %d)", sig);
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


/** Installs a SIGSEGV handler that logs the fault and traced function stack
 *  to syslog before re-raising the signal to invoke the previous handler.
 */
void install_segv_handler(void) {
   struct sigaction sa;
   memset(&sa, 0, sizeof sa);
   sa.sa_sigaction = segv_handler;
   sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGSEGV, NULL, &old_segv);
   sigaction(SIGSEGV, &sa, NULL);
}


//
// Detect resume from sleep using CLOCK_BOOTTIME and CLOCK_MONOTONIC.
//
// Detects if resume from sleep has occurred by detecting changes in the
// accumulated sleep time, by using CLOCK_BOOTTIME and CLOCK_MONOTONIC.
// BOOTTIME advances during sleep; MONOTONIC does not.
// Their difference = cumulative time spent asleep since the algorithm was
// Started. An increase in the cumulative time asleep since vs the previous
// value indicates a resume occurred.
//
// This is a cruder mechanism than watching for dbus PrepareForSleep
// signals, but does not require dbus.  It also has the advantage that
// this algorithm will indicate that a sleep has occurred if immediately
// executed after resume, whereas there is a sliver of time between
// when execution resumed and the arrival of the of dbus message.

/** Global baseline set at startup by #init_baseline_accumulated_sleep_ns().
 *  UINT64_MAX means not yet initialized.
 */
static uint64_t global_initial_accumulated_sleep_ns = UINT64_MAX;

/** Per-thread baseline, initialized from the global on each thread's first call.
 *  UINT64_MAX means this thread has not yet initialized its baseline.
 */
static _Thread_local uint64_t previous_accumulated_sleep_ns = UINT64_MAX;

/** CLOCK_BOOTTIME ms at which this thread most recently DETECTED a resume,
 * i.e. the start of the 5-sec "recently resumed" grace window, not its end.
 * The window runs until this value plus 5 sec, and
 * millisec_since_resume_detected_by_clocktime() measures from it.
 * See recently_resumed_from_sleep_by_clocktime().
 *
 * UINT64_MAX, not 0, means no resume has yet been detected on this thread.
 * 0 is a valid CLOCK_BOOTTIME value, and using it as the sentinel put every
 * thread inside the grace window for the first 5 seconds after boot.
 */
static _Thread_local uint64_t most_recent_detection_ms = UINT64_MAX;


/** Gets the current accumulated sleep time. 
 * 
 *  @param accumulated sleep time in nanaosec
 */
static uint64_t get_accumulated_sleep_ns() {
   struct timespec bt;
   struct timespec mt;
   clock_gettime(CLOCK_BOOTTIME,  &bt);  // advances during sleep
   clock_gettime(CLOCK_MONOTONIC, &mt);  // does not advance during sleep
   uint64_t boottime_ns = SECS2NANOS(bt.tv_sec) + bt.tv_nsec;
   uint64_t mono_ns     = SECS2NANOS(mt.tv_sec) + mt.tv_nsec;
   uint64_t accumulated_sleep_ns = boottime_ns - mono_ns;
   return accumulated_sleep_ns;
}


/** Records the current accumulated sleep time as the global baseline.
 *
 *  Must be called once at program startup before any additional threads are
 *  created. Each thread's per-thread baseline is seeded from this value on its
 *  first call to #recently_resumed_from_sleep(), so all threads can detect
 *  resumes that occur after this baseline was recorded.
 */
void init_accumulated_sleep() {
   global_initial_accumulated_sleep_ns = get_accumulated_sleep_ns();
}

#ifdef UNUSED
void reset_recently_resumed_by_clocktime_cache() {
   most_recent_detection_ms = UINT64_MAX;   // i.e. no resume yet detected
}
#endif


/** Detects whether the system has resumed from sleep.
 *
 *  Uses the difference between CLOCK_BOOTTIME (advances during sleep) and
 *  CLOCK_MONOTONIC (does not advance during sleep) to measure cumulative
 *  sleep time.  An increase since the prior call on this thread indicates
 *  that a resume occurred.
 *
 *  Once a resume is detected, calls on the same thread within the following
 *  5 seconds (the grace window) also return true, so that multiple call
 *  sites on a thread can each observe the resume.  See the discussion of
 *  the tradeoffs in the function body.
 *
 *  @param  detected_now_loc  if non-NULL, set to true if THIS call detected
 *          the resume, false if it answered from the grace window or found
 *          no resume.  A caller weighing this detector against another
 *          source needs the distinction: a detection on this call means
 *          sleep accumulated that this thread had not yet accounted for, so
 *          another source may not have processed the corresponding event
 *          either.  Within the grace window that is no longer true, the
 *          resume having been observed at least once already.
 *  @return true if a resume from sleep was detected on this call, or was
 *          detected on this thread within the past 5 seconds
 */
bool recently_resumed_from_sleep_by_clocktime(bool * detected_now_loc) {
   bool debug = false;
   bool resumed = false;
   bool detected_now = false;

   uint64_t cur_boottime_ms = NANOS2MILLIS( cur_boot_time_nanosec());

   // Grace window vs one-shot detection.
   //
   // The delta check in the else branch below is inherently one-shot: it
   // updates previous_accumulated_sleep_ns the moment it detects a resume,
   // so on the very next call the increase is ~0 and it reports no resume.
   // This branch converts that single observation into a state: for 5
   // seconds after a detection, every call on this thread reports
   // "recently resumed".  most_recent_detection_ms does not reset the
   // detector's baseline (previous_accumulated_sleep_ns does that); it
   // records when the window opened.
   //
   // The tradeoff.  One-shot, i.e. this function without this branch, is
   // the simpler contract, with no window length to choose, but the
   // observation is consumed by whichever caller asks first, so multiple
   // call sites on one thread cannot coexist.  Concretely: the priming
   // call in dw_udev_watch(), made before the post-add-event sleep
   // precisely so that its reference point precedes that sleep, would
   // swallow the detection, and the call to
   // dw_pause_if_recently_resumed_from_sleep() on the next iteration of
   // the watch loop would never see it and never pause.  (Not the same
   // iteration: an add event makes that iteration's guard false.)  Both
   // run on the watch thread, and the detector's state is per thread.
   // With the window, "recently
   // resumed" is a state that any number of callers can query for a
   // bounded period.  The cost is that true is returned for the whole
   // window, so a caller acting on the bare boolean would act repeatedly;
   // callers must consult millisec_since_resume_detected_by_clocktime()
   // and act only on the remainder of their own interval, as
   // dw_pause_if_recently_resumed_from_sleep() does.

   // Sample on every call, including inside the grace window.  Returning
   // early from the window without sampling would leave the baseline stale,
   // so a second suspend beginning during the window went undetected: the
   // window would expire measured from the FIRST resume while the second had
   // never been seen at all.
   uint64_t current_accumulated_sleep_ns  = get_accumulated_sleep_ns();

   if (previous_accumulated_sleep_ns == UINT64_MAX) {
      // First call on this thread: seed from the global baseline if available,
      // otherwise fall back to current value (no resume detectable this call).
      previous_accumulated_sleep_ns =
            (global_initial_accumulated_sleep_ns != UINT64_MAX)
               ? global_initial_accumulated_sleep_ns
               : current_accumulated_sleep_ns;
   }
   // Accumulated sleep is the difference of two separately sampled clocks,
   // so successive values jitter by the interval between the two reads,
   // a few microseconds, and are not monotonic.  Subtracting unsigned when
   // the newer value is the smaller wraps to nearly 2**64, which passes the
   // threshold test below and reports a resume that never occurred.  Guard
   // the subtraction, and on a decrease take the lower value as the new
   // baseline so that a high sample is not latched, leaving every
   // subsequent sample looking like a decrease.

   // Compare in uint64_t nanoseconds throughout -- narrowing to int
   // milliseconds before comparing would overflow for a suspend
   // longer than ~24.8 days (INT_MAX ms).
   uint64_t sleep_increase_ns = 0;
   if (current_accumulated_sleep_ns > previous_accumulated_sleep_ns)
      sleep_increase_ns = current_accumulated_sleep_ns - previous_accumulated_sleep_ns;
   else
      previous_accumulated_sleep_ns = current_accumulated_sleep_ns;
   const uint64_t detection_threshold_secs = 1;
   const uint64_t detection_threshold_ns =  SECS2NANOS(detection_threshold_secs);

   // n.b. the increase is sleep accumulated since this thread's baseline was
   // last written, not the duration of the most recent suspend.  A suspend
   // shorter than the threshold does not update the baseline, so its sleep
   // remains in the sum.  Do not read this value as one suspend's length.
   if (sleep_increase_ns > detection_threshold_ns) {
      // Accumulated sleep grew by > detection_threshold_secs since previous => we resumed.
      resumed = true;
      detected_now = true;
      uint64_t prior_accumulated_sleep_ns = previous_accumulated_sleep_ns;
      previous_accumulated_sleep_ns = current_accumulated_sleep_ns;
      SIMPLE_STD_FUNC_SYSLOG(LOG_INFO,
            "Resume from sleep detected by BOOTTIME/MONOTONIC, sleep increase=%"PRIu64" ms, "
            "previous=%"PRIu64" ms, current=%"PRIu64" ms",
            NANOS2MILLIS(sleep_increase_ns),
            NANOS2MILLIS(prior_accumulated_sleep_ns),
            NANOS2MILLIS(current_accumulated_sleep_ns));
      most_recent_detection_ms = cur_boottime_ms;
   }
   else if (most_recent_detection_ms != UINT64_MAX &&
            (cur_boottime_ms - most_recent_detection_ms) < 5000)
   {
      resumed = true;
      SIMPLE_STD_FUNC_SYSLOG(LOG_DEBUG, "Called within 5 sec of reset");
   }

   DBGF(debug, "previous_accumulated_sleep_ns=%"PRIu64", current_accumulated_sleep_ns=%"PRIu64
               ", detected_now=%s, returning %s",
               NANOS2MILLIS(previous_accumulated_sleep_ns),
               NANOS2MILLIS(current_accumulated_sleep_ns),
               sbool(detected_now), sbool(resumed));

   if (detected_now_loc)
      *detected_now_loc = detected_now;
   return resumed;
}


/** Returns the number of milliseconds since a resume from sleep was last
 *  detected on this thread by recently_resumed_from_sleep_by_clocktime().
 *
 *  Lets callers that pause for a fixed interval after a resume sleep only
 *  the time remaining in that interval, rather than the full interval on
 *  every call within the detector's grace window.
 *
 *  @return milliseconds since the resume was detected,
 *          UINT64_MAX if no resume has been detected on this thread
 */
uint64_t millisec_since_resume_detected_by_clocktime() {
   if (most_recent_detection_ms == UINT64_MAX)
      return UINT64_MAX;
   return NANOS2MILLIS(cur_boot_time_nanosec()) - most_recent_detection_ms;
}


//
// Combined CLOCKTIME/BOOTTIME and dbus algorithm
//

/** Determines whether the system recently resumed from sleep, consulting
 *  every available detection method.
 *
 *  @param  within_ms           interval that defines "recently"
 *  @param  millisec_since_loc  if non-NULL, where to return the number of
 *                              milliseconds since the resume, UINT64_MAX if
 *                              no recent resume
 *  @return true if a resume from sleep occurred within the past **within_ms**
 *
 *  @remark
 *  Callers pause for the time remaining in their own interval, rather than
 *  this function pausing, because each has its own sleep and logging needs.
 *
 *  ddcutil detects a resume from sleep two ways.  They are complementary,
 *  not redundant, and neither alone is sufficient.
 *
 *  The **dbus** method (dbus_util.c) records when the logind
 *  **PrepareForSleep(false)** signal is received.  It is precise, it is
 *  process wide, and it covers three cases the clock method cannot:
 *   - **Program start.**  ldbus_elapsed_since_resume_from_sleep_mark_start()
 *     deliberately treats the start of the sleep watch thread like a resume,
 *     because the window just after boot or login has the same transient
 *     EACCES race on /dev/i2c opens that the window after resume does.  The
 *     clock method reports nothing at startup: no sleep has accumulated.
 *   - **Short suspends.**  The clock method needs more than 1 second of
 *     accumulated sleep before it reports a resume at all.
 *   - **Shared timing.**  last_resume_from_sleep_ns is a single process wide
 *     value, so every thread computes the same elapsed time and pauses only
 *     the remainder of the interval.  The clock method's state is per thread,
 *     so threads detect independently and each starts its own pause from its
 *     own first look, which can multiply the pauses taken.
 *  Its weakness is latency.  The signal is delivered asynchronously, and the
 *  display watch thread can be woken by udev and begin reopening buses before
 *  the dbus thread has processed it.  That interval is exactly when udev has
 *  not yet reapplied the /dev/i2c ACLs, so relying on dbus alone mistakes a
 *  transient permission failure for a permanent one.
 *
 *  The **clock** method (recently_resumed_from_sleep_by_clocktime()) compares
 *  CLOCK_BOOTTIME against CLOCK_MONOTONIC.  It cannot be late: the divergence
 *  is already present the instant the thread runs again, and no signal
 *  delivery is involved.  Its weaknesses mirror the strengths above.  It is
 *  coarse, per thread, silent at startup, and its reference point is when a
 *  thread happened to look, not when the resume occurred.
 *
 *  dbus is therefore preferred where it is trustworthy, and the clock method
 *  covers the case it cannot: the signal not yet delivered.
 *
 *  Neither elapsed time can simply be trusted over the other:
 *   - Taking whichever is smaller prefers the clock method systematically,
 *     since its reference point is the later one whenever detection lags the
 *     resume, and pauses for the full interval well after the resume: resume
 *     at T, dbus signal at T+50 ms, first call on this thread at T+3000 ms,
 *     elapsed reported as 0 rather than 3000.
 *   - Taking dbus whenever it is within the interval lets a timestamp that
 *     predates the suspend, from an earlier resume or from
 *     ldbus_elapsed_since_resume_from_sleep_mark_start(), mask a detection
 *     that is genuinely fresh, and pauses too little.
 *
 *  The tie is broken on whether the clock method detected the resume on this
 *  very call, which it reports through its out parameter.  A detection now
 *  means this thread had not yet accounted for the sleep, so dbus may not
 *  have processed the signal either and its timestamp may predate the
 *  suspend; the clock is preferred.  Inside the grace window the resume has
 *  already been observed once, dbus has had time to catch up, and it is the
 *  more accurate of the two.  See the body for why the two errors are not
 *  symmetric.  Note that the decision deliberately does not depend on
 *  within_ms, which is tunable and must not determine correctness.
 */
bool recently_resumed_from_sleep(int within_ms, uint64_t * millisec_since_loc) {
   bool debug = false;
   bool resumed = false;
   uint64_t millisec_since = UINT64_MAX;

   // Called on every invocation, whatever dbus reports, so that this thread's
   // baseline stays current and its grace window opens when the resume is
   // first observed here.  Otherwise the first call on a thread where dbus
   // always won the race would report a resume that was long since handled.
   bool clock_detected_now = false;
   bool resumed_by_clocktime = recently_resumed_from_sleep_by_clocktime(&clock_detected_now);

#ifdef USE_DBUS
   uint64_t dbus_elapsed_ms = NANOS2MILLIS(ldbus_elapsed_since_resume_from_sleep_ns());

   // A detection on THIS call means sleep accumulated that this thread had
   // not yet accounted for, so dbus may not have processed the corresponding
   // signal either; its timestamp can predate the suspend, and measuring
   // from it would pause far too little.  The clock is preferred in that
   // case.  Within the grace window the resume has been observed at least
   // once already, dbus has had time to catch up, and its timestamp is the
   // more accurate of the two, so it is preferred there.
   //
   // The two errors are not symmetric, which is what settles the direction.
   // Preferring the clock when dbus was in fact current costs one interval
   // of extra pause, latency and nothing more.  Preferring dbus when its
   // timestamp is stale reopens the buses while udev has not yet reapplied
   // the ACLs, which costs the whole EACCES retry ladder, up to
   // max_eacces_retry_ms, plus a diagnostic dump in the system log.
   //
   // Deliberately not decided by comparing the two elapsed times, nor by
   // comparing either against within_ms: within_ms is tunable and must not
   // be what determines correctness.
   if (!clock_detected_now && dbus_elapsed_ms < (uint64_t) within_ms) {
      resumed = true;
      millisec_since = dbus_elapsed_ms;
   }
#endif

   // Fallback, for the case dbus cannot cover: the signal has not yet been
   // delivered, or the build has no dbus support.
   if (!resumed && resumed_by_clocktime) {
      uint64_t clock_elapsed_ms = millisec_since_resume_detected_by_clocktime();
      if (clock_elapsed_ms < (uint64_t) within_ms) {
         resumed = true;
         millisec_since = clock_elapsed_ms;
      }
   }

   if (millisec_since_loc)
      *millisec_since_loc = millisec_since;
   DBGF(debug, "within_ms=%d, millisec_since=%"PRIu64", returning %s",
               within_ms, millisec_since, sbool(resumed));
   return resumed;
}
