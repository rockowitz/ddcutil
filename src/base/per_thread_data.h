/** \file per_thread_data.h
 *
 *  Maintains per-thread settings and statistics.
 *
 *  The dependencies between this file and thread_retry_data.c and thread_sleep.data
 *  are not unidirectional.  The functionality has been split into 3 files for clarity.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PER_THREAD_DATA_H_
#define PER_THREAD_DATA_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "base/parms.h"
#include "base/displays.h"

extern GHashTable *  per_thread_data_hash;   // key is thread id, value is Per_Thread_Data*

void init_per_thread_data();     // module initialization
void terminate_per_thread_data();  // release all resources

extern int  ptd_lock_count;
extern int  ptd_unlock_count;
extern int  cross_thread_operation_blocked_count;

typedef struct {
   char *    function;
   int       total_calls;
   uint64_t  total_nanosec;
} Per_Thread_Function_Stats;

// key is function name, value is Per_Thread_Fuction_Stats *
typedef GHashTable Function_Stats_Hash;

typedef struct {
   bool                  initialized;
   pid_t                 thread_id;
#ifdef REMOVED
   char *                description;
#endif
   Display_Handle *      cur_dh;
   char *                cur_func;
   uint64_t              cur_start;
   Function_Stats_Hash * function_stats;
   // double                sleep_multiplier;
} Per_Thread_Data;

bool ptd_cross_thread_operation_start();
void ptd_cross_thread_operation_end();
void ptd_cross_thread_operation_block();
void dbgrpt_per_thread_data_locks(int depth);

Per_Thread_Data * ptd_get_per_thread_data();

#ifdef REMOVED
void         ptd_set_thread_description(const char * description);
void         ptd_append_thread_description(const char * addl_description);
const char * ptd_get_thread_description_t();
#endif

// Apply a function to all Per_Thread_Data records
typedef void (*Ptd_Func)(Per_Thread_Data * data, void * arg);
void ptd_apply_all(Ptd_Func func, void * arg);
void ptd_apply_all_sorted(Ptd_Func func, void * arg);

void dbgrpt_per_thread_data(Per_Thread_Data * data, int depth);
void ptd_list_threads(int depth);

// API function performance profiling
extern bool ptd_api_profiling_enabled;
void ptd_profile_function_start(const char * func);
void ptd_profile_function_end(const char * func);
void ptd_profile_report_all_threads(int depth);
void ptd_profile_report_stats_summary(int depth);
void ptd_profile_reset_all_stats();

#endif /* PER_THREAD_DATA_H_ */

