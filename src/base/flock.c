/** @file flock.c */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <sys/file.h>
#include <syslog.h>
#include <unistd.h>

#include "public/ddcutil_types.h"

#include "util/debug_util.h"
#include "util/file_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/timestamp.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"


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


void show_flock(const char * filename) {
   int inode = get_inode_by_fn(filename);
   MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Processes locking %s (inode %d): ", filename, inode);
   char cmd[80];
   g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d | cut -d' ' -f'1'", inode);
   execute_shell_cmd_rpt(cmd, 1);  // *** TEMP ***
   GPtrArray * pids = execute_shell_cmd_collect(cmd);
   // rpt_vstring(1, "Processes locking inode %jd", inode);
   for (int ndx = 0; ndx < pids->len; ndx++) {
      char * spid = g_ptr_array_index(pids, ndx);
      rpt_vstring(2, "%s", spid);  // *** TEMP ***
      g_snprintf(cmd, 80, "cat /proc/%s/status | grep -E -e Name -e State -e '^Pid:'", spid);
      execute_shell_cmd_rpt(cmd, 1); // *** TEMP ***
      GPtrArray * status_lines = execute_shell_cmd_collect(cmd);
      for (int k = 0; k < status_lines->len; k ++) {
         MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "   %s", (char*) g_ptr_array_index(status_lines, k));
      }
      rpt_nl();
      g_ptr_array_free(status_lines, true);
   }
   g_ptr_array_free(pids,true);
}


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


void explore_flock(int fd, const char * filename) {
   int inode = get_inode_by_fd(fd);
   intmax_t pid = get_process_id();
   DBGMSG("pid=%jd filename = %s, inode=%d", pid, filename, inode);
   execute_shell_cmd_rpt("lslocks|grep /dev/i2c", 1);
   char cmd[80];
   // g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d", inode);
   // execute_shell_cmd_rpt(cmd, 1);
   g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d | cut -d' ' -f'1'", inode);
   execute_shell_cmd_rpt(cmd, 1);
   GPtrArray * pids_locking_inode = execute_shell_cmd_collect(cmd);
   rpt_vstring(1, "Processing locking inode %jd:", inode);
   for (int ndx = 0; ndx < pids_locking_inode->len; ndx++) {
      // cast to avoid coverity warning:
      rpt_vstring(2, "%s", (char*)  g_ptr_array_index(pids_locking_inode, ndx));
   }

   // g_snprintf(cmd, 80, "ls /proc/%jd", pid);
   // execute_shell_cmd_rpt(cmd, 1);
   // g_snprintf(cmd, 80, "cat /proc/%jd/cmdline", pid);
   // execute_shell_cmd_rpt(cmd, 1);
   g_snprintf(cmd, 80, "cat /proc/%jd/status | grep -E -e Name -e State -e '^Pid:'", pid);
   execute_shell_cmd_rpt(cmd, 1);
   GPtrArray * pids = execute_shell_cmd_collect(cmd);
   for (int ndx = 0; ndx < pids->len; ndx++) {
      //* case avoids coverity warning
      rpt_vstring(3, "%s", (char *) g_ptr_array_index(pids, ndx));
   }
}


Status_Errno flock_lock_by_fd(int fd, const char * filename, bool wait) {
   assert(filename);
   bool debug = false;
   debug = debug ||  debug_flock;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "fd=%d, filename=%s, wait=%s", fd, filename, SBOOL(wait));

   // int inode = get_inode_by_fn(filename);
   // int inode2 = get_inode_by_fd(fd);
   // assert(inode == inode2);

   int operation = LOCK_EX|LOCK_NB;
   int total_wait_millisec = 0;
   uint64_t max_wait_millisec = (wait) ? flock_max_wait_millisec : 0;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "flock_poll_millisec=%jd, flock_max_wait_millisec=%jd ",
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
      assert(flockrc == -1);
      int errsv = errno;
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "filename=%s, flock_call_ctr=%d, flock() returned: %s",
            filename, flock_call_ctr, psc_desc(-errsv));

      if (total_wait_millisec > max_wait_millisec) {
         MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Max wait time exceeded after %d flock() calls", flock_call_ctr);
         flockrc = DDCRC_FLOCKED;
         break;
      }

      if (errsv != EWOULDBLOCK ) {          // n. EWOULDBLOCK == EAGAIN
         DBGTRC_NOPREFIX(true, TRACE_GROUP, "Unexpected error from flock() for %s: %s",
                   filename, psc_desc(-errsv));
         flockrc = -errsv;
         break;
      }

      DBGTRC_NOPREFIX(debug || debug_flock, DDCA_TRC_NONE,
                 "Resource locked. filename=%s, flock_call_ctr=%d, Sleeping", filename, flock_call_ctr);

      if (flock_call_ctr == 1)
              MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE, "%s locked.  Retrying...", filename);
      usleep(flock_poll_millisec*1000);
      total_wait_millisec += flock_poll_millisec;
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "end of polling loop. flockrc = %d", flockrc);

   if (flockrc != 0) {
      if (IS_DBGTRC(true, DDCA_TRC_NONE)) {
         DBGMSG("Flock diagnostics:");
         show_flock(filename);
         show_backtrace(2);
         backtrace_to_syslog(LOG_ERR, 0);
         //  ASSERT_WITH_BACKTRACE(false);
      }
   }

   if (flock_call_ctr > 1) {
      if (flockrc == 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "flock for %s succeeded after %d calls", filename, flock_call_ctr);
         SYSLOG2(DDCA_SYSLOG_NOTICE, "flock for %s succeeded after %d calls", filename, flock_call_ctr);
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "flock for %s failed on %d calls", filename, flock_call_ctr);
         SYSLOG2(DDCA_SYSLOG_ERROR, "flock for %s failed on %d calls", filename, flock_call_ctr);
      }
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
