/* query_sysenv_logs.c
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include "../app_sysenv/query_sysenv_logs.h"

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"

#include "base/core.h"
#include "base/status_code_mgt.h"

#include "query_sysenv_base.h"

//
// Log files
//

void filter_and_limit_g_ptr_array(
      GPtrArray * line_array,
      char **     filter_terms,
      bool        ignore_case,
      int         limit)
{
   bool debug = false;
   DBGMSF(debug, "line_array=%p, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
            line_array, ntsa_length(filter_terms), bool_repr(ignore_case), limit);
   if (debug) {
      // (const char **) cast to conform to strjoin() signature
      char * s = strjoin( (const char **) filter_terms, -1, ", ");
      DBGMSG("Filter terms: %s", s);
      free(s);
   };
#ifdef TOO_MUCH
   if (debug) {
      if (filter_terms) {
         printf("(%s) filter_terms:\n", __func__);
         ntsa_show(filter_terms);
      }
   }
#endif
   // inefficient, just make it work for now
   for (int ndx = (line_array->len)-1 ; ndx >= 0; ndx--) {
      char * s = g_ptr_array_index(line_array, ndx);
      // DBGMSF(debug, "s=|%s|", s);
      bool keep = true;
      if (filter_terms)
         keep = apply_filter_terms(s, filter_terms, ignore_case);
      if (!keep) {
         g_ptr_array_remove_index(line_array, ndx);
      }
   }
   gaux_ptr_array_truncate(line_array, limit);

   DBGMSF(debug, "Done. line_array->len=%d", line_array->len);
}




/** Reads the contents of a file into a #GPtrArray of lines, optionally keeping only
 *  those lines containing at least one on a list of terms.  After filtering, the set
 *  of returned lines may be further reduced to either the first or last n number of
 *  lines.
 *
 *  \param  line_array #GPtrArray in which to return the lines read
 *  \param  fn         file name
 *  \param  filter_terms  #Null_Terminated_String_Away of filter terms
 *  \param  ignore_case   ignore case when testing filter terms
 *  \param  limit if 0, return all lines that pass filter terms
 *                if > 0, return at most the first #limit lines that satisfy the filter terms
 *                if < 0, return at most the last  #limit lines that satisfy the filter terms
 *  \return if >= 0, number of lines before filtering and limit applied
 *          if < 0,  -errno
 *
 *  \remark
 *  This function was created because using grep in conjunction with pipes was
 *  producing obscure shell errors.
 *  \remark The #GPtrArray is passed into this function instead of allocating it
 *          to allow for returning a status code.
 *  \remark Consider adding the ability to treat filter terms as regular expressions
 */
static int read_file_with_filter(
      GPtrArray * line_array,
      char *      fn,
      char **     filter_terms,
      bool        ignore_case,
      int         limit)
{
   bool debug = false;
   DBGMSF(debug, "line_array=%p, fn=%s, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
            line_array, fn, ntsa_length(filter_terms), bool_repr(ignore_case), limit);

#ifdef TOO_MUCH
   if (debug) {
      if (filter_terms) {
         printf("(%s) filter_terms:\n", __func__);
         ntsa_show(filter_terms);
      }
   }
#endif

   g_ptr_array_set_free_func(line_array, g_free);    // in case not already set

   int rc = file_getlines(fn, line_array, /*verbose*/ true);
   DBGMSF(debug, "file_getlines() returned %d", rc);

   if (rc > 0) {
      filter_and_limit_g_ptr_array(
         line_array,
         filter_terms,
         ignore_case,
         limit);
   }
   else { // rc == 0
      DBGMSF(debug, "Empty file");
   }

   DBGMSF(debug, "Returning: %d", rc);
   return rc;
}


int execute_cmd_collect_with_filter(
      char *       cmd,
      char **      filter_terms,
      bool         ignore_case,
      int          limit,
      GPtrArray ** result_loc)
{
   bool debug = false;
   DBGMSF(debug, "cmd|%s|, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
            cmd, ntsa_length(filter_terms), bool_repr(ignore_case), limit);

   int rc = 0;
   GPtrArray *line_array = execute_shell_cmd_collect(cmd);
   if (!line_array) {
      rc = -1;
   }
   else {
      rc = line_array->len;
      if (rc > 0) {
         filter_and_limit_g_ptr_array(
            line_array,
            filter_terms,
            ignore_case,
            limit);
      }
   }
   *result_loc = line_array;

   DBGMSF(debug, "Returning: %d", rc);
   return rc;
}


#ifdef USE_SHELL
/* Helper function that scans a single log file
 *
 * \param  pre_grep     portion of command before the grep command
 * \param  grep_cmd     grep command
 * \param  post_grep    portion of command after the grep command
 * \param  title        describes what is being scanned
 * \param  depth        logical indentation depth
 */
bool probe_one_log_using_shell(
      char * pre_grep,
      char * grep_cmd,
      char * post_grep,
      char * title,
      int    depth)
{
   bool debug = false;
   bool result = true;
   assert(grep_cmd);
   assert(title);
   DBGMSF(debug, "Starting. pre_grep=\"%s\", grep_cmd=\"%s\", post_grep=\"%s\", title=\"%s\"",
          pre_grep, grep_cmd, post_grep, title);
   int l1 = (pre_grep) ? strlen(pre_grep) : 0;
   int l2 = strlen(grep_cmd);
   int l3 = (post_grep) ? strlen(post_grep) : 0;
   int bsz = l1 + l2 + l3 + 1;
   char * buf = malloc(bsz);
   DBGMSF(debug, "Allocated buffer of size %d", bsz);
   buf[0] = '\0';
   if (pre_grep)
      strcpy(buf, pre_grep);
   strcat(buf, grep_cmd);
   if (post_grep)
      strcat(buf, post_grep);

   // rpt_vstring(depth,"Checking %s for video and I2C related lines...", title);
   rpt_vstring(depth,"Checking %s for I2C related lines...", title);
   DBGMSF(debug, "Shell command, len=%d: \"%s\"", strlen(buf), buf);
   // GPtrArray * all_lines = execute_shell_cmd_collect(buf);
   // for (int ndx = 0; ndx < all_lines->len; ndx++)
   //    rpt_vstring(5, "%d: %s", ndx, g_ptr_array_index(all_lines, ndx));
   if ( !execute_shell_cmd_rpt(buf, depth+1) ) {
      rpt_vstring(depth+1,"Unable to process %s", title);
      result = false;
   }
   rpt_nl();
   free(buf);
   DBGMSF(debug, "Done.  Returning %s", bool_repr(result));
   return result;
}
#endif

/*  Helper function for building egrep command.  Appends search terms.
 *
 *  \param  terms  null terminated array of grep terms
 *  \param  buf    pointer to buffer to which terms are appended
 *  \param  bufsz  buffer size
 */
void add_egrep_terms(char ** terms, char * buf, int bufsz) {
   bool debug = false;
   DBGMSF(debug, "Starting. buf=|%s|", buf);
   char ** p = terms;
   char * src = NULL;
   while (*p) {
      // 11/4/2017: quoted search terms suddenly causing parsing error when
      // string is passed to popen() and the command ends with a "| head" or "|tail"
      // why?
      // Eliminated adding quotes and there's no problem
      // Luckily no search terms contain blanks
      // Alas, not true for future use - Raspbian
      // problem solved by eliminating spaces around "|" before "head"or "tail"

   src = " -e\""; strncat(buf, src, bufsz - (strlen(buf)+1));
   //                  strncat(buf, " -e\"", bufsz - (strlen(buf)+1));
                     strncat(buf, *p,  bufsz - (strlen(buf)+1));
   src = "\"";    strncat(buf, src, bufsz - (strlen(buf)+1));
   //                  strncat(buf, "\"", bufsz - (strlen(buf)+1));
#ifdef ALT
                   strncat(buf, " -e ", bufsz - (strlen(buf)+1));
                     strncat(buf, *p,  bufsz - (strlen(buf)+1));
   //                  strncat(buf, "\"", bufsz - (strlen(buf)+1));
#endif
      p++;
   }
   DBGMSF(debug, "Done. len=%d, buf=|%s|", strlen(buf), buf);
}


#ifdef USING_SHELL
/** Scan one log file using grep
 *
 *  \param  log_fn      file name
 *  \param  terms       #Null_Terminated_String_Array of grep terms
 *  \param  ignore_case if true, perform case insensitive grep
 *  \param  limit       if > 0, report only the first #limit lines found
 *                      if < 0, report only the last #limit lines found
 *                      if 0, report all lines found
 *  \param  depth       logical indentation depth
 *  \return true if log find found, false if not
 */
bool probe_log_using_shell(
      char *  log_fn,
      char ** terms,
      bool    ignore_case,
      int     limit,
      int     depth)
{
   bool debug = false;;
   DBGMSF(debug, "Starting.  log_fn=%s", log_fn);

   assert(log_fn);
   assert(terms);
   bool file_found = false;
   if (regular_file_exists(log_fn)) {
      const int limit_buf_sz = 200;
      char limit_buf[limit_buf_sz];
      limit_buf[0] = '\0';
      if (limit < 0) {
         rpt_vstring(depth, "Limiting output to last %d lines...", -limit);
         snprintf(limit_buf, 200, " %s|tail -n %d", log_fn, -limit);
      }
      else if (limit > 0) {
         rpt_vstring(depth, "Limiting output to first %d lines...", limit);
         snprintf(limit_buf, 200, " %s|head -n %d", log_fn, limit);
      }
      else {
         snprintf(limit_buf, 200, " %s", log_fn);
      }
      DBGMSF(debug, "limit_buf size=%d, len=%d, \"%s\"",
                    limit_buf_sz, strlen(limit_buf), limit_buf);
      char gbuf[1000];
      int  gbufsz = 1000;

      if (ignore_case)
         strncpy(gbuf, "grep -E -i ", gbufsz);
      else
         strncpy(gbuf, "grep -E ", gbufsz);
      add_egrep_terms(terms, gbuf, gbufsz);
      DBGMSF(debug, "gbuf size=%d, len=%d, \"%s\"",
                    gbufsz, strlen(gbuf), gbuf);
      probe_one_log_using_shell(NULL, gbuf, limit_buf, log_fn, depth);
      file_found = true;
   }
   else
      rpt_vstring(depth, "File not found: %s", log_fn);

   DBGMSF(debug, "Done. Returning %s", bool_repr(file_found));
   return file_found;
}
#endif


bool probe_log_using_api(
      char *  log_fn,
      char ** filter_terms,
      bool    ignore_case,
      int     limit,
      int     depth)
{
   bool debug = false;
   assert(log_fn);
   DBGMSF(debug, "Starting. log_fn=%s, filter_terms=%p, ignore_case=%s, limit=%d",
                 log_fn, filter_terms, bool_repr(ignore_case), limit);
   bool file_found = false;
   int rc = 0;
   if (regular_file_exists(log_fn)) {
      rpt_vstring(depth, "Scanning file: %s", log_fn);
      if (limit < 0) {
         rpt_vstring(depth, "Limiting output to last %d relevant lines...", -limit);
      }
      else if (limit > 0) {
          rpt_vstring(depth, "Limiting output to first %d relevant lines...", limit);
      }
      GPtrArray * found_lines = g_ptr_array_new_full(1000, g_free);
      rc = read_file_with_filter(found_lines, log_fn, filter_terms, ignore_case, limit);
      if (rc < 0) {
         f0printf(FERR, "Error reading file: %s\n", psc_desc(rc));
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
   }
   else {
      rpt_vstring(depth, "File not found: %s", log_fn);
      rc = -ENOENT;
   }
   DBGMSF(debug, "rc=%d, file_found=%s", rc, bool_repr(file_found));
   rpt_nl();
   return file_found;
}



bool probe_cmd_using_api(
      char *  cmd,
      char ** filter_terms,
      bool    ignore_case,
      int     limit,
      int     depth)
{
   bool debug = false;
   assert(cmd);
   DBGMSF(debug, "Starting. cmd=%s, filter_terms=%p, ignore_case=%s, limit=%d",
                 cmd, filter_terms, bool_repr(ignore_case), limit);

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
      f0printf(FERR, "Error executing command: %s\n", psc_desc(rc));
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

   bool result = (rc >= 0);
   DBGMSF(debug, "rc=%d, returning %s", rc, bool_repr(result));
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

#ifdef USE_SHELL
   char gbuf[500];             // contains grep command
   int  gbufsz = sizeof(gbuf);
#endif

   int depth = 0;
   int d1 = depth+1;
   int d2 = depth+2;
   // DBGMSG("Starting");
   // debug_output_dest();

   rpt_nl();
   rpt_title("Examining system logs...", depth);

   // TODO: Pick simpler data structures.  Is Value_Name_Title_Table worth it?

   const Byte LOG_XORG       = 0x80;
   const Byte LOG_DAEMON     = 0x40;
   const Byte LOG_SYSLOG     = 0x20;
   const Byte LOG_KERN       = 0x10;
   const Byte LOG_JOURNALCTL = 0x08;
   const Byte LOG_MESSAGES   = 0x04;
   const Byte LOG_DMESG      = 0x02;

   Value_Name_Title_Table log_table = {
         VNT(LOG_DMESG,      "dmesg"              ),
         VNT(LOG_JOURNALCTL, "journalctl"         ),
         VNT(LOG_DAEMON,     "/var/log/daemon.log" ),
         VNT(LOG_KERN,       "/var/log/kern.log"  ),
         VNT(LOG_MESSAGES,   "/var/log/messages"  ),
         VNT(LOG_SYSLOG,     "/var/log/syslog"    ),
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

#ifdef USE_SHELL
   strncpy(gbuf, "egrep -i", gbufsz);
   add_egrep_terms(known_video_driver_modules, gbuf, gbufsz);
#endif

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

#ifdef USE_SHELL
   add_egrep_terms(addl_matches, gbuf, gbufsz);
#endif

   Null_Terminated_String_Array drivers_plus_addl_matches =
            ntsa_join(get_known_video_driver_module_names(), addl_matches, /*dup*/ false);

   // *** dmesg ***

   rpt_nl();
   // first few lines of dmesg are lost.  turning on any sort of debugging causes
   // them to reappear.  apparently a NL in the stream does the trick.  why?
   // it's a heisenbug.  Just use the more verbose journalctl output
   logs_checked |= LOG_DMESG;
#ifdef USE_SHELL
   log_dmesg_found = probe_one_log_using_shell("dmesg |",      gbuf, NULL,                   "dmesg",      depth+1);
#endif

   // DBGMSG("Alternative using API:");
   rpt_title("Scanning dmesg output for I2C related entries...", depth+1);
   log_dmesg_found = probe_cmd_using_api("dmesg", drivers_plus_addl_matches, /*ignore_case*/ true, 0, depth+1);
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
   // DBGMSG("Using probe_cmd_using_api()...:");
   rpt_title("Scanning journalctl output for I2C related entries...", depth+1);
   log_dmesg_found = probe_cmd_using_api("journalctl --no-pager --boot", drivers_plus_addl_matches, /*ignore_case*/ true, 0, depth+1);
   if (log_dmesg_found)
      logs_found |= LOG_DMESG;
    rpt_nl();

   // 11/4/17:  Now getting error msgs like:
   //   sh: 1: Syntax error: end of file unexpected
   // apparent problem with pipe and execute_shell_cmd_report()

   // no, it's journalctl that's the offender.  With just journalctl, earlier
   // messages re Summary of Udev devices is screwed up
   // --no-pager solves the problem

   // DBGMSG("Using probe_one_log_using_shell()...");
   // probe_one_log_using_shell("journalctl --no-pager --boot|", gbuf, NULL,                   "journalctl", depth+1);


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
#ifdef ALT
      DBGMSG("Using probe_log_using_shell()...");
      log_xorg_found = probe_log_using_shell("/var/log/Xorg.0.log", xorg_terms, /*ignore_case*/ true, 0, depth+1);
      if (log_xorg_found)
         logs_found |= LOG_XORG;
#endif
      // DBGMSG("Using probe_log_using_api...");
      log_xorg_found =  probe_log_using_api("/var/log/Xorg.0.log", xorg_terms, /*ignore_case*/ true,  0, depth+1);
      if (log_xorg_found)
         logs_found |= LOG_XORG;
   }
   else {
      logs_checked |= LOG_XORG;
      // rpt_vstring(depth+1, "Limiting output to 200 lines...");

#ifdef SHELL_CMD
      DBGMSG("Using probe_one_log_using_shell()...");
      log_xorg_found = probe_one_log_using_shell(NULL,           gbuf, " /var/log/Xorg.0.log|head -n 200", "Xorg.0.log", depth+1);
      if (log_xorg_found)
         logs_found |= LOG_XORG;
#endif

      // DBGMSG("Using probe_log_using_api...");
      log_xorg_found =  probe_log_using_api("/var/log/Xorg.0.log", drivers_plus_addl_matches, /*ignore_case*/ true, 200, depth+1);
      if (log_xorg_found)
         logs_found |= LOG_XORG;
   }


   // ***/var/log/kern.log, /var/log/daemon.log, /var/log/syslog, /va/log/messages ***

#ifdef USING_SHELL
   // Problem: Commands sometimes produce obscure shell error messages
   log_messages_found = probe_log_using_shell("/var/log/messages",   log_terms, /*ignore_case*/ true, -40, d1);
   log_kern_found     = probe_log_using_shell("/var/log/kern.log",   log_terms, /*ignore_case*/ true, -20, d1);
   log_daemon_found   = probe_log_using_shell("/var/log/daemon.log", log_terms, /*ignore_case*/ true, -10, d1);
   log_syslog_found   = probe_log_using_shell("/var/log/syslog",     log_terms, /*ignore_case*/ true, -50, d1);
#endif

   // Using our own code instead of shell to scan files
   log_messages_found = probe_log_using_api("/var/log/messages",   log_terms, /*ignore_case*/ true, -40, d1);
   log_kern_found     = probe_log_using_api("/var/log/kern.log",   log_terms, /*ignore_case*/ true, -20, d1);
   log_daemon_found   = probe_log_using_api("/var/log/daemon.log", log_terms, /*ignore_case*/ true, -10, d1);
   log_syslog_found   = probe_log_using_api("/var/log/syslog",     log_terms, /*ignore_case*/ true, -50, d1);

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
                      bool_repr(logs_checked & entry->value),
                      bool_repr(logs_found & entry->value));
   }
   rpt_nl();
   if (log_terms != all_terms)
      ntsa_free(log_terms, false);
   ntsa_free(all_terms, false);
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
#ifdef OLD
      probe_one_log_using_shell(
            NULL,
            "egrep -i -e\"dtparam\" -e\"dtoverlay\"",
            " /boot/config.txt | grep -v \"^ *#\"",
            "/boot/config.txt",
            depth+1
            );
#endif
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
      execute_shell_cmd_rpt("grep I2C /boot/config-$(uname -r)", 1 /* depth */);
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

