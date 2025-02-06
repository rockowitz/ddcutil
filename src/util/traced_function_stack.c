/** @file traced_function_stack.c */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
// #include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "common_inlines.h"
#include "backtrace.h"
#include "glib_util.h"
#include "string_util.h"
#include "syslog.h"

#include "common_printf_formats.h"
#include "debug_util.h"

#include "traced_function_stack.h"

bool traced_function_stack_enabled = false;
bool traced_function_stack_errors_fatal = false;
bool __thread traced_function_stack_suspended = false;

/*static*/ __thread GQueue * traced_function_stack;
static __thread bool  traced_function_stack_invalid = false;
static GPtrArray * all_traced_function_stacks = NULL;
static GMutex all_traced_function_stacks_mutex;

__thread pid_t process_id = 0;
__thread pid_t thread_id  = 0;

static void list_traced_function_stacks();


/** Delete all entries in the traced function stack for the current thread,
 *  and reset the traced_function_stack flag.
 */
void reset_current_traced_function_stack() {
   if (traced_function_stack) {
      int ct = g_queue_get_length(traced_function_stack);
      for (int ctr = 0; ctr<ct; ctr++) {
         char * funcname = g_queue_pop_tail(traced_function_stack);
         free(funcname);
      }
      assert(g_queue_get_length(traced_function_stack) == 0);
   }
   traced_function_stack_invalid = false;
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
 */
void debug_traced_function_stack(GQueue * stack, bool reverse) {
   if (stack) {
      printf(PRItid" Traced function stack %p:\n", TID(), stack);
      int queue_len = g_queue_get_length(stack);
      if (queue_len > 0) {
         // printf(PRItid"traced function stack (addr=%p, len=%d:\n", TID(), stack, queue_len );
         if (reverse) {
            for (int ndx =  g_queue_get_length(stack)-1; ndx >=0 ; ndx--) {
               printf("   %s\n", (char*) g_queue_peek_nth(stack, ndx));
            }
         }
         else {
            for (int ndx = 0; ndx < g_queue_get_length(stack); ndx++) {
               printf("   %s\n", (char*) g_queue_peek_nth(stack, ndx));
            }
         }
      }
      else {
         printf("    EMPTY\n");
      }
   }
}


void collect_traced_function_stack(GPtrArray* collector, GQueue * stack, bool reverse, int stack_adjust) {
   if (stack && collector) {
      printf(PRItid" Traced function stack %p:\n", TID(), stack);
      int queue_len = g_queue_get_length(stack) - stack_adjust;
      if (queue_len > 0) {
         // printf(PRItid"traced function stack (addr=%p, len=%d:\n", TID(), stack, queue_len );
         if (reverse) {
            for (int ndx =  g_queue_get_length(stack)-stack_adjust; ndx >=0 ; ndx--) {
               g_ptr_array_add(collector, strdup(g_queue_peek_nth(stack, ndx)));
            }
         }
         else {
            for (int ndx = 0; ndx < g_queue_get_length(stack); ndx++) {
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
      collect_traced_function_stack(collector, callstack, reverse, 2);  // was 2
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
   bool debug = true;
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
 *  current thread.
 *
 *  @param reverse  order of entries
 */
void debug_current_traced_function_stack(bool reverse) {
   bool debug = true;
   if (debug)
      list_traced_function_stacks();
   if (traced_function_stack) {
      debug_traced_function_stack(traced_function_stack, reverse);
   }
   else {
      printf(PRItid" no traced function stack\n", TID());
   }
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
   bool debug = true;
   if (debug) {
      printf(PRItid"(%s) Starting. initail function: %s\n", TID(), __func__, funcname);
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
   bool debug = false;
   if (debug) {
      printf(PRItid"(push_traced_function) funcname = %s, traced_function_stack_enabled=%d\n",
            TID(), funcname, traced_function_stack_enabled);
      syslog(LOG_DEBUG, PRItid"(push_traced_function) funcname = %s, traced_function_stack_enabled=%d\n",
            TID(), funcname, traced_function_stack_enabled);
   }

   if (traced_function_stack_enabled && !traced_function_stack_suspended) {
      if (!traced_function_stack) {
         traced_function_stack = new_traced_function_stack(funcname);
         if (debug)
            printf(PRItid"(push_traced_function) allocated new traced_function_stack %p, starting with %s\n",
                  TID(), traced_function_stack, funcname);
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
      debug_current_traced_function_stack(true);
   }
}


/** Returns the function name on the top of the stack for the current thread.
 *
 *  @return function name, null if the stack is empty. Caller should not free.
 */
char * peek_traced_function() {
   bool debug = false;
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
void error_msg(char * format, ...) {
   char msgbuf[300];
   va_list(args);
   va_start(args, format);
   g_vsnprintf(msgbuf, 300, format, args);
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

   if (traced_function_stack_enabled && !traced_function_stack_suspended && !traced_function_stack_invalid) {
      if (!traced_function_stack) {
         fprintf(stderr, PRItid"(%s) funcname=%s. No traced function stack\n",
               TID(), __func__, funcname);
         list_traced_function_stacks();
      }
      else {
         char * popped_func = g_queue_pop_head(traced_function_stack);
         if (!popped_func) {
            error_msg(PRItid" traced_function_stack=%p, expected %s, traced_function_stack is empty",
                      TID(), traced_function_stack, funcname);

            error_msg(PRItid" Function %s likely did not call push_traced_function() at start",
                      TID(), funcname);
            show_backtrace(0);
            backtrace_to_syslog(1,true);
            traced_function_stack_invalid = true;
            if (traced_function_stack_errors_fatal)
               ASSERT_WITH_BACKTRACE(0);
         }
         else {
            if (!streq(popped_func, funcname)) {
               error_msg(PRItid" traced_function_stack=%p, !!! popped traced function %s, expected %s",
                         TID(), traced_function_stack,  popped_func, funcname);

               char * look_ahead = peek_traced_function();
               if (streq(look_ahead, funcname)) {
                  error_msg(PRItid" Function %s does not call pop_traced_function() at end",
                            TID(), funcname);

               }
               else {
                  error_msg(PRItid" Function %s likely did not call push_traced_function() at start",
                        TID(), funcname);
               }

               debug_current_traced_function_stack(/*reverse=*/ false);
               show_backtrace(1);
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
   bool debug = true;
   if (debug) {
      printf(PRItid"(%s) Starting. stack=%p\n", TID(), __func__, traced_function_stack);
      if (stack) {
         printf(PRItid"(free_traced_function_stack) Final contents of traced_function_stack:\n", TID());
         debug_traced_function_stack(stack, true);
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
   bool debug = true;
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
          debug_traced_function_stack(entry->traced_function_stack, false);
       }
   }
   else {
       printf(PRItid"(%s) traced_function_stacks not set\n", TID(),__func__);
   }
}
#endif

