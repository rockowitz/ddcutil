/** @file backtrace.c */

// Copyright (C) 2016-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
/** \cond */
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifdef UNUSED
#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/types.h>
#include <sys/syscall.h>
#include <syslog.h>
#endif
#endif
/** \endcond */

#include "string_util.h"

#include "backtrace.h"

// HACK
#ifdef TARGET_BSD
#undef HAVE_EXECINFO_H
#endif

#ifdef HAVE_EXECINFO_H
/* Extracts the function name and offset from a backtrace line
 *
 * \param  bt_line   line returned by backtrace()
 * \param  name_only if true, return only the name
 * \return string of form "name+offset" or just "name".
 *         It is the responsibility of the caller to free this string.
 */
static char * extract_function(char * bt_line, bool name_only) {
   bool debug = false;
   if (debug)
      printf("\n(%s) bt_line = |%s|\n", __func__, bt_line);
   char * result = NULL;
   char * start = strchr(bt_line, '(');
   if (!start) {
      result = g_strdup("???");
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
   if (name_only) {
      char *p = strchr(result, '+');
      if (p) {
         *p = '\0';
         char * res = g_strdup(result);
         free(result);
         result = res;
      }
   }

   if (debug)
      printf("(%s) Returning %p -> |%s|\n", __func__, result, result);
   return result;
}
#endif


#ifdef OLD
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
            char * s = extract_function(strings[j], true);
            printf("   %s\n", s);
            bool final = (streq(s, "main")) ? true : false;
            free(s);
            if (final)
                  break;
         }
      }

      free(strings);
   }
}
#endif

/** Returns an array of function names for the backtrace stack.
 *
 *  @param stack_adjust  adjust the start of the reported functions
 *  @return array of strings of names of functions, caller must deep free
 */
GPtrArray * get_backtrace(int stack_adjust) {
#ifdef HAVE_EXECINFO_H
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  stack_adjust = %d\n", __func__, stack_adjust);

   GPtrArray * result = NULL;
   int j, nptrs;
   const int MAX_ADDRS = 100;
   void *buffer[MAX_ADDRS];
   char **strings;

   nptrs = backtrace(buffer, MAX_ADDRS);
   if (debug)
      printf("(%s) backtrace() returned %d addresses\n", __func__, nptrs);

   strings = backtrace_symbols(buffer, nptrs);
   if (strings == NULL) {
      if (debug)
         perror("backtrace_symbols unavailable");
   }
   else {
      result = g_ptr_array_sized_new(nptrs-stack_adjust);
      if (debug)
         printf("Current call stack\n");
      for (j = 0; j < nptrs; j++) {
         if (j < stack_adjust) {
            if (debug)
               printf("(%s) Suppressing %s\n", __func__, strings[j]);
         }
         else {
            // printf("   %s\n", strings[j]);
            char * s = extract_function(strings[j], true); // caller must free s
            if (debug)
               printf("   %s\n", s);
            g_ptr_array_add(result, s);
            bool final = (streq(s, "main")) ? true : false;
            if (final)
                  break;
         }
      }

      free(strings);
   }
   return result;
#else
   return NULL;
#endif
}



#ifdef FUTURE
void gptrarray_to_syslog(int priority, GPtrArray* lines) {
}
#endif


void backtrace_to_syslog(int syslog_priority, int stack_adjust) {
   GPtrArray * callstack = get_backtrace(stack_adjust+2); // +2 for get_backtrace(), backtrace()
   if (!callstack) {
      syslog(LOG_PERROR|LOG_ERR, "backtrace unavailable");
   }
   else {
      syslog(syslog_priority, "Current call stack:");

      for (int ndx = 0; ndx < callstack->len; ndx++) {
         syslog(syslog_priority, "   %s", (char *) g_ptr_array_index(callstack, ndx));
      }
      g_ptr_array_set_free_func(callstack, g_free);
      g_ptr_array_free(callstack, true);
   }
}

