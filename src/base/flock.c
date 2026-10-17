/** @file flock.c */

// Copyright (C) 2024-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <syslog.h>
#include <unistd.h>

#include "public/ddcutil_types.h"

#include "util/backtrace.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/file_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/timestamp.h"
#include "util/traced_function_stack.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"

#ifdef EXPERIMENTAL_FLOCK_RECOVERY
#include "i2c/i2c_edid.h"     // violates layering
#include "i2c/i2c_execute.h"
#endif

#include "base/flock.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_BASE;

bool cross_instance_locks_enabled = DEFAULT_ENABLE_FLOCK;
int  flock_poll_millisec = DEFAULT_FLOCK_POLL_MILLISEC;
int  flock_max_wait_millisec = DEFAULT_FLOCK_MAX_WAIT_MILLISEC;
bool debug_flock = false;


void i2c_enable_cross_instance_locks(bool yesno) {
   bool debug = false;
   cross_instance_locks_enabled = yesno;
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "yesno = %s", SBOOL(yesno));
}


//
// Debugging Functions
//

/** Collects the processes holding a lock on a file, as reported by /proc/locks.
 *
 *  @param  filename  file name
 *  @return GPtrArray of process ids, as strings; empty if none or if the file
 *          cannot be stat'd.  The caller is responsible for freeing it.
 *
 *  @remark
 *  A /proc/locks line identifies the locked file by the hex major:minor device
 *  number of the filesystem containing it and the decimal inode number, e.g.
 *  "146: FLOCK  ADVISORY  WRITE 704836 00:07:1388 0 EOF".  The device number
 *  of /dev differs from system to system, so it is taken from stat().  Lines
 *  for processes blocked waiting for a lock ("146: -> FLOCK ...") are skipped,
 *  since those processes do not hold it.
 *  @remark
 *  This is done in C rather than with a shell pipeline because a child process
 *  inherits any /dev/i2c file descriptor this process has open, as those are not
 *  opened O_CLOEXEC, and so keeps any lock on it in place while the child runs.
 */
static GPtrArray * collect_pids_locking_file(const char * filename) {
   GPtrArray * pids = g_ptr_array_new_with_free_func(g_free);
   struct stat st;
   if (stat(filename, &st) == 0) {
      GPtrArray * lines = g_ptr_array_new_with_free_func(free);
      file_getlines("/proc/locks", lines, false);
      for (guint ndx = 0; ndx < lines->len; ndx++) {
         // fields: ordinal, lock class, mode, access, pid, major:minor:inode, ...
         char ** tokens = g_strsplit_set(g_ptr_array_index(lines, ndx), " ", -1);
         char *  fields[6];
         int     field_ct = 0;
         for (int k = 0; tokens[k] && field_ct < 6; k++) {
            if (*tokens[k])
               fields[field_ct++] = tokens[k];
         }
         if (field_ct == 6 && !streq(fields[1], "->")) {
            unsigned int       maj;
            unsigned int       min;
            unsigned long long ino;
            if (sscanf(fields[5], "%x:%x:%llu", &maj, &min, &ino) == 3 &&
                maj == major(st.st_dev) && min == minor(st.st_dev)     &&
                ino == (unsigned long long) st.st_ino)
            {
               g_ptr_array_add(pids, g_strdup(fields[4]));
            }
         }
         g_strfreev(tokens);
      }
      g_ptr_array_free(lines, true);
   }
   return pids;
}


/** Reports the Name, State, and Pid lines of /proc/<pid>/status,
 *  or collects them.
 *
 *  @param  spid          process id, as a string
 *  @param  depth         logical indentation depth
 *  @param  report_lines  if non-NULL, add the indented lines to this array
 *                        instead of writing them to the current output destination
 */
static void rpt_process_status_collect(const char * spid, int depth, GPtrArray* report_lines) {
   char fn[40];
   g_snprintf(fn, sizeof(fn), "/proc/%s/status", spid);
   GPtrArray * lines = g_ptr_array_new_with_free_func(free);
   int rc = file_getlines(fn, lines, false);
   if (rc < 0)
      rpt_vstring_collect(depth, report_lines, "Unable to read %s: %s", fn, strerror(-rc));
   else {
      for (guint ndx = 0; ndx < lines->len; ndx++) {
         char * line = g_ptr_array_index(lines, ndx);
         if (str_starts_with(line, "Name:") || str_starts_with(line, "State:") ||
             str_starts_with(line, "Pid:"))
            rpt_vstring_collect(depth, report_lines, "%s", line);
      }
   }
   g_ptr_array_free(lines, true);
}


/** Reports the Name, State, and Pid lines of /proc/<pid>/status.
 *
 *  @param  spid   process id, as a string
 *  @param  depth  logical indentation depth
 */
void rpt_process_status(const char * spid, int depth) {
   rpt_process_status_collect(spid, depth, NULL);
}


/** Reports the processes holding a lock on a file, or collects the report.
 *
 *  @param  filename   file name
 *  @param  collector  if non-NULL, add the report lines to this array
 *                     instead of writing them to the current output destination
 */
void show_flock_collect(const char * filename, GPtrArray * collector) {
   ino_t inode = get_inode_by_fn(filename);
   rpt_vstring_collect(1, collector, "Processes locking %s (inode %ju): ", filename, (uintmax_t) inode);
   GPtrArray * pids = collect_pids_locking_file(filename);
   for (guint ndx = 0; ndx < pids->len; ndx++) {
      char * spid = g_ptr_array_index(pids, ndx);
      rpt_vstring_collect(2, collector, "%s", spid);
      rpt_process_status_collect(spid, 2, collector);
      rpt_label_collect(0, collector, "");
   }
   g_ptr_array_free(pids,true);
}


void show_flock(const char * filename, bool dest_syslog) {
   GPtrArray * lines = g_ptr_array_new_with_free_func(g_free);
   show_flock_collect(filename, lines);
   for (guint ndx = 0; ndx < lines->len; ndx++) {
      char * line = g_ptr_array_index(lines, ndx);   // already indented
      if (dest_syslog)
         DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "%s", line);
      else
         rpt_label(0, line);
   }
   g_ptr_array_free(lines, true);
}




#ifdef UNUSED
void show_lsof(const char * filename) {
   MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Programs holding %s open:", filename);
   rpt_lsof(filename, 1);
   char cmd[80];
   g_snprintf(cmd, 80, "lsof %s", filename);
   GPtrArray* lsof_lines = execute_shell_cmd_collect(cmd);
   for (int ndx = 0; ndx < lsof_lines->len; ndx++) {
      MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "   %s", (char*) g_ptr_array_index(lsof_lines, ndx));
   }
   g_ptr_array_free(lsof_lines, true);
}
#endif


void explore_flock(int fd, const char * filename) {
   ino_t inode = get_inode_by_fd(fd);
   intmax_t pid = get_process_id();
   DBGMSG("pid=%jd filename = %s, inode=%ju", pid, filename, (uintmax_t) inode);
   execute_shell_cmd_rpt("lslocks|grep /dev/i2c", 1);
   char cmd[80];
   // g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d", inode);
   // execute_shell_cmd_rpt(cmd, 1);
   GPtrArray * pids_locking_inode = collect_pids_locking_file(filename);
   rpt_vstring(1, "Processing locking inode %ju:", (uintmax_t) inode);
   for (guint ndx = 0; ndx < pids_locking_inode->len; ndx++) {
      // cast to avoid coverity warning:
      rpt_vstring(2, "%s", (char*)  g_ptr_array_index(pids_locking_inode, ndx));
   }
   g_ptr_array_free(pids_locking_inode, true);

   // g_snprintf(cmd, 80, "ls /proc/%jd", pid);
   // execute_shell_cmd_rpt(cmd, 1);
   // g_snprintf(cmd, 80, "cat /proc/%jd/cmdline", pid);
   // execute_shell_cmd_rpt(cmd, 1);
   g_snprintf(cmd, 80, "cat /proc/%jd/status | grep -E -e Name -e State -e '^Pid:'", pid);
   execute_shell_cmd_rpt(cmd, 1);
   GPtrArray * pids = execute_shell_cmd_collect(cmd);
   for (guint ndx = 0; ndx < pids->len; ndx++) {
      //* case avoids coverity warning
      rpt_vstring(3, "%s", (char *) g_ptr_array_index(pids, ndx));
   }
}

#ifdef EXPERIMENTAL_FLOCK_RECOVERY
static bool simple_ioctl_read_edid_by_fd(
      int  fd,
      int  read_size,
      bool write_before_read,
      int  depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, filename=%s, read_size=%d, write_before_read=%s",
                 fd, filename_for_fd_t(fd), read_size, sbool(write_before_read));
   assert(read_size == 128 || read_size == 256);
   rpt_nl();
   rpt_vstring(depth, "Attempting simple %d byte EDID read, fd=%d, %s initial write",
                  read_size, fd,
                  (write_before_read) ? "WITH" : "WITHOUT"
                  );

   Byte * edid_buf = calloc(1, 256);

   int rc = 0;
   bool ok = false;

   if (write_before_read) {
      edid_buf[0] = 0x00;
      rc = i2c_ioctl_writer(fd, 0x50, 1, edid_buf);
      if (rc < 0) {
         rpt_vstring(depth, "write of 1 byte failed, errno = %s", linux_errno_desc(errno));
         rpt_label(depth, "Continuing");
      }
   }

   DBGMSF(debug, "Calling i2c_ioctl_reader(), read_bytewise=false, read_size=%d, edid_buf=%p",
                 read_size, edid_buf);
   rc = i2c_ioctl_reader(fd, 0x50, false, read_size, edid_buf);
   if (rc < 0) {
      rpt_vstring(depth,"read failed. errno = %s", linux_errno_desc(errno));
   }
   else {
      rpt_hex_dump(edid_buf, read_size, depth+1);
      ok = true;
   }

   free(edid_buf);
   DBGMSF(debug, "Returning: %s", sbool(ok));
   return ok;
}
#endif


Status_Errno flock_lock_by_fd(int fd, const char * filename, bool wait) {
   assert(filename);
   bool debug = false;
   debug = debug || debug_flock;
#ifdef EXPERIMENTAL_FLOCK_RECOVERY
   debug = debug || streq(filename,"/devi2c-25");
#endif
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "fd=%d, filename=%s, wait=%s", fd, filename, SBOOL(wait));

   // int inode = get_inode_by_fn(filename);
   // int inode2 = get_inode_by_fd(fd);
   // assert(inode == inode2);

   int operation = LOCK_EX|LOCK_NB;
   int total_wait_millisec = 0;
   uint64_t max_wait_millisec = (wait) ? flock_max_wait_millisec : 0;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "flock_poll_millisec=%jd, flock_max_wait_millisec=%d ",
         flock_poll_millisec, flock_max_wait_millisec);
   Status_Errno flockrc = 0;
   int flock_call_ctr = 0;

   while(true) {
      DBGTRC_NOPREFIX(debug||debug_flock, DDCA_TRC_NONE,
            "Calling flock(%d,0x%04x), filename=%s flock_call_ctr=%d, total_wait_millisec %d...",
            fd, operation, filename, flock_call_ctr, total_wait_millisec);

      flock_call_ctr++;
      flockrc = flock(fd, operation);
      if (flockrc == 0)  {
         DBGTRC_NOPREFIX(debug || debug_flock /* || (flock_call_ctr > 1  && debug_flock) */, DDCA_TRC_NONE,
               "flock succeeded, filename=%s, flock_call_ctr=%d", filename, flock_call_ctr);
#ifdef EXPLORING
         explore_flock(fd, filename);
#endif
         break;
      } // flockrc == 0

      // handle failure
      // n.b. lock will fail w EAGAIN if at least NEC display turned off
      assert(flockrc == -1);
      int errsv = errno;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "filename=%s, flock_call_ctr=%d, flock() returned: %s",
            filename, flock_call_ctr, psc_desc(-errsv));

      if (flock_call_ctr == 1 && IS_DBGTRC(true, DDCA_TRC_NONE)) {
         DBGMSG("Flock diagnostics:");
         show_flock(filename, false);
         // show_backtrace(0);
         dbgrpt_current_traced_function_stack(/*reverse*/ false, false, 0);
         TRACED_FUNCTION_STACK_TO_SYSLOG(DDCA_SYSLOG_ERROR, TFS_MOST_RECENT_FIRST);
      }

      // give up if sleeping again would exceed the max wait time
      if (total_wait_millisec + flock_poll_millisec > max_wait_millisec) {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Max wait time %"PRIu64" milliseconds reached after %d flock() calls",
               max_wait_millisec, flock_call_ctr);
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "Max wait time %"PRIu64" milliseconds reached after %d flock() calls",
               max_wait_millisec, flock_call_ctr);
         flockrc = DDCRC_FLOCKED;
         break;
      }

      if (errsv != EWOULDBLOCK ) {          // n. EWOULDBLOCK == EAGAIN
         DBGTRC_NOPREFIX(true, TRACE_GROUP, "Unexpected error from flock() for %s: %s",
                   filename, psc_desc(-errsv));
         flockrc = -errsv;
         break;
      }

#ifdef EXPERIMENTAL_FLOCK_RECOVERY
      if (errsv == EWOULDBLOCK) {
         Buffer * edidbuf = buffer_new(256, "");
         Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(fd, edidbuf, NULL);
         bool found_edid = (rc == 0);
         buffer_free(edidbuf, "");
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "able to read edid directly: %s", sbool(found_edid));
         // TODO: read attributes
         // RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", dh->dref->

         simple_ioctl_read_edid_by_fd(fd,
                                      256,   // read size
                                      true,  //write before read
                                      2);    // depth
      }
#endif
      DBGTRC_NOPREFIX(debug || debug_flock, DDCA_TRC_NONE,
                 "Resource locked. filename=%s, flock_call_ctr=%d, Sleeping", filename, flock_call_ctr);

      //         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s locked.  Retrying...", filename);
      usleep(flock_poll_millisec*1000);
      total_wait_millisec += flock_poll_millisec;
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "end of polling loop. flockrc = %d", flockrc);

   // DBGMSG("Testing Flock diagnostics:");
   // show_flock(filename, true);


   if (flockrc == 0) {
      if (flock_call_ctr == 1) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "flock() for %s succeeded after %d calls", filename, flock_call_ctr);
         // floods syslog:
         // DECORATED_SYSLOG(DDCA_SYSLOG_DEBUG, "flock() for %s succeeded after %d calls", filename, flock_call_ctr);
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "flock() for %s succeeded after %d calls", filename, flock_call_ctr);
         DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "flock() for %s succeeded after %d calls", filename, flock_call_ctr);
      }
   }
   else {
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "flock() for %s failed on %d calls", filename, flock_call_ctr);
      if (IS_DBGTRC(true, DDCA_TRC_NONE)) {
         DBGMSG("Flock diagnostics:");
         show_flock(filename, false);
         // show_backtrace(0);
         dbgrpt_current_traced_function_stack(/*reverse*/ false, false, 0);
         TRACED_FUNCTION_STACK_TO_SYSLOG(DDCA_SYSLOG_ERROR, TFS_MOST_RECENT_FIRST);
      }

      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "flock() for %s failed on %d calls", filename, flock_call_ctr);
      DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Flock diagnostics:");
      show_flock(filename, true);
      // backtrace_to_syslog(LOG_ERR, 0);
   }
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_BASE,flockrc, "filename=%s", filename);
   return flockrc;
}


Status_Errno flock_unlock_by_fd(int fd) {
   bool debug = false;
   debug = debug || debug_flock;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "fd=%d, filename=%s", fd, filename_for_fd_t(fd));
   int result = 0;
   assert(cross_instance_locks_enabled);

   DBGTRC_NOPREFIX(debug || debug_flock, TRACE_GROUP, "Calling flock(%d,LOCK_UN) filename=%s...",
            fd, filename_for_fd_t(fd));

   int rc = flock(fd, LOCK_UN);
   if (rc < 0) {
      result = errno;
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "Unexpected error from flock(..,LOCK_UN): %s",
            psc_desc(-result));
   }

   DBGTRC_RET_DDCRC(debug, DDCA_TRC_BASE, result, "filename=%s", filename_for_fd_t(fd));
   return result;
}


void init_flock() {
   RTTI_ADD_FUNC(flock_lock_by_fd);
   RTTI_ADD_FUNC(flock_unlock_by_fd);
}
