/** @file traced_function_stack.c */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "backtrace.h"
#include "common_inlines.h"
#include "common_printf_formats.h"
#include "debug_util.h"
#include "glib_util.h"
#include "report_util.h"
#include "string_util.h"

#include "traced_function_stack.h"

bool              traced_function_stack_enabled = false;
bool              traced_function_stack_errors_fatal = false;
static __thread bool     traced_function_stack_suspended = false;
static __thread bool     debug_tfs = false;
__thread GQueue * traced_function_stack;

static GPtrArray *   all_traced_function_stacks = NULL;
static GMutex        all_traced_function_stacks_mutex;
static __thread bool traced_function_stack_invalid = false;

static void list_traced_function_stacks();


/** Turns debug messages on or off for the current thread.
 *
 *  @param  newval  new setting
 *  @return old setting
 */
bool set_debug_thread_tfs(bool newval) {
   bool old = debug_tfs;
   if (traced_function_stack_enabled)
      debug_tfs = newval;
   // printf("(%s) debug_tfs\n", sbool(debug_tfs));
   return old;
}


/** Delete all entries in the traced function stack for the current thread,
 *  and reset the traced_function_stack_invalid flag.
 */
void reset_current_traced_function_stack() {
   bool debug = false;
   debug = debug || debug_tfs;

   DBGF(debug, PRItid "Starting", TID());

   if (traced_function_stack) {
      int ct = g_queue_get_length(traced_function_stack);
      for (int ctr = 0; ctr<ct; ctr++) {
         char * funcname = g_queue_pop_tail(traced_function_stack);
         DBGF(debug, PRItid, "Removed %s", TID(), funcname);
         free(funcname);
      }
      assert(g_queue_get_length(traced_function_stack) == 0);
   }

   traced_function_stack_invalid = false;
   DBGF(debug, PRItid "Done", TID());
}


#ifdef UNUSED
bool suspend_traced_function_stack(bool onoff) {
   bool old = traced_function_stack_suspended;
   traced_function_stack_suspended = onoff;
   return old;
}
#endif


/** Reports the contents of the specified traced function stack.
 *
 *  @param stack    traced function stack to report
 *  @param reverse  order of entries
 *  @param show_tid prefix entries with thread id
 *  @param depth    logical indentation depth
 *
 *  @remark thread is redundant in certain contexts
 */
void dbgrpt_traced_function_stack(GQueue * stack, bool reverse, bool show_tid, int depth) {
   int d0 = depth;
   int d1 = d0+1;

   if (stack) {
      if (show_tid)
         drpt_vstring(d0, PRItid" Traced function stack %p:", TID(), stack);
      else
         drpt_vstring(d0, "Traced function stack %p:", stack);
      int queue_len = g_queue_get_length(stack);
      if (queue_len > 0) {
         // printf("%"PRItid"traced function stack (addr=%p, len=%d:\n", TID(), stack, queue_len );
         if (reverse) {
            for (int ndx =  g_queue_get_length(stack)-1; ndx >=0 ; ndx--) {
               drpt_vstring(d1, "%2d: %s", ndx, (char*) g_queue_peek_nth(stack, ndx));
            }
         }
         else {
            for (int ndx = 0; ndx < g_queue_get_length(stack); ndx++) {
               drpt_vstring(d1, "%2d: %s", ndx, (char*) g_queue_peek_nth(stack, ndx));
            }
         }
      }
      else {
         drpt_label(d1, "EMPTY");
      }
   }
   else {
      drpt_vstring(d0, PRItid"Current thread has no traced function stack.", TID());
   }
}


void collect_traced_function_stack(GPtrArray* collector,
                                   GQueue*    stack,
                                   bool       reverse,
                                   int        stack_adjust)
{
   bool debug = false;
   if (debug)
      dbgrpt_traced_function_stack(stack, false, true, 0);

   if (stack && collector) {
      DBGF(debug, PRItid" reverse=%s, stack_adjust=%d, Traced function stack %p:",
            TID(), sbool(reverse), stack_adjust, stack);
      int full_len = g_queue_get_length(stack);
      int adjusted_len = full_len - stack_adjust;
      if (adjusted_len > 0) {
         DBGF(debug, PRItid"traced function stack (addr=%p, adjusted_len=%d:", TID(), stack, adjusted_len );
         if (reverse) {
            for (int ndx =  full_len-1; ndx >=stack_adjust ; ndx--) {
               g_ptr_array_add(collector, strdup(g_queue_peek_nth(stack, ndx)));
            }
         }
         else {
            for (int ndx = stack_adjust; ndx < full_len; ndx++) {
               g_ptr_array_add(collector, strdup(g_queue_peek_nth(stack, ndx)));
            }
         }
      }
   }
}


void traced_function_stack_to_syslog(GQueue* callstack, int syslog_priority, bool reverse) {
   if (!callstack) {
      syslog(LOG_PERROR|LOG_ERR, "traced_function_stack unavailable");
   }
   else {
      GPtrArray * collector = g_ptr_array_new_with_free_func(g_free);
      collect_traced_function_stack(collector, callstack, reverse, 0);
      // syslog(syslog_priority, "Traced function stack %p:", callstack);

      if (collector->len == 0)
         syslog(syslog_priority, "   EMPTY");
      else {
         for (int ndx = 0; ndx < collector->len; ndx++) {
            syslog(syslog_priority, "   %s", (char *) g_ptr_array_index(collector, ndx));
         }
      }
      g_ptr_array_free(collector, true);
   }
}


void current_traced_function_stack_to_syslog(int syslog_priority, bool reverse) {
   bool debug = false;
   if (debug)
      list_traced_function_stacks();
   if (!traced_function_stack)
      syslog(LOG_PERROR|LOG_ERR, "No traced function stack for current thread");
   else {
      syslog(syslog_priority, "Traced function stack %p for current thread "PRItid, traced_function_stack, TID());
      traced_function_stack_to_syslog(traced_function_stack, syslog_priority, reverse);
   }
}


/** Reports the contents of the specified traced function stack for the
 *  current thread using report_util functions for writing debug information.
 *
 *  @param reverse  order of entries
 *  @param show_tid prefix entries with thread id
 *  @param depth    logical indentation depth
 *
 *  @remark thread is redundant in certain contexts
 */
void dbgrpt_current_traced_function_stack(bool reverse, bool show_tid, int depth) {
   bool debug = false;
   if (debug)
      list_traced_function_stacks();
   if (traced_function_stack) {
      dbgrpt_traced_function_stack(traced_function_stack, reverse, show_tid, depth);
   }
   else {
      if (show_tid)
         drpt_vstring(depth, PRItid" no traced function stack", TID());
      else
         drpt_vstring(depth, "no traced function stack");
   }
}


/** Returns the number of entries in the traced function stack for the
 *  current thread.
 *
 *  @return number of entries, 0 if stack does not exist.
 */
int current_traced_function_stack_size() {
   int qsize = (traced_function_stack) ? g_queue_get_length(traced_function_stack) : 0;
   return qsize;
}


/** Returns the contents of the traced function stack for the current thread
 *  as a GPtrArray of function names.
 *
 *  @param  most_recent_last  specifies order
 *  @return GPtrArray containing copies of the function names on the stack.
 */
GPtrArray * get_current_traced_function_stack_contents(bool most_recent_last) {
   GPtrArray* callstack = g_ptr_array_new_with_free_func(g_free);
   int qsize = g_queue_get_length(traced_function_stack);
   if (most_recent_last) {
      for (int ndx = 0; ndx < qsize; ndx++) {
         g_ptr_array_add(callstack, strdup((char*) g_queue_peek_nth(traced_function_stack, ndx)));
      }
   }
   else {
      for (int ndx = qsize-1; ndx >= 0; ndx--) {
          g_ptr_array_add(callstack, strdup((char*) g_queue_peek_nth(traced_function_stack, ndx)));
       }
   }
   return callstack;
}


GPtrArray * stash_current_traced_function_stack() {
   bool debug = true;
   if (debug) {
      drpt_label(0, "Starting. Traced function stack to be stashed:");
      dbgrpt_current_traced_function_stack(true, true, 0);
   }
   GPtrArray * result = get_current_traced_function_stack_contents(true);
   g_ptr_array_set_free_func(result, free);
   DBGF(debug, "Done.  Returning %p", result);
   return result;
}

void restore_current_traced_function_stack(GPtrArray* stashed) {
   bool debug = true;
   DBGF(debug, "Starting. Restoring stashed stack %p", stashed);
   reset_current_traced_function_stack();
   if (stashed) {
      for (int ndx = 0; ndx < stashed->len; ndx++) {
         push_traced_function(g_ptr_array_index(stashed, ndx));
      }
      g_ptr_array_free(stashed, true);
   }
   if (debug) {
      drpt_label(0, "Done.    Restored traced function stack:");
      dbgrpt_current_traced_function_stack(true, true, 0);
   }
}


typedef struct {
   GQueue * traced_function_stack;
   pid_t    thread_id;
   char *   initial_function;
} All_Traced_Function_Stacks_Entry;


void free_traced_function_stacks_entry(All_Traced_Function_Stacks_Entry* entry) {
   free(entry->initial_function);
   free(entry);
}


/** Lists all the traced function stacks. */
static void list_traced_function_stacks() {
   g_mutex_lock(&all_traced_function_stacks_mutex);
   if (!all_traced_function_stacks) {
      printf("No traced function stacks found.\n");
   }
   else {
      printf("Traced function stacks:\n");
      for (int ndx = 0; ndx < all_traced_function_stacks->len; ndx++) {
         All_Traced_Function_Stacks_Entry* entry = g_ptr_array_index(all_traced_function_stacks, ndx);
         printf("   thread: "PRItid"  stack: %p   initial function: %s\n",
               (intmax_t)entry->thread_id, entry->traced_function_stack, entry->initial_function);
      }
   }
   g_mutex_unlock(&all_traced_function_stacks_mutex);
}


/** Creates a traced function stack on the current thread
 *  and adds it to the list of traced function stacks
 *  on all threads.
 */
static GQueue * new_traced_function_stack(const char * funcname) {
   bool debug = false;
   debug = debug || debug_tfs;
   if (debug) {
      printf(PRItid"(%s) Starting. initial function: %s\n", TID(), __func__, funcname);
      list_traced_function_stacks();
   }

   GQueue* result = g_queue_new();
   g_mutex_lock(&all_traced_function_stacks_mutex);
   if (!all_traced_function_stacks)
      all_traced_function_stacks = g_ptr_array_new_with_free_func(
            (GDestroyNotify) free_traced_function_stacks_entry);
   All_Traced_Function_Stacks_Entry * entry = calloc(1, sizeof(All_Traced_Function_Stacks_Entry));
   entry->traced_function_stack = result;
   entry->thread_id = tid();
   entry->initial_function = strdup(funcname);
   // printf("entry=%p\n", entry);
   g_ptr_array_add(all_traced_function_stacks, entry);
   g_mutex_unlock(&all_traced_function_stacks_mutex);

   if (debug)
      printf(PRItid"(%s) Done.    Returning %p\n", TID(), __func__, result);
   return result;
}


/** Pushes a copy of a function name onto the traced function stack for the
 *  current thread.
 *
 *  @param funcname  function name
 */
void push_traced_function(const char * funcname) {
   // printf("(%s) debug_tfs = %s\n", __func__, sbool(debug_tfs));
   bool debug = false;
   debug = debug || debug_tfs;
   if (debug) {
      printf(PRItid"(%s) funcname = %s, "
            "traced_function_stack_enabled=%s, traced_function_stack_suspended=%s\n",
            TID(), __func__, funcname,
            sbool(traced_function_stack_enabled), sbool(traced_function_stack_suspended));
      syslog(LOG_DEBUG, PRItid"(%s) funcname = %s, "
            "traced_function_stack_enabled=%s, traced_function_stack_suspended=%s\n",
            TID(), __func__, funcname,
            sbool(traced_function_stack_enabled), sbool(traced_function_stack_suspended));
   }

   if (traced_function_stack_enabled && !traced_function_stack_suspended) {
      if (!traced_function_stack) {
         traced_function_stack = new_traced_function_stack(funcname);
         if (debug)
            printf(PRItid"%s) allocated new traced_function_stack %p, starting with %s\n",
                  TID(), __func__, traced_function_stack, funcname);
      }
      g_queue_push_head(traced_function_stack, g_strdup(funcname));
   }
   else {
      if (debug)
         fprintf(stderr, "traced_function_stack is disabled\n");
   }

   //
   if (debug) {
      printf(PRItid" (%s) Done\n", TID(), __func__);
      // show_backtrace(0);
      dbgrpt_current_traced_function_stack(false, true, 0);
   }
}


/** Returns the function name on the top of the stack for the current thread.
 *
 *  @return function name, null if the stack is empty. Caller should not free.
 */
char * peek_traced_function() {
   bool debug = false;
   debug = debug || debug_tfs;
   if (debug)
      printf(PRItid"(%s) Starting.\n", TID(), __func__);

   char * result = NULL;
   if (traced_function_stack && !traced_function_stack_invalid) {
      result = g_queue_peek_head(traced_function_stack);   // or g_strdup() ???
   }

   if (debug)
      printf(PRItid"(%s), returning %s\n", TID(), __func__, result);
   return result;
}


static
void tfs_error_msg(char * format, ...) {
   char msgbuf[300];
   va_list(args);
   va_start(args, format);
   g_vsnprintf(msgbuf, 300, format, args);
   va_end(args);
   fprintf(stderr, "%s\n", msgbuf);
   syslog(LOG_ERR, "%s", msgbuf);
}


/** Removes the function name on the top of the stack.
 *
 *  If the popped function name does not match the expected name,
 *  the traced function stack is corrupt.  In that case, diagnostics
 *  are written to both the terminal and the system log.  If flag
 *  #traced_function_stack_errors_fatal is set, program execution
 *  terminates with an assert() failure.  If not, thread global
 *  variable #traced_function_stack_invalid is set, marking the
 *  stack unusable until the stack is emptied.
 *
 *  @param funcname expected function name
 */
void pop_traced_function(const char * funcname) {
   bool debug = false;
   debug = debug || debug_tfs;

   if (debug) {
      printf(PRItid"(%s) expected = %s, "
            "traced_function_stack_enabled=%s, traced_function_stack_suspended=%s\n",
            TID(), __func__, funcname,
            sbool(traced_function_stack_enabled), sbool(traced_function_stack_suspended));
      syslog(LOG_DEBUG, PRItid"(%s) expected = %s, "
            "traced_function_stack_enabled=%s, traced_function_stack_suspended=%s\n",
            TID(), __func__, funcname,
            sbool(traced_function_stack_enabled), sbool(traced_function_stack_suspended));
   }

   if (traced_function_stack_enabled && !traced_function_stack_suspended && !traced_function_stack_invalid) {
      if (!traced_function_stack) {
         fprintf(stderr, PRItid"(%s) funcname=%s. No traced function stack\n",
               TID(), __func__, funcname);
         list_traced_function_stacks();
      }
      else {
         char * popped_func = g_queue_pop_head(traced_function_stack);
         if (!popped_func) {
            tfs_error_msg(PRItid" traced_function_stack=%p, expected %s, traced_function_stack is empty",
                      TID(), traced_function_stack, funcname);

            tfs_error_msg(PRItid" Function %s likely did not call push_traced_function() at start",
                      TID(), funcname);
            // show_backtrace(1);
            backtrace_to_syslog(1,true);
            traced_function_stack_invalid = true;
            if (traced_function_stack_errors_fatal)
               ASSERT_WITH_BACKTRACE(0);
         }
         else {
            if (!streq(popped_func, funcname)) {
               tfs_error_msg(PRItid" traced_function_stack=%p, !!! popped traced function %s, expected %s",
                         TID(), traced_function_stack,  popped_func, funcname);

               char * look_ahead = peek_traced_function();
               if (streq(look_ahead, funcname)) {
                  tfs_error_msg(PRItid" Function %s does not call pop_traced_function() at end",
                            TID(), funcname);

               }
               else {
                  tfs_error_msg(PRItid" Function %s likely did not call push_traced_function() at start",
                        TID(), funcname);
               }

               dbgrpt_current_traced_function_stack(/*reverse=*/ false, true, 0);
               // show_backtrace(1);
               backtrace_to_syslog(LOG_ERR, /* stack_adjust */ 1);
               current_traced_function_stack_to_syslog(LOG_ERR, /*reverse*/ false);
               traced_function_stack_invalid = true;
               if (traced_function_stack_errors_fatal)
                  ASSERT_WITH_BACKTRACE(0);
            }
            else {
               if (debug) {
                  fprintf(stdout, PRItid"(%s) Popped %s\n", TID(), __func__, popped_func);
                  syslog(LOG_DEBUG, PRItid"(%s) Popped %s", TID(), __func__, popped_func);
               }
            }
            free(popped_func);
         }
      }
   }
}


/** Frees the specified traced function stack
 *
 *  @param  stack  pointer to stack
 *
 *  @remark
 *  Must be called with #all_traced_function_stacks_mutex locked.
 */
static void free_traced_function_stack(GQueue * stack) {
   bool debug = false;
   if (debug) {
      printf(PRItid"(%s) Starting. stack=%p\n", TID(), __func__, traced_function_stack);
      if (stack) {
         printf(PRItid"(free_traced_function_stack) Final contents of traced_function_stack:\n", TID());
         dbgrpt_traced_function_stack(stack, true, true, 0);
      }
   }

   if (stack) {
      g_queue_free_full(stack, g_free);
      g_ptr_array_remove(all_traced_function_stacks, stack);
   }

   if (debug)
      printf(PRItid"(%s) Done.\n", TID(), __func__);
}


/** Frees the traced function stack on the current thread.
 *
 * Must be called WITHOUT all_traced_function_stacks_mutex locked
 */
void free_current_traced_function_stack() {
   bool debug = false;
   debug = debug || debug_tfs;
   if (traced_function_stack) {
      if (debug) {
         printf(PRItid"(free_current_traced_function_stack) traced_function_stack=%p. Executing.\n",
               TID(), traced_function_stack);
      }
      g_mutex_lock(&all_traced_function_stacks_mutex);
      free_traced_function_stack(traced_function_stack);
      g_mutex_unlock(&all_traced_function_stacks_mutex);
   }
}


/** Frees all traced function stacks.
 *
 *  Must be called without #all_traced_function_stacks_mutex locked.
 */
void free_all_traced_function_stacks() {
   bool debug =  true;
   if (debug) {
      printf(PRItid"(%s) Starting.\n", TID(), __func__);
   }

   g_mutex_lock(&all_traced_function_stacks_mutex);
   // g_queue_set_free_func(all_traced_function_stacks, g_free);
   if (all_traced_function_stacks) {
      printf("Found %d traced function stack(s)\n", all_traced_function_stacks->len);
      for (int ndx = all_traced_function_stacks->len-1; ndx >= 0; ndx--) {
         All_Traced_Function_Stacks_Entry * entry = g_ptr_array_index(all_traced_function_stacks, ndx);
         // printf("entry=%p\n", entry);
         if (debug)
            printf("Freeing traced function stack for thread %d\n", entry->thread_id);
         free_traced_function_stack(entry->traced_function_stack);
         g_ptr_array_remove_index(all_traced_function_stacks, ndx);
         free(entry);
      }
      g_ptr_array_free(all_traced_function_stacks, true);
      all_traced_function_stacks = NULL;
   }
   else {
      printf(PRItid"(%s) traced_function_stacks not set\n", TID(), __func__);
   }
   g_mutex_unlock(&all_traced_function_stacks_mutex);

   if (debug)
      printf(PRItid"(%s) Done.\n", TID(), __func__);
};


#ifdef INCLUDE_ONLY_IF_NEEDED_FOR_DEBUGGING
/** UNSAFE - FOR DEBUGGING USE ONLY */
void dbgrpt_all_traced_function_stacks() {
   bool debug = true;
   if (all_traced_function_stacks) {
      for (int ndx = all_traced_function_stacks->len-1; ndx >= 0; ndx--) {
          All_Traced_Function_Stacks_Entry * entry = g_ptr_array_index(all_traced_function_stacks, ndx);
          // printf("entry=%p\n", entry);
          if (debug)
             printf("Reporting traced function stack %p for thread %d\n",
                       entry->traced_function_stack, entry->thread_id);
          dbgrpt_traced_function_stack(entry->traced_function_stack, false);
       }
   }
   else {
       printf(PRItid"(%s) traced_function_stacks not set\n", TID(),__func__);
   }
}
#endif

