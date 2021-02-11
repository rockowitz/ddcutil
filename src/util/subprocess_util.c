/** @file subprocess_util.c
 *
 * Functions to execute shell commands
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "config.h"

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
   if (debug)
      printf("(%s) Starting. shell_cmd = |%s|\n", __func__, shell_cmd);
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
                   printf("(%s) Truncating character '%c' (0x%02x)\n", __func__, ch, ch);
                // else
                //    printf("(%s) Truncating expected NL (0x%02x)\n", __func__, ch);
             }
             a_line[strlen(a_line)-1] = '\0';
          }
          else
             printf("(%s) Zero length line\n", __func__);
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
       // per getline() doc, buffer is allocated even if getline(),
       free(a_line);
       int pclose_rc = pclose(fp);
       int errsv = errno;
       if (debug)
          printf("(%s) pclose() rc=%d, error=%d - %s\n", __func__, pclose_rc, errsv, strerror(errsv));
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
 *
 *  @return :GPtrArray of response lines if command succeeded
 *           NULL                        if command failed, e.g. command not found
 */
GPtrArray * execute_shell_cmd_collect(const char * shell_cmd) {
   bool debug = false;
   GPtrArray * result = g_ptr_array_new();
   g_ptr_array_set_free_func(result, g_free);
   if (debug)
      printf("(%s) Starting. shell_cmd = |%s|", __func__, shell_cmd);
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
       fprintf(stderr, "Unable to execute command \"%s\": %s\n", shell_cmd, strerror(errno));
       ok = false;
    }
    else {
       char * a_line = NULL;
       size_t len = 0;
       bool first_line = true;
       while ( getline(&a_line, &len, fp) >= 0) {
          if (strlen(a_line) > 0)
             a_line[strlen(a_line)-1] = '\0';
          if (first_line) {
             if (str_ends_with(a_line, "not found")) {
                // printf("(%s) found \"not found\"\n", __func__);
                ok = false;
                break;
             }
             first_line = false;
          }
          g_ptr_array_add(result, strdup(a_line));
          free(a_line);
          a_line = NULL;
          len = 0;
       }
       free(a_line);
       int pclose_rc = pclose(fp);
       if (debug)
          printf("(%s) plose() rc = %d\n", __func__, pclose_rc);
    }
    if (!ok) {
       g_ptr_array_free(result, true);
       result = NULL;
    }
    free(cmdbuf);
    return result;
 }


/** Execute a shell command and return the contents in a newly allocated
 *  #GPtrArray of lines. Optionally, keep only those lines containing at least
 *  one in a list of terms.  After filtering, the set of returned lines may
 *  be further reduced to either the first or last n number of lines.
 *
 *  \param  cmd        command to execute
 *  \param  fn         file name
 *  \param  filter_terms  #Null_Terminated_String_Away of filter terms
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
   if (debug)
      printf("(%s) cmd|%s|, ct(filter_terms)=%d, ignore_case=%s, limit=%d\n",
            __func__, shell_cmd, ntsa_length(filter_terms), sbool(ignore_case), limit);

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
            limit);
      }
   }
   *result_loc = line_array;

   if (debug)
      printf("(%s) Returning: %d\n", __func__, rc);
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
      result = strdup(g_ptr_array_index(response, 0));
      g_ptr_array_free(response, true);
   }
   return result;
}


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
