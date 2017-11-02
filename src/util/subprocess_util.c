/* subprocess_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file subprocess_util.c
* Functions to execute shell commands
*/

/** \cond */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

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
bool execute_shell_cmd_rpt(char * shell_cmd, int depth) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. shell_cmd = |%s|\n", __func__, shell_cmd);
   bool ok = true;
   FILE * fp;
   char cmdbuf[200];
   snprintf(cmdbuf, sizeof(cmdbuf), "(%s) 2>&1", shell_cmd);
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
       ssize_t read;
       bool first_line = true;
       while ( (read=getline(&a_line, &len, fp)) != -1) {
          if (strlen(a_line) > 0) {
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

          // free(a_line);
       }
       int pclose_rc = pclose(fp);
       if (debug)
          printf("(%s) plose() rc = %d\n", __func__, pclose_rc);
    }
    return ok;
 }


/** Executes a shell command and writes the output to stdout.
 *
 * @param shell_cmd      command to execute
 *
 * @return  true           command succeeded
 *          false          failed, e.g. command not found
 */
bool execute_shell_cmd(char * shell_cmd) {
   return execute_shell_cmd_rpt(shell_cmd, -1);
}


/** Executes a shell command and returns the output as an array of strings.
 *
 *  @param shell_cmd      command to execute
 *
 *  @return :GPtrArray of response lines if command succeeded
 *           NULL                        if command failed, e.g. command not found
 */
GPtrArray * execute_shell_cmd_collect(char * shell_cmd) {
   bool debug = false;
   GPtrArray * result = g_ptr_array_new();
   // TO DO: set free func
   if (debug)
      printf("(%s) Starting. shell_cmd = |%s|", __func__, shell_cmd);
   bool ok = true;
   FILE * fp;
   char cmdbuf[200];
   snprintf(cmdbuf, sizeof(cmdbuf), "(%s) 2>&1", shell_cmd);
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
       ssize_t read;
       bool first_line = true;
       while ( (read=getline(&a_line, &len, fp)) != -1) {
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
       }
       int pclose_rc = pclose(fp);
       if (debug)
          printf("(%s) plose() rc = %d\n", __func__, pclose_rc);
    }
    if (!ok) {
       g_ptr_array_free(result, true);
       result = NULL;
    }
    return result;
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
char * execute_shell_cmd_one_line_result(char * shell_cmd) {
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
bool is_command_in_path(char * cmd) {
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



int test_command_executability(char * cmd) {
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


   return WEXITSTATUS(rc);
}
