/* subprocess_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "util/subprocess_util.h"


/* Executes a shell command and writes the output to the terminal
 *
 * Arguments:
 *    shell_cmd      command to execute
 *    depth          logical report indentation depth
 *
 * Returns:
 *    true           command succeeded
 *    false          failed, e.g. command not found
 */
bool execute_shell_cmd(char * shell_cmd, int depth) {
   bool debug = false;
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
          rpt_title(a_line, depth);
          // fputs(a_line, stdout);
          // free(a_line);
       }
       int pclose_rc = pclose(fp);
       if (debug)
          printf("(%s) plose() rc = %d\n", __func__, pclose_rc);
    }
    return ok;
 }
