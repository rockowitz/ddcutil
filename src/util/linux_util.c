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
#include <signal.h>        // for fatal signal handlers
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
#include "suspend_resume_util.h"
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
 *  Shell command lsof is used to collect the information.
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

   // Reported only when a prepare signal is unmatched, so that the usual
   // report is unchanged.  It is the one line that distinguishes a permission
   // failure occurring inside a suspend, where the ACLs are expected to be
   // transiently gone, from one occurring at any other time.  One call, so
   // that the verdict and the elapsed time describe the same instant.
   uint64_t prepare_elapsed_ns = UINT64_MAX;
   bool cycle_open = ldbus_in_open_sleep_cycle(&prepare_elapsed_ns);
   if (prepare_elapsed_ns != UINT64_MAX)
      G_PTR_ARRAY_ADD_STRING(collector,
         "Sleep cycle open: PrepareForSleep(true) received %s seconds ago, "
         "matching PrepareForSleep(false) not yet received%s",
         formatted_time_t(prepare_elapsed_ns),
         cycle_open ? "" : ", retired: no longer treated as a resume");
#endif

   // no_mutate: this is a report, and must not consume the detection or open
   // the grace window.  It formerly did, which was harmless only while
   // i2c_open_bus_basic() consulted the detector before every open; once
   // that pre-open pause gave way to retrying after EACCES, this call could
   // be the first on the thread and so decide, from a diagnostic, whether a
   // later caller pauses.
   bool recent =  recently_resumed_from_sleep_by_clocktime0(/*no_modify=*/true, NULL);
   G_PTR_ARRAY_ADD_STRING(collector, "recently_returned_from_sleep_by_clocktime() returned %s",
                                     sbool(recent));
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

#ifdef REDUNDANT_AND_EXPENSIVE
   g_ptr_array_add(collector, strdup("Using command getfacl: "));
   char cmd[PATH_MAX+20];
   g_snprintf(cmd, PATH_MAX+20, "getfacl %s  --all-effective" , fqfn);
   char * errmsg = NULL;
   execute_shell_cmd_collect1(cmd, collector, &errmsg);
   if (errmsg) {
      g_ptr_array_add(collector, errmsg);
      fprintf(stderr, "%s   (A)\n", errmsg);
   }
#endif

   g_ptr_array_add(collector, strdup( "Using acl api:"));
   rpt_facl_collect0(fqfn, collector, depth);
#ifdef REDUNDANT
   g_ptr_array_add(collector, strdup( "Using low level acl api:"));
   rpt_facl_collect1(fqfn, collector, depth);
#endif
   rpt_lsof_collect0(fqfn, collector);  // uses command lsof

   char * sacl = get_user_acl(fqfn, uid);
   G_PTR_ARRAY_ADD_STRING(collector, "acl for user %d: %s", uid, sacl);
   free(sacl);

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
// Fatal signal handlers
//

// Signals whose default action terminates the process abruptly, running
// neither the library destructor that logs "libddcutil terminating." nor any
// atexit function, so that the system log shows the process simply ceasing.
// SIGSEGV, SIGBUS, SIGILL and SIGFPE are faults; SIGABRT is what assert() and
// glib's fatal paths raise.  SIGBUS is the one that catches a shared library
// replaced while a process still has it mapped, a common accident when
// installing a rebuilt libddcutil under a running client.
//
// SIGTERM and SIGINT are deliberately absent.  They are the client program's
// to handle, and a library claiming them changes how that program responds to
// an ordinary request to stop.  The signals here are ones no correct program
// continues from, and the previous handler is chained to in every case, so
// installing them does not alter what the client does with them.
static const int    fatal_signals[]      = {SIGSEGV,   SIGBUS,   SIGILL,   SIGFPE,   SIGABRT};
static const char * fatal_signal_names[] = {"SIGSEGV", "SIGBUS", "SIGILL", "SIGFPE", "SIGABRT"};
_Static_assert(ARRAY_SIZE(fatal_signals) == ARRAY_SIZE(fatal_signal_names),
               "fatal_signals and fatal_signal_names must correspond");

/** Disposition of each signal in #fatal_signals before this module installed
 *  its own, saved per signal so that the handler can chain to the right one.
 */
static struct sigaction old_fatal_actions[ARRAY_SIZE(fatal_signals)];


/** Returns the index of a signal within #fatal_signals.
 *
 *  @param  sig  signal number
 *  @return index, -1 if this module did not install a handler for it
 */
static int fatal_signal_ndx(int sig) {
   for (unsigned int ndx = 0; ndx < ARRAY_SIZE(fatal_signals); ndx++) {
      if (fatal_signals[ndx] == sig)
         return ndx;
   }
   return -1;
}


/** Handler for the signals in #fatal_signals.  Logs the signal and dumps the
 *  traced function stack to syslog, then chains to the handler that was
 *  previously installed.
 *
 *  @param sig      signal number
 *  @param info     details about the signal
 *  @param ucontext context at the time of the signal
 *
 *  @remark
 *  syslog() and the message formatting are not async-signal-safe.  That is the
 *  usual trade for a crash handler that has to say something useful, and it is
 *  what the SIGSEGV handler this generalizes always did, but a fault taken
 *  inside malloc() can deadlock here instead of logging.  Only the signal name
 *  lookup was kept off that path, being a table rather than strsignal().
 */
static void fatal_signal_handler(int sig, siginfo_t *info, void *ucontext) {
#ifdef BACKTRACE
   // Show backtrace
   void *frames[32];
   int n = backtrace(frames, 32);
   backtrace_symbols_fd(frames, n, STDERR_FILENO);
#endif

   int ndx = fatal_signal_ndx(sig);
   // si_code and si_addr locate the fault.  For SIGBUS in particular they are
   // what distinguishes its causes: si_code BUS_ADRERR at an address within a
   // mapped file is the signature of that file having been truncated or
   // replaced underneath the process.
   SIMPLE_STD_SYSLOG(LOG_ERR, "Fatal signal %d (%s), si_code=%d, si_addr=%p",
         sig,
         (ndx >= 0) ? fatal_signal_names[ndx] : "unrecognized",
         (info) ? info->si_code : 0,
         (info) ? info->si_addr : NULL);
   current_traced_function_stack_to_syslog(LOG_ERR, TFS_MOST_RECENT_LAST);

   if (ndx < 0)   // no handler installed here, so nothing saved to chain to
      return;

   struct sigaction old = old_fatal_actions[ndx];
   // Restore the previous disposition before chaining, so that a fault
   // repeated by the handler chained to is not caught here a second time.
   sigaction(sig, &old, NULL);

   if (old.sa_flags & SA_SIGINFO) {
       old.sa_sigaction(sig, info, ucontext);
   } else if (old.sa_handler == SIG_DFL) {
       // The signal is blocked while this handler runs, so it becomes pending
       // and terminates the process when the handler returns.
       raise(sig);
   } else if (old.sa_handler != SIG_IGN) {
       old.sa_handler(sig);
   }
}


/** Installs a handler for each signal in #fatal_signals, logging the signal
 *  and the traced function stack to syslog before chaining to whatever
 *  handler was in place.
 *
 *  Called during initialization of both the command line program and the
 *  shared library.
 */
void install_fatal_signal_handlers(void) {
   struct sigaction sa;
   memset(&sa, 0, sizeof sa);
   sa.sa_sigaction = fatal_signal_handler;
   sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);

   for (unsigned int ndx = 0; ndx < ARRAY_SIZE(fatal_signals); ndx++) {
      sigaction(fatal_signals[ndx], NULL, &old_fatal_actions[ndx]);
      sigaction(fatal_signals[ndx], &sa, NULL);
   }
}

