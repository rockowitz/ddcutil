/** @file subprocess_util.c
 *
 * Functions to execute shell commands
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
/** \endcond */

#include "config.h"

#include "debug_util.h"
#include "file_util.h"
#include "glib_util.h"
#include "report_util.h"
#include "string_util.h"

#include "subprocess_util.h"


/** Executes a shell command and writes the output to the current report destination
 * or to stdout.
 *
 * @param   shell_cmd      command to execute
 * @param   depth          logical report indentation depth,
 *                         if < 0, write to stdout
 *
 * @return   true           command succeeded
 *           false          failed, e.g. command not found
 */
bool execute_shell_cmd_rpt(const char * shell_cmd, int depth) {
   bool debug = false;
   DBGF(debug, "Starting. shell_cmd = |%s|", shell_cmd);
   bool ok = true;
   FILE * fp;
   int bufsz = strlen(shell_cmd) + 50;
   char * cmdbuf = calloc(1, bufsz);
   snprintf(cmdbuf, bufsz, "(%s) 2>&1", shell_cmd);
   // printf("(%s) cmdbuf=|%s|\n", __func__, cmdbuf);
   fp = popen(cmdbuf, "r");
   if (!fp) {
      printf("Unable to execute command \"%s\": %s\n", shell_cmd, strerror(errno));
      ok = false;
   }
   else {
       char * a_line = NULL;
       size_t len = 0;
       bool first_line = true;
       while ( getline(&a_line, &len, fp) >= 0) {
          if (strlen(a_line) > 0) {
             // printf("(%s) a_line: |%s|\n", __func__, a_line);
             int ch = a_line[strlen(a_line)-1];
             if (debug) {
                if (ch != '\n')
                   DBG("Truncating character '%c' (0x%02x)", ch, ch);
                // else
                //    printf("(%s) Truncating expected NL (0x%02x)\n", __func__, ch);
             }
             a_line[strlen(a_line)-1] = '\0';
          }
          else
             DBG("Zero length line");
          if (first_line) {
             if (str_ends_with(a_line, "not found")) {
                // printf("(%s) found \"not found\"\n", __func__);
                ok = false;
                break;
             }
             first_line = false;
          }

          if (debug && !str_all_printable(a_line)) {
             printf("(%s) String contains non-printable character!\n", __func__);
          }
          // printf("%s", "\n");   // solves the missing line problem, but why?
          if (depth < 0) {
             fputs(a_line, stdout);
             fputs("\n", stdout);
          }
          else {
             // n. output will be sent to current rpt_ dest !
             rpt_title(a_line, depth);
          }

          free(a_line);   // 1/2018 was commented out, why?
          a_line = NULL;
          len = 0;
       }
       // per getline() doc, buffer is allocated even if getline() fails
       free(a_line);
       int pclose_rc = pclose(fp);
       int errsv = errno;
       DBGF(debug, "pclose() rc=%d, error=%d - %s", pclose_rc, errsv, strerror(errsv));
    }
    free(cmdbuf);
    return ok;
 }


/** Executes a shell command and writes the output to stdout.
 *
 * @param shell_cmd      command to execute
 *
 * @return  true           command succeeded
 *          false          failed, e.g. command not found
 */
bool execute_shell_cmd(const char * shell_cmd) {
   return execute_shell_cmd_rpt(shell_cmd, -1);
}


/** Executes a shell command and returns the output as an array of strings.
 *
 *  @param shell_cmd      command to execute
 *  @param collector      if non-null, use it instead of allocating new GPtrArray
 *  @param emsg_loc       if non-null, return error msg here instead of writing
 *                        it to stderr
 *
 *  @return :GPtrArray of response lines if command succeeded
 *           NULL                        if command failed, e.g. command not found
 */
GPtrArray * execute_shell_cmd_collect1(
               const char* shell_cmd, GPtrArray* collector, char** emsg_loc)
{
   bool debug = false;
   GPtrArray* result = collector;
   if (!result)
      result = g_ptr_array_new_with_free_func(g_free);
   DBGF(debug, "Starting. shell_cmd = |%s|", shell_cmd);
   bool ok = true;
   FILE * fp;
   int bufsz = strlen(shell_cmd) + 50;
   char * cmdbuf = calloc(1, bufsz);
   snprintf(cmdbuf, bufsz, "(%s) 2>&1", shell_cmd);
   // printf("(%s) cmdbuf=|%s|\n", __func__, cmdbuf);
   fp = popen(cmdbuf, "r");
   // printf("(%s) open. errno=%d\n", __func__, errno);
    if (!fp) {
       // int errsv = errno;
       char * emsg = g_strdup_printf("Unable to execute command \"%s\": %s", shell_cmd, strerror(errno));
       if (emsg_loc)
          *emsg_loc = strdup(emsg);
       else
          fprintf(stderr, "%s", emsg);
       free(emsg);
       ok = false;
    }
    else {
       char * a_line = NULL;
       size_t len = 0;
       bool first_line = true;
       while ( getline(&a_line, &len, fp) >= 0) {
          if (strlen(a_line) > 0)
             a_line[strlen(a_line)-1] = '\0';
          DBGF(debug, "a_line = |%s|\n", a_line);
          if (first_line) {
             if (str_ends_with(a_line, "not found")) {
                // printf("(%s) found \"not found\"\n", __func__);
                ok = false;
                if (emsg_loc)
                   *emsg_loc = strdup(a_line);
                else
                   fprintf(stderr, "%s\n", a_line);
                break;
             }
             first_line = false;
          }
          g_ptr_array_add(result, g_strdup(a_line));
          free(a_line);
          a_line = NULL;
          len = 0;
       }
       free(a_line);
       int pclose_rc = pclose(fp);
       DBGF(debug, "pclose() rc = %d", pclose_rc);
    }
    if (!ok) {
       // g_ptr_array_free(result, true);
       result = NULL;
    }
    free(cmdbuf);
    return result;
 }


/** Executes a shell command and returns the output as an array of strings.
 *
 *  @param shell_cmd      command to execute
 *  @param collector      if non-null, use it instead of allocating new GPtrArray
 *
 *  @return :GPtrArray of response lines if command succeeded
 *           NULL                        if command failed, e.g. command not found
 */
GPtrArray * execute_shell_cmd_collect0(const char * shell_cmd, GPtrArray* collector) {
   bool debug = false;
   DBGF(debug, "Starting. shell_cmd = |%s|, collector = %p", shell_cmd, collector);
   return execute_shell_cmd_collect1(shell_cmd, collector, NULL);
}


/** Executes a shell command and returns the output as an array of strings.
 *
 *  @param shell_cmd      command to execute
 *
 *  @return :GPtrArray of response lines if command succeeded
 *           NULL                        if command failed, e.g. command not found
 *
 *  @remark
 *  **g_free** is set as the free function on the returned array
 */
GPtrArray * execute_shell_cmd_collect(const char * shell_cmd) {
   GPtrArray * lines = g_ptr_array_new_with_free_func(g_free);
   return execute_shell_cmd_collect0(shell_cmd, lines);
}

// TODO factor out variant of execute_cmd_collect_with_filter()
//      that returns error msg instead of writing to terminal,
//      as in execute_commena_collect1()

/** Execute a shell command and return the contents in a newly allocated
 *  #GPtrArray of lines. Optionally, keep only those lines containing at least
 *  one in a list of terms.  After filtering, the set of returned lines may
 *  be further reduced to either the first or last n number of lines.
 *
 *  \param  cmd        command to execute
 *  \param  fn         file name
 *  \param  filter_terms  #Null_Terminated_String_Array of filter terms
 *  \param  ignore_case   ignore case when testing filter terms
 *  \param  limit if 0, return all lines that pass filter terms
 *                if > 0, return at most the first #limit lines that satisfy the filter terms
 *                if < 0, return at most the last  #limit lines that satisfy the filter terms
 *  \param  result_loc  address at which to return a pointer to the newly allocate #GPtrArray
 *  \return if >= 0, number of lines before filtering and limit applied
 *          if < 0,  -errno
 */
int execute_cmd_collect_with_filter(
      const char * shell_cmd,
      char **      filter_terms,
      bool         ignore_case,
      int          limit,
      GPtrArray ** result_loc)
{
   bool debug = false;
   DBGF(debug, "cmd|%s|, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
           shell_cmd, ntsa_length(filter_terms), sbool(ignore_case), limit);

   int rc = 0;
   GPtrArray *line_array = execute_shell_cmd_collect(shell_cmd);
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
            limit,
            false);     // line_array has free function set
      }
   }
   *result_loc = line_array;

   DBGF(debug, "Returning: %d", rc);
   return rc;
}


/** Executes a shell command that always outputs a single line and returns the
 * output as a newly allocated character string
 *
 *  @param shell_cmd      command to execute
 *
 *  @return :response   if command succeeded
 *           NULL       if command failed, e.g. command not found
 *
 *  @remark
 *  Caller is responsible for freeing the returned string.
 */
char * execute_shell_cmd_one_line_result(const char * shell_cmd) {
   char * result = NULL;
   GPtrArray * response = execute_shell_cmd_collect(shell_cmd);
   if (response) {
      if (response->len > 0)
         result = g_strdup(g_ptr_array_index(response, 0));
      g_ptr_array_free(response, true);
   }
   return result;
}


#ifdef OLD
/** Tests if a command is found in path
 *
 *  @param cmd command name
 *
 *  @return true/false
 *
 *  TODO: Check that actually executable,
 *        e.g. could be in /sbin and not running privileged
 */
bool is_command_in_path(const char * cmd) {
   bool result = false;
   char shell_cmd[100];
   snprintf(shell_cmd, sizeof(shell_cmd), "which %s", cmd);
   GPtrArray * resp = execute_shell_cmd_collect(shell_cmd);
   if (resp) {
      if (resp->len > 0)
         result = true;
      g_ptr_array_free(resp, true);
   }
   return result;
}
#endif


/** Tests if a name is a regular file this process can execute.
 *
 *  @param  fqfn  fully qualified file name
 *  @return true/false
 *
 *  @remark
 *  access() alone is not sufficient: it reports X_OK for a directory, so
 *  /usr/bin would answer yes for any command name.
 */
static bool is_executable_file(const char * fqfn) {
   struct stat st;
   return stat(fqfn, &st) == 0 && S_ISREG(st.st_mode) && access(fqfn, X_OK) == 0;
}


/** Tests if a command is found in path
 *
 *  @param cmd command name
 *
 *  @return true/false
 *
 *  @remark
 *  Walks $PATH directly rather than running "which".  Forking a shell to
 *  answer a question about the file system cost two processes per call, and
 *  made the answer depend on which "which" is installed: it is a shell
 *  builtin in some shells, a debianutils script on Debian, a GNU binary
 *  elsewhere, and absent altogether in minimal containers.  In that last
 *  case the old test reported every command PRESENT: execute_shell_cmd_collect()
 *  appends 2>&1, so the shell's own "which: not found" was captured as
 *  output, and the test was merely whether any output appeared.
 *
 *  @remark
 *  Also answers the question the former TODO here asked.  "which" reports
 *  what it finds on the path, whereas this tests X_OK, so a command in
 *  /usr/sbin that the current user cannot execute now reports false rather
 *  than true.  That is the intended reading of the name for the callers in
 *  query_sysenv_drm.c, which run the command if it is reported present.
 */
bool is_command_in_path(const char * cmd) {
   bool result = false;

   if (cmd && *cmd) {
      if (strchr(cmd, '/')) {
         // Contains a separator, so it names a file rather than a command to
         // be looked up.  $PATH is not searched, as in "which" and in execvp().
         result = is_executable_file(cmd);
      }
      else {
         const char * path = getenv("PATH");
         if (!path)
            path = "/usr/local/bin:/usr/bin:/bin";   // as good a default as any
         const char * seg = path;
         while (!result) {
            const char * colon = strchr(seg, ':');
            size_t seglen = (colon) ? (size_t)(colon - seg) : strlen(seg);
            // An empty segment means the current directory, per POSIX.  Not
            // searched: a file in the working directory is not a command
            // "in path" for our purposes, and treating it as one would let
            // an unrelated file decide what sysenv reports.
            if (seglen > 0 && seglen + 1 + strlen(cmd) < PATH_MAX) {
               char fqfn[PATH_MAX];
               g_snprintf(fqfn, sizeof(fqfn), "%.*s/%s", (int) seglen, seg, cmd);
               result = is_executable_file(fqfn);
            }
            if (!colon)
               break;
            seg = colon + 1;
         }
      }
   }

   return result;
}


#ifdef OLD
/** Tests if a command is executable.
 *
 *  \param cmd command to test execute
 *  \retval   0    ok
 *  \retval 127    command not found
 *  \retval   2    command requires sudo
 *  \retval   1    command executed, but with some error
 */
int test_command_executability(const char * cmd) {
   assert(cmd);
   char * full_cmd = calloc(1, strlen(cmd) + 20);
   strcpy(full_cmd, cmd);
   strcat(full_cmd, ">/dev/null 2>&1");
   // printf("(%s) cmd: |%s|, full_cmd: |%s|\n", __func__, cmd, full_cmd);
   int rc = system(full_cmd);
   // printf("(%s) system(%s) returned: %d, %d, %d\n", __func__, full_cmd, rc, WIFEXITED(rc), WEXITSTATUS(rc));
   free(full_cmd);

   // 0 ok
   // 127 command not found
   // 2 on dmidecode - not running sudo
   // 2 on i2cdetect - not sudo
   // 1 on i2cdetect - sudo, but some error

#ifdef TARGET_BSD
   return (rc & 0xff00) >> 8;
#else
   return WEXITSTATUS(rc);
#endif
}
#endif


/** Tests if a command is executable, by executing it and reporting its
 *  exit status.
 *
 *  \param cmd command to test execute
 *  \retval   0    ok
 *  \retval 127    command not found
 *  \retval   2    command requires sudo
 *  \retval   1    command executed, but with some error
 *
 *  Output written by the command, on either stdout or stderr, is discarded.
 *
 *  @remark
 *  Unlike #is_command_in_path(), this function must run the command: its
 *  answer is the command's exit status, which no test of the file system can
 *  supply.  What is avoided is the shell.  The former implementation appended
 *  ">/dev/null 2>&1" to the caller's string and passed it to system(), which
 *  runs /bin/sh; g_spawn_command_line_sync() instead parses the string into
 *  an argument vector using shell quoting rules and execs it directly.
 *
 *  @remark
 *  The consequence is that shell metacharacters -- pipes, redirection, globs,
 *  variable expansion, command sequences -- are no longer interpreted, since
 *  there is no shell to interpret them.  A quoted command with simple
 *  arguments behaves as before.  The function has no callers, so nothing
 *  relies on the former behavior; a caller that needs a pipeline should use
 *  one of the execute_shell_cmd...() functions, which run a shell by design.
 *
 *  @remark
 *  system() reports 127 when the shell cannot find the command.  There is no
 *  shell to report that here, so a spawn failure for that reason is mapped to
 *  127 to keep the documented return values.
 */
int test_command_executability(const char * cmd) {
   assert(cmd);
   int result = 127;

   gchar * out = NULL;
   gchar * err = NULL;
   gint    wait_status = 0;
   GError * error = NULL;

   if (g_spawn_command_line_sync(cmd, &out, &err, &wait_status, &error)) {
      // 0 ok
      // 127 command not found
      // 2 on dmidecode - not running sudo
      // 2 on i2cdetect - not sudo
      // 1 on i2cdetect - sudo, but some error
#ifdef TARGET_BSD
      result = (wait_status & 0xff00) >> 8;
#else
      result = WEXITSTATUS(wait_status);
#endif
   }
   else {
      // G_SPAWN_ERROR_NOENT is the command not existing, which system() would
      // have reported as 127.  Any other failure is reported the same way:
      // the command did not run, so it is not executable.
      DBGF(false, "g_spawn_command_line_sync(|%s|) failed: %s", cmd,
                  (error) ? error->message : "(no message)");
      result = 127;
   }

   g_free(out);
   g_free(err);
   if (error)
      g_error_free(error);

   return result;
}
