/* debug_util.c
 *
 * <copyright>
 * Copyright (C) 2016-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file debug_util.c
 * Functions for debugging
 */

/** \cond */
#include <execinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/** \endcond */

#include "debug_util.h"

/* Extracts the function name and offset from a backtrace line
 *
 * Arguments:
 *   bt_line   line returned by backtrace()
 *
 * Returns:    string of form "name+offset".  It is the
 *             resposibility of the caller to free this string.
 */
static char * extract_function(char * bt_line) {
   bool debug = false;
   if (debug)
      printf("\n(%s) bt_line = |%s|\n", __func__, bt_line);
   char * result = NULL;
   char * start = strchr(bt_line, '(');
   if (!start) {
      result = strdup("???");
   }
   else {
      start++;          // character after paren
      char * end = strchr(start, ')');

      if (!end)
         end = bt_line + strlen(bt_line);
      int len = end - start;
      if (debug) {
         printf("(%s) start=%p, end=%p, len=%d\n", __func__, start, end, len);
         printf("(%s) extract is |%.*s|\n", __func__, len, start);
      }
      result = malloc(len+1);
      memcpy(result, start, len);
      result[len] = '\0';
   }
   if (debug)
      printf("(%s) Returning |%s|\n", __func__, result);
   return result;
}


/** Show the call stack.
 *
 * @param  stack_adjust  number of initial stack frames to ignore, to hide this
 *                       function and possibly some number of immediate callers
 *
 * @remark
 * Note that the call stack is approximate.  The underlying system function, backtrace()
 * relies on external symbols, so static functions will not be properly shown.
 */
void show_backtrace(int stack_adjust)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  stack_adjust = %d\n", __func__, stack_adjust);

   int j, nptrs;
   const int MAX_ADDRS = 100;
   void *buffer[MAX_ADDRS];
   char **strings;

   nptrs = backtrace(buffer, MAX_ADDRS);
   if (debug)
      printf("(%s) backtrace() returned %d addresses\n", __func__, nptrs);

   strings = backtrace_symbols(buffer, nptrs);
   if (strings == NULL) {
      perror("backtrace_symbols unavailable");
   }
   else {
      printf("Current call stack\n");
      for (j = 0; j < nptrs; j++) {
         if (j < stack_adjust) {
            if (debug)
               printf("(%s) Suppressing %s\n", __func__, strings[j]);
         }
         else {
            // printf("   %s\n", strings[j]);
            char * s = extract_function(strings[j]);
            printf("   %s\n", s);
            free(s);
         }
      }

      free(strings);
   }
}
