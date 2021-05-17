/** @file query_sysenv_logs.c
 *
 *  Query configuration files, logs, and output of logging commands.
 */

// Copyright (C) 2017-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "util/data_structures.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"

#include "base/core.h"
#include "base/status_code_mgt.h"
/** endcond */

#include "query_sysenv_base.h"

#include "query_sysenv_logs.h"


static bool probe_log(
      char *  log_fn,
      char ** filter_terms,
      bool    ignore_case,
      int     limit,
      int     depth)
{
   bool debug = false;
   assert(log_fn);
   DBGMSF(debug, "Starting. log_fn=%s, filter_terms=%p, ignore_case=%s, limit=%d",
                 log_fn, filter_terms, sbool(ignore_case), limit);
   bool file_found = false;
   int rc = 0;
   if ( !regular_file_exists(log_fn) ) {
      rpt_vstring(depth, "File not found: %s", log_fn);
      goto bye;
   }
   if ( access(log_fn, R_OK) < 0 ) {
      rpt_vstring(depth, "File not readable: %s", log_fn);
      goto bye;
   }

   rpt_vstring(depth, "Scanning file: %s", log_fn);

   // char  shell_cmd[PATH_MAX * 2 + 50];
   bool bigfile = false;
   struct stat st;
   // Coverity complains if return code not checked.
   rc = stat(log_fn, &st);
   if (rc != 0) {
      DBGMSG("Error executing stat(), errno = %d", errno);
      DBGMSG("Assuming file %s is huge", log_fn);
      bigfile = true;
   }
   else if (st.st_size > 1000000) {
      if (debug) {
         uint64_t sz = st.st_size;
         DBGMSG( "File %s is huge.  Size =  %" PRIu64 ". ", log_fn, sz);
      }
      bigfile = true;
   }

   if (limit < 0) {
      rpt_vstring(depth, "Limiting output to last %d relevant lines...", -limit);
   }
   else if (limit > 0) {
       rpt_vstring(depth, "Limiting output to first %d relevant lines...", limit);
   }
   GPtrArray * found_lines = NULL;

   if (bigfile && limit <= 0) {
      int maxlines = 50000;
      f0printf(stderr, "File %s is huge.  Examining only last %d lines\n", log_fn, maxlines);

      rc = file_get_last_lines(log_fn, maxlines, &found_lines, /*verbose*/ true);
      if (rc < 0) {
         DBGMSG("Error calling file_get_last_lines(), rc = %d", rc);
         goto bye;
      }

      // for (int ndx = 0; ndx < found_lines->len; ndx++) {
      //    DBGMSG("Found line: %s", g_ptr_array_index(found_lines,ndx));
      // }

      DBGMSF(debug, "file_get_last_lines() returned %d", rc);
      DBGMSF(debug, "before filter, found_lines->len = %d", found_lines->len);
      filter_and_limit_g_ptr_array(
            found_lines, filter_terms, ignore_case, limit);
      DBGMSF(debug, "after filter, found_lines->len = %d", found_lines->len);

   }
   else {
      found_lines = g_ptr_array_new_full(1000, g_free);
      rc = read_file_with_filter(found_lines, log_fn, filter_terms, ignore_case, limit);
   }

   if (rc < 0) {
      f0printf(stderr, "Error reading file: %s\n", psc_desc(rc));
   }
   else if (rc == 0) {   // rc >0 is the original number of lines
      rpt_title("Empty file", depth);
      file_found = true;
   }
   else if (found_lines->len == 0) {
      rpt_title("No lines found after filtering", depth);
      file_found = true;
   }
   else {
      for (int ndx = 0; ndx < found_lines->len; ndx++) {
         rpt_title(g_ptr_array_index(found_lines, ndx), depth+1);
      }
      file_found = true;
   }
   g_ptr_array_free(found_lines, true);

bye:
   DBGMSF(debug, "rc=%d, file_found=%s", rc, sbool(file_found));
   rpt_nl();
   return file_found;
}



static bool probe_cmd(
      char *  cmd,
      char ** filter_terms,
      bool    ignore_case,
      int     limit,
      int     depth)
{
   bool debug = false;
   assert(cmd);
   DBGMSF(debug, "Starting. cmd=%s, filter_terms=%p, ignore_case=%s, limit=%d",
                 cmd, filter_terms, sbool(ignore_case), limit);

   rpt_vstring(depth, "Executing command: %s", cmd);
   if (limit < 0) {
      rpt_vstring(depth, "Limiting output to last %d relevant lines...", -limit);
   }
   else if (limit > 0) {
       rpt_vstring(depth, "Limiting output to first %d relevant lines...", limit);
   }
   GPtrArray * filtered_lines = NULL;
   int rc = execute_cmd_collect_with_filter(cmd, filter_terms, ignore_case, limit, &filtered_lines);
   if (rc < 0) {
      f0printf(stderr, "Error executing command: %s\n", psc_desc(rc));
   }
   else if (rc == 0) {   // rc >0 is the original number of lines
      rpt_title("No output", depth);
   }
   else if (filtered_lines->len == 0) {
      rpt_title("No lines found after filtering", depth);
   }
   else {
      for (int ndx = 0; ndx < filtered_lines->len; ndx++) {
         rpt_title(g_ptr_array_index(filtered_lines, ndx), depth+1);
      }
   }
   if (filtered_lines) {
      g_ptr_array_set_free_func(filtered_lines, g_free);
      g_ptr_array_free(filtered_lines, true);
   }

   bool result = (rc >= 0);
   DBGMSF(debug, "rc=%d, returning %s", rc, sbool(result));
   rpt_nl();
   return result;
}


/** Scans log files for lines of interest.
 *
 *  Depending on operating environment, some subset of
 *  the following files and command output:
 *    - dmesg
 *    - journalctl
 *    - /var/log/daemon.log
 *    - /var/log/kern.log
 *    - /var/log/messages
 *    - /var/log/syslog
 *    - /var/log/Xorg.0.log
 *
 *  \param accum collected environment information
 */
void probe_logs(Env_Accumulator * accum) {
   // TODO: Function needs major cleanup

   int depth = 0;
   int d1 = depth+1;
   int d2 = depth+2;
   // DBGMSG("Starting");
   // debug_output_dest();

   rpt_nl();
   rpt_title("Examining system logs...", depth);
   sysenv_rpt_current_time("Current timestamps:", depth);

   // TODO: Pick simpler data structures.  Is Value_Name_Title_Table worth it?


   // 1 suffixes to not conflict with token in syslog.h
   const Byte LOG_XORG       = 0x80;
   const Byte LOG_DAEMON1    = 0x40;
   const Byte LOG_SYSLOG1     = 0x20;
   const Byte LOG_KERN1       = 0x10;
   const Byte LOG_JOURNALCTL = 0x08;
   const Byte LOG_MESSAGES   = 0x04;
   const Byte LOG_DMESG      = 0x02;

   Value_Name_Title_Table log_table = {
         VNT(LOG_DMESG,      "dmesg"              ),
         VNT(LOG_JOURNALCTL, "journalctl"         ),
         VNT(LOG_DAEMON1,     "/var/log/daemon.log" ),
         VNT(LOG_KERN1,       "/var/log/kern.log"  ),
         VNT(LOG_MESSAGES,   "/var/log/messages"  ),
         VNT(LOG_SYSLOG1,     "/var/log/syslog"    ),
         VNT(LOG_XORG,       "/var/log/Xorg.0.log"),
         VNT_END
   };

   bool log_xorg_found       = false;
   bool log_daemon_found     = false;        // Raspbian
   bool log_syslog_found     = false;        // Ubuntu, Raspbian
   bool log_kern_found       = false;        // Raspbian
 //bool log_journalctl_found = false;        // Debian, Raspbian
   bool log_messages_found   = false;        // Raspbian
   bool log_dmesg_found      = false;

   Byte logs_checked = 0x00;
   Byte logs_found   = 0x00;

#ifdef NO
   // Problem: dmesg can be filled w i2c errors from i2cdetect trying to
   // read an SMBus device
   // Disable prefix_matches until filter out SMBUS devices
   p = prefix_matches;
#endif
   char * addl_matches[] = {
         "drm",
         "video",
         "eeprom",
         "i2c_",    // was i2c_
         NULL
   };

   Null_Terminated_String_Array drivers_plus_addl_matches =
            ntsa_join(get_known_video_driver_module_names(), addl_matches, /*dup*/ false);

   // *** dmesg ***

   rpt_nl();
   // first few lines of dmesg are lost.  turning on any sort of debugging causes
   // them to reappear.  apparently a NL in the stream does the trick.  why?
   // it's a heisenbug.  Just use the more verbose journalctl output
   logs_checked |= LOG_DMESG;

   rpt_title("Scanning dmesg output for I2C related entries...", depth+1);
   log_dmesg_found = probe_cmd(
         "dmesg",
         drivers_plus_addl_matches,
         true,    // ignore_case
         0,       // no limit
         depth+1);
   if (log_dmesg_found)
      logs_found |= LOG_DMESG;

   // *** journalctl ***

   logs_checked |= LOG_JOURNALCTL;

#ifdef ALT
   // if don't use this version, don't need to link with libsystemd
   DBGMSG("Using get_current_boot_messages...");
   rpt_title("Checking journalctl for I2C related entries...", depth+1);
   GPtrArray * journalctl_msgs = get_current_boot_messages(drivers_plus_addl_matches, /* ignore case */true, 0);
   if (journalctl_msgs) {
      // log_journalctl_found = true;
      logs_found |= LOG_JOURNALCTL;

      for (int ndx = 0; ndx < journalctl_msgs->len; ndx++) {
         rpt_vstring(depth+2, "%s", g_ptr_array_index(journalctl_msgs, ndx));
      }
   }
   rpt_nl();
#endif

   // has a few more lines from nvidia-persistence, lines have timestamp, hostname, and subsystem
   rpt_title("Scanning journalctl output for I2C related entries...", depth+1);
   log_dmesg_found = probe_cmd("journalctl --no-pager --boot", drivers_plus_addl_matches, /*ignore_case*/ true, 0, depth+1);
   if (log_dmesg_found)
      logs_found |= LOG_DMESG;
    rpt_nl();

   // *** Xorg.0.log ***

   char * xorg_terms[] = {
    //   "[Ll]oadModule:",     // matches LoadModule, UnloadModule
         "LoadModule:",     // matches LoadModule, UnloadModule
    //     "[Ll]oading",         // matches Loading Unloading
         "Loading",
         "driver for",
         "Matched .* as autoconfigured",
         "Loaded and initialized",
         "drm",
         "soc",
         "fbdev",       // matches fbdevhw
         "vc4",
         "i2c",
         NULL
   };

   // Null_Terminated_String_Array log_terms = all_terms;
   char * rasp_log_terms[] = {
         "i2c",
         NULL
   };

   Null_Terminated_String_Array log_terms = ntsa_join(drivers_plus_addl_matches, rasp_log_terms, false);
   Null_Terminated_String_Array all_terms = log_terms;

   if (accum->is_arm) {
      logs_checked |= LOG_XORG;
      log_xorg_found =  probe_log("/var/log/Xorg.0.log", xorg_terms, /*ignore_case*/ true,  0, depth+1);
      if (log_xorg_found)
         logs_found |= LOG_XORG;
   }
   else {
      logs_checked |= LOG_XORG;
      // rpt_vstring(depth+1, "Limiting output to 200 lines...");
      log_xorg_found =  probe_log("/var/log/Xorg.0.log", drivers_plus_addl_matches, /*ignore_case*/ true, 200, depth+1);
      if (log_xorg_found)
         logs_found |= LOG_XORG;
   }

   // ***/var/log/kern.log, /var/log/daemon.log, /var/log/syslog, /var/log/messages ***

   // Using our own code instead of shell to scan files
   log_messages_found = probe_log("/var/log/messages",   log_terms, /*ignore_case*/ true, -40, d1);
   log_kern_found     = probe_log("/var/log/kern.log",   log_terms, /*ignore_case*/ true, -20, d1);
   log_daemon_found   = probe_log("/var/log/daemon.log", log_terms, /*ignore_case*/ true, -10, d1);
   log_syslog_found   = probe_log("/var/log/syslog",     log_terms, /*ignore_case*/ true, -50, d1);

   logs_checked |= (LOG_MESSAGES | LOG_KERN | LOG_DAEMON | LOG_SYSLOG);
   if (log_messages_found)
      logs_found |= LOG_MESSAGES;
   if (log_kern_found)
      logs_found |= LOG_KERN;
   if (log_daemon_found)
      logs_found |= LOG_DAEMON;
   if (log_syslog_found)
      logs_found |= LOG_SYSLOG;

   // for now, just report the logs seen to avoid warning about unused vars
#ifdef NO
   rpt_title("Log files found:  ", depth);
   rpt_bool("dmesg"               , NULL, log_dmesg_found,      d1);
   rpt_bool("/var/log/messages"   , NULL, log_messages_found,   d1);
   rpt_bool("journalctl"          , NULL, log_journalctl_found, d1);
   rpt_bool("/var/log/kern"       , NULL, log_kern_found,       d1);
   rpt_bool("/var/log/syslog"     , NULL, log_syslog_found,     d1);
   rpt_bool("/var/log/daemaon"    , NULL, log_daemon_found,     d1);
   rpt_bool("/var/log/Xorg.0.log" , NULL, log_xorg_found,       d1);
#endif
   rpt_nl();
   rpt_title("Log Summary", d1);
   rpt_vstring(d2,  "%-30s  %-7s   %-6s",  "Log", "Checked", "Found");
   rpt_vstring(d2,  "%-30s  %-7s   %-6s",  "===", "=======", "=====");
   for (Value_Name_Title * entry = log_table; entry->title; entry++) {
      rpt_vstring(d2, "%-30s  %-7s   %-6s",
                      entry->title,
                      sbool(logs_checked & entry->value),
                      sbool(logs_found & entry->value));
   }
   rpt_nl();
   if (log_terms != all_terms)
      ntsa_free(log_terms, false);
   ntsa_free(all_terms, false);
   ntsa_free(drivers_plus_addl_matches, false);
}


/** Examines kernel configuration files and DKMS.
 *
 *  \param  accum  accumulated environment
 */
void probe_config_files(Env_Accumulator * accum) {
   int depth = 0;
   // DBGMSG("Starting");
   // debug_output_dest();

   rpt_nl();
   rpt_title("Examining configuration files...", depth);

   if (accum->is_arm) {
      rpt_title("Examining /boot/config.txt:", depth+1);
      execute_shell_cmd_rpt("egrep -i -edtparam -edtoverlay -edevice_tree /boot/config.txt | grep -v \"^ *#\"", depth+2);
      rpt_nl();
      rpt_vstring(depth+1, "Looking for blacklisted drivers in /etc/modprobe.d:");
      execute_shell_cmd_rpt("grep -ir blacklist /etc/modprobe.d | grep -v \"^ *#\"", depth+2);
   }
   else {
      rpt_nl();
      rpt_vstring(0,"DKMS modules:");
      execute_shell_cmd_rpt("dkms status", 1 /* depth */);
      rpt_nl();
      rpt_vstring(0,"Kernel I2C configuration settings:");
   // execute_shell_cmd_rpt("grep I2C /boot/config-$(uname -r)", 1 /* depth */);
      execute_shell_cmd_rpt("grep I2C_CHARDEV /boot/config-$(uname -r)", 1 /* depth */);
      rpt_nl();
      rpt_vstring(0,"Kernel AMDGPU configuration settings:");
      execute_shell_cmd_rpt("grep AMDGPU /boot/config-$(uname -r)", 1 /* depth */);
      rpt_nl();
      // TMI:
      // rpt_vstring(0,"Full xrandr --props:");
      // execute_shell_cmd_rpt("xrandr --props", 1 /* depth */);
      // rpt_nl();
   }
}

