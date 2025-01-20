/** @file traced_function_stack.c */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <syslog.h>

#include "common_inlines.h"
#include "backtrace.h"
#include "glib_util.h"

#include "traced_function_stack.h"

bool traced_function_stack_enabled = true;
bool __thread traced_function_stack_suspended = false;

static __thread GQueue * traced_function_stack;
static GPtrArray * all_traced_function_stacks = NULL;
static GMutex all_traced_function_stacks_mutex;

__thread pid_t process_id = 0;
__thread pid_t thread_id  = 0;

/** Reports the contents of the specified traced function stack.
 *
 *  @param stack    traced function stack to report
 *  @param reverse  order of entries
 */
void debug_traced_function_stack(GQueue * stack, bool reverse) {
   if (stack) {
      printf("[%d] Traced function stack %p:\n", tid(), stack);
      int queue_len = g_queue_get_length(stack);
      if (queue_len > 0) {
         printf("[%7d]traced function stack (addr=%p, len=%d:\n", tid(), stack, queue_len );
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


/** Reports the contents of the specified traced function stack for the
 *  current thread.
 *
 *  @param reverse  order of entries
 */
void debug_current_traced_function_stack(bool reverse) {
   if (traced_function_stack) {
      debug_traced_function_stack(traced_function_stack, reverse);
   }
   else {
      printf("[%d] no traced function stack\n", tid());
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
} All_Traced_Function_Stacks_Entry;


/** Creates a traced function stack on the current thread
 *  and adds it to the list of traced function stacks
 *  on all threads.
 */
GQueue * new_traced_function_stack() {
   bool debug = false;
   if (debug)
      printf("[%7d](%s) Starting.\n", tid(), __func__);

   GQueue* result = g_queue_new();
   g_mutex_lock(&all_traced_function_stacks_mutex);
   if (!all_traced_function_stacks)
      all_traced_function_stacks = g_ptr_array_new();
   All_Traced_Function_Stacks_Entry * entry = calloc(1, sizeof(All_Traced_Function_Stacks_Entry));
   entry->traced_function_stack = result;
   entry->thread_id = tid();
   // printf("entry=%p\n", entry);
   g_ptr_array_add(all_traced_function_stacks, entry);
   g_mutex_unlock(&all_traced_function_stacks_mutex);

   if (debug)
      printf("[%7d](%s) Done.    Returning %p\n", tid(), __func__, result);

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
      printf("[%d](push_traced_function) funcname = %s, traced_function_stack_enabled=%d\n",
            tid(), funcname, traced_function_stack_enabled);
      syslog(LOG_DEBUG, "[%d](push_traced_function) funcname = %s, traced_function_stack_enabled=%d\n",
            tid(), funcname, traced_function_stack_enabled);
   }

   if (traced_function_stack_enabled && !traced_function_stack_suspended) {
      if (!traced_function_stack) {
         traced_function_stack = new_traced_function_stack();
         if (debug)
            printf("(push_traced_function) tid=%d, allocated new traced_function_stack %p\n", tid(), traced_function_stack);
      }
      g_queue_push_head(traced_function_stack, g_strdup(funcname));
   }
   else {
      if (debug)
         fprintf(stderr, "traced_function_stack is disabled\n");
   }

   // debug_current_traced_function_stack(true);
}


/** Returns the function name on the top of the stack for the current thread.
 *
 *  @return function name, null if the stack is empty. Caller should not free.
 */
char * peek_traced_function() {
   char * result = NULL;
   if (traced_function_stack) {
      result = g_queue_peek_head(traced_function_stack);   // or g_strdup() ???
   }
   // printf("(peek_traced_function) tid=%d, returning %s\n", tid(), result);
   return result;
}


/** Removes the function name on the top of the stack and returns it to the
 *  caller.
 *
 *  @param funcname expected function name
 *  @return function name, or null if the stack is empty.
 *
 *  The caller is responsible for freeing the returned function name.
 */
void pop_traced_function(const char * funcname) {
   bool debug = false;

   if (!traced_function_stack) {
      fprintf(stderr, "[%7d](%s) funcname=%s. No traced function stack\n", tid(), __func__, funcname);
   }
   else {
      if (traced_function_stack_enabled && !traced_function_stack_suspended) {
         char * popped_func = g_queue_pop_head(traced_function_stack);
         if (!popped_func) {
            fprintf(stderr, "(%s) tid=%d, traced_function_stack=%p, expected %s, traced_function_stack is empty\n",
                  __func__, tid(), traced_function_stack, funcname);
            syslog(LOG_ERR, "(%s) tid=%d, traced_function_stack=%p, expected %s, traced_function_stack is empty\n",
                  __func__, tid(), traced_function_stack, funcname);
            backtrace_to_syslog(LOG_ERR, /* stack_adjust */ 1);
         }
         else {
            if (strcmp(popped_func, funcname) != 0) {
               fprintf(stderr, "[%d](%s)traced_function_stack=%p, !!! popped traced function %s, expected %s\n",
                     tid(), __func__, traced_function_stack,  popped_func, funcname);
               syslog(LOG_ERR, "[%d](%s)traced_function_stack=%p, !!! popped traced function %s, expected %s\n",
                     tid(), __func__, traced_function_stack,  popped_func, funcname);
               fprintf(stderr, "Current traced function stack:\n");
               debug_current_traced_function_stack(true);
            }
            else {
               if (debug) {
                  fprintf(stdout, "[%7d](%s) Popped %s\n", tid(), __func__, popped_func);
                  syslog(LOG_DEBUG, "[%d](%s) Popped %s", tid(), __func__, popped_func);
               }
            }
            free(popped_func);
         }
      }
   }
}


/** Frees the specified traced function stack.
 *
 *  @param  stack  pointer to stack
 *
 *  @remark
 *  Must be called with #all_traced_function_stacks_mutex locked.
 */
static void free_traced_function_stack(GQueue * stack) {
   bool debug = false;
   if (debug) {
      printf("[%d](%s) Starting. stack=%p\n", tid(), __func__, traced_function_stack);
      if (stack) {
         printf("[%d](free_traced_function_stack) Final contents of traced_function_stack:\n", tid());
         debug_traced_function_stack(stack, true);
      }
   }

   if (stack) {
      g_queue_free_full(stack, g_free);
      g_ptr_array_remove(all_traced_function_stacks, stack);
   }

   if (debug)
      printf("[%d](%s) Done.\n", tid(), __func__);
}


/** Frees the traced function stack on the current thread.
 *
 * Must be called WITHOUT all_traced_function_stacks_mutex locked
 */
void free_current_traced_function_stack() {
   bool debug = false;
   if (traced_function_stack) {
      if (debug) {
         printf("[%d](free_current_traced_function_stack) traced_function_stack=%p. Executing.\n",
               tid(), traced_function_stack);
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
   bool debug = true;
   if (debug)
      printf("[%7d](%s) Starting.\n", tid(), __func__);

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
      printf("[%7d](%s) traced_function_stacks not set\n", tid(), __func__);
   }
   g_mutex_unlock(&all_traced_function_stacks_mutex);

   if (debug)
      printf("[%7d](%s) Done.\n", tid(), __func__);
};


