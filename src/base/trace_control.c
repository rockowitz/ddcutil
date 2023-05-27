/** @file trace_control.c
 *
 *  Manage whether tracing is performed.
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "config.h"

#include "public/ddcutil_types.h"

#include "util/data_structures.h"
#include "util/glib_util.h"
#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "rtti.h"

#include "trace_control.h"

_Thread_local  int  trace_api_call_depth = 0;
_Thread_local  int  trace_callstack_call_depth = 0;

DDCA_Syslog_Level syslog_level = DDCA_SYSLOG_NOT_SET;

bool enable_syslog = true;

Value_Name_Title_Table syslog_level_table = {
      VNT(DDCA_SYSLOG_DEBUG,   "DEBUG"),
      VNT(DDCA_SYSLOG_VERBOSE, "VERBOSE"),
      VNT(DDCA_SYSLOG_INFO,    "INFO"),
      VNT(DDCA_SYSLOG_NOTICE,  "NOTICE"),
      VNT(DDCA_SYSLOG_WARNING, "WARN"),
      VNT(DDCA_SYSLOG_ERROR,   "ERROR"),
      VNT(DDCA_SYSLOG_NEVER,   "NEVER"),
      VNT_END
};
const int syslog_level_ct = (ARRAY_SIZE(syslog_level_table)-1);


const char * syslog_level_name(DDCA_Syslog_Level level) {
   char * result = "DDCA_SYSLOG_NOT_SET";
   if (level != DDCA_SYSLOG_NOT_SET)
      result = vnt_name(syslog_level_table, level);
   return result;
}


DDCA_Syslog_Level
syslog_level_name_to_value(const char * name) {
   return (DDCA_Syslog_Level) vnt_find_id(syslog_level_table,
                                          name,
                                          true,      // search title field
                                          true,      // ignore-case
                                          DDCA_SYSLOG_NOT_SET);
}


/** Given a message severity level, test whether it should be
 *  written to the system log.
 *
 *  @param  msg_level  severity of message
 *  @return true if msg should be written to system log, false if not
 */
bool test_emit_syslog(DDCA_Syslog_Level msg_level) {
   bool result =  (syslog_level != DDCA_SYSLOG_NOT_SET &&
         msg_level <= syslog_level);
   return result;
}


/** Given a ddcutil severity level for messages written to the system log,
 *  returns the syslog priority level to be used in a syslog() call.
 *
 *  @param  level ddcutil severity level
 *  @return priority for syslog() call,
 *          -1 for msg that should never be output
 */
int syslog_importance_from_ddcutil_syslog_level(DDCA_Syslog_Level level) {
   int priority = -1;
   switch(level) {
   case DDCA_SYSLOG_NOT_SET: priority = -1;           break;
   case DDCA_SYSLOG_NEVER:   priority = -1;           break;
   case DDCA_SYSLOG_ERROR:   priority = LOG_ERR;      break;  // 3
   case DDCA_SYSLOG_WARNING: priority = LOG_WARNING;  break;  // 4
   case DDCA_SYSLOG_NOTICE:  priority = LOG_NOTICE;   break;  // 5
   case DDCA_SYSLOG_INFO:    priority = LOG_INFO;     break;  // 6
   case DDCA_SYSLOG_VERBOSE: priority = LOG_INFO;     break;  // 6
   case DDCA_SYSLOG_DEBUG:   priority = LOG_DEBUG;    break;  // 7
   }
   return priority;
}


static
Value_Name_Title_Table trace_group_table = {
      VNT(DDCA_TRC_BASE,  "BASE"),
      VNT(DDCA_TRC_I2C,   "I2C"),
      VNT(DDCA_TRC_DDC,   "DDC"),
      VNT(DDCA_TRC_USB,   "USB"),
      VNT(DDCA_TRC_TOP,   "TOP"),
      VNT(DDCA_TRC_ENV,   "ENV"),
      VNT(DDCA_TRC_API,   "API"),
      VNT(DDCA_TRC_UDF,   "UDF"),
      VNT(DDCA_TRC_VCP,   "VCP"),
      VNT(DDCA_TRC_DDCIO, "DDCIO"),
      VNT(DDCA_TRC_SLEEP, "SLEEP"),
      VNT(DDCA_TRC_RETRY, "RETRY"),
      VNT_END
};
const int trace_group_ct = (ARRAY_SIZE(trace_group_table)-1);


/** Given a trace group name, returns its identifier.
 *  Case is ignored.
 *
 *  @param name trace group name
 *  @return trace group identifier
 *  @retval  TRC_NEVER unrecognized name
 *
 *  /ingroup dbgtrace
 */
DDCA_Trace_Group trace_class_name_to_value(const char * name) {
   return (DDCA_Trace_Group) vnt_find_id(
                           trace_group_table,
                           name,
                           true,      // search title field
                           true,      // ignore-case
                           DDCA_TRC_NONE);
}

static DDCA_Trace_Group trace_levels = DDCA_TRC_NONE;   // 0x00

/** Replaces the groups to be traced.
 *
 * @param trace_flags bit flags indicating groups to trace
 *
 * @ingroup dbgtrace
 */
void set_trace_groups(DDCA_Trace_Group trace_flags) {
   bool debug = false;
   if (debug)
      printf("(%s) trace_flags=0x%04x\n", __func__, trace_flags);

   trace_levels = trace_flags;
}


/** Adds to the groups to be traced.
 *
 * @param trace_flags bit flags indicating groups to trace
 *
 * @ingroup dbgtrace
 */
void add_trace_groups(DDCA_Trace_Group trace_flags) {
   bool debug = false;
   if (debug)
      printf("(%s) trace_flags=0x%04x\n", __func__, trace_flags);

   trace_levels |= trace_flags;
}


// traced_function_table and traced_file_table were initially implemented using
// GHashTable.  The implementation had bugs, and given that (a) these data structures
// are used only for testing and (b) there will be at most a handful of entries in the
// tables, a simpler GPtrArray implementation is used.

static GPtrArray  * traced_function_table = NULL;
static GPtrArray  * traced_file_table     = NULL;
static GPtrArray  * traced_api_call_table = NULL;
static GPtrArray  * traced_callstack_call_table = NULL;

#ifdef UNUSED
void dbgrpt_traced_function_table(int depth) {
   if (traced_function_table) {
      rpt_vstring(depth, "traced_function_table:");
      if (traced_function_table) {
         for (int ndx = 0; ndx < traced_function_table->len; ndx++) {
            rpt_vstring(depth+1, g_ptr_array_index(traced_function_table, ndx));
         }
      }
   }
   else
      rpt_vstring(depth, "traced_function_table: NULL");
}
#endif


/** Adds a function to the list of functions to be traced.
 *  @param funcname function name
 */
bool add_traced_function(const char * funcname) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. funcname=|%s|\n", __func__, funcname);

   if (!rtti_get_func_addr_by_name(funcname))
      return false;

   if (!traced_function_table)
      traced_function_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54
   bool missing = (gaux_string_ptr_array_find(traced_function_table, funcname) < 0);
   if (missing)
      g_ptr_array_add(traced_function_table, g_strdup(funcname));

   if (debug)
      printf("(%s) Done. funcname=|%s|, missing=%s\n",
             __func__, funcname, SBOOL(missing));
   return true;
}


bool add_traced_api_call(const char * funcname) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. funcname=|%s|\n", __func__, funcname);

   if (!rtti_get_func_addr_by_name(funcname))
      return false;

   if (!traced_api_call_table)
      traced_api_call_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54
   bool missing = (gaux_string_ptr_array_find(traced_api_call_table, funcname) < 0);
   if (missing)
      g_ptr_array_add(traced_api_call_table, g_strdup(funcname));

   if (debug)
      printf("(%s) Done. funcname=|%s|, missing=%s\n",
             __func__, funcname, SBOOL(missing));
   return true;
}


bool add_traced_callstack_call(const char * funcname) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. funcname=|%s|\n", __func__, funcname);

   if (!rtti_get_func_addr_by_name(funcname))
      return false;

   if (!traced_callstack_call_table)
      traced_callstack_call_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54
   bool missing = (gaux_string_ptr_array_find(traced_callstack_call_table, funcname) < 0);
   if (missing)
      g_ptr_array_add(traced_callstack_call_table, g_strdup(funcname));

   if (debug)
      printf("(%s) Done. funcname=|%s|, missing=%s\n",
             __func__, funcname, SBOOL(missing));
   return true;
}



/** Adds a file to the list of files to be traced.
 *
 *  @param filename file name
 *
 *  @remark
 *  Only the basename portion of the specified file name is used.
 *  @remark
 *  If the file name does not end in ".c", that suffix is appended.
 */
void add_traced_file(const char * filename) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. filename = |%s| \n", __func__, filename);

   if (!traced_file_table)
      traced_file_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54

   gchar * bname = g_path_get_basename(filename);
   if (!str_ends_with(bname, ".c")) {
      int newsz = strlen(bname) + 2 + 1;
      gchar * temp = calloc(1, newsz);
      strcpy(temp, bname);
      strcat(temp, ".c");
      free(bname);
      bname = temp;
   }

   bool missing = (gaux_string_ptr_array_find(traced_file_table, bname) < 0);
   if (missing)
      g_ptr_array_add(traced_file_table, bname);
   else
      free(bname);
   if (debug)
      printf("(%s) Done. filename=|%s|, bname=|%s|, missing=%s\n",
             __func__, filename, bname, SBOOL(missing));
}


static char * get_gptrarray_as_joined_string(GPtrArray * arry, bool sort) {
   char * result = NULL;
   if (arry) {
      if (sort)
         g_ptr_array_sort(arry, gaux_ptr_scomp);
      result = join_string_g_ptr_array(arry, ", ");
   }
   return result;
}


#ifdef OLD
static char * get_traced_functions_as_joined_string() {
   char * result = NULL;
   if (traced_function_table) {
      g_ptr_array_sort(traced_function_table, gaux_ptr_scomp);
      result = join_string_g_ptr_array(traced_function_table, ", ");
   }
   return result;
}


static char * get_traced_api_calls_as_joined_string() {
   char * result = NULL;
   if (traced_api_call_table) {
      g_ptr_array_sort(traced_api_call_table, gaux_ptr_scomp);
      result = join_string_g_ptr_array(traced_api_call_table, ", ");
   }
   return result;
}


static char * get_traced_callstack_calls_as_joined_string() {
   char * result = NULL;
   if (traced_callstack_call_table) {
      g_ptr_array_sort(traced_callstack_call_table, gaux_ptr_scomp);
      result = join_string_g_ptr_array(traced_callstack_call_table, ", ");
   }
   return result;
}



static char * get_traced_files_as_joined_string() {
   char * result = NULL;
   if (traced_file_table) {
      g_ptr_array_sort(traced_file_table, gaux_ptr_scomp);
      result = join_string_g_ptr_array(traced_file_table, ", ");
   }
   return result;
}
#endif


/** Checks if a function is being traced.
 *
 *  @param funcname function name
 *  @return **true** if the function is being traced, **false** if not
 */
bool is_traced_function(const char * funcname) {
   bool result = (traced_function_table && gaux_string_ptr_array_find(traced_function_table, funcname) >= 0);
   // printf("(%s) funcname=|%s|, returning: %s\n", __func__, funcname, SBOOL(result2));
   return result;
}


bool is_traced_api_call(const char * funcname) {
   bool debug = false;
   if (debug) {
      printf("(%s) Starting. funcname = %s\n", __func__, funcname);
      printf("(%s) traced_api_calls: %s\n", __func__, get_gptrarray_as_joined_string(traced_api_call_table, true) );
   }

   bool result = (traced_api_call_table && gaux_string_ptr_array_find(traced_api_call_table, funcname) >= 0);

   if (debug)
      printf("(%s) funcname=|%s|, returning: %s\n", __func__, funcname, SBOOL(result));
   return result;
}


bool is_traced_callstack_call(const char * funcname) {
   bool debug = false;
   if (debug) {
      printf("(%s) Starting. funcname = %s\n", __func__, funcname);
      printf("(%s) traced_callstack_calls: %s\n", __func__, get_gptrarray_as_joined_string(traced_callstack_call_table, true) );
   }

   bool result = (traced_callstack_call_table && gaux_string_ptr_array_find(traced_callstack_call_table, funcname) >= 0);

   if (debug)
      printf("(%s) funcname=|%s|, returning: %s\n", __func__, funcname, SBOOL(result));
   return result;
}


/** Checks if a file is being traced.
 *
 *  @param filename file name
 *  @return **true** if trace is enabled for all functions in the file, **false** if not
 */
bool is_traced_file(const char * filename) {
   bool result = false;
   if (filename) {
      char * bname = g_path_get_basename(filename);
      result = (traced_file_table && gaux_string_ptr_array_find(traced_file_table, bname) >= 0);
      // printf("(%s) filename=|%s|, bname=|%s|, returning: %s\n", __func__, filename, bname, SBOOL(result));
      free(bname);
   }
   return result;
}

/** Checks if tracing is to be performed.
 *
 * Tracing is enabled if any of the following tests pass:
 * - trace group
 * - file name
 * - function name
 *
 * @param trace_group group to check
 * @param filename    file from which check is occurring
 * @param funcname    function name
 *
 * @return **true** if tracing enabled, **false** if not
 *
 * @remark
 * - Multiple trace group bits can be set in **trace_group**.  If any of those
 *   group are currently being traced, the function returns **true**. That is,
 *   a given trace location in the code can be activated by multiple trace groups.
 * - If trace_group == TRC_ALWAYS (0xff), the function returns **true**.
 *   Commonly, if debugging is active in a given location, the trace_group value
 *   being checked can be set to TRC_ALWAYS, so a site can have a single debug/trace
 *   function call.
 *
 * @ingroup dbgtrace
 *
 */
bool is_tracing(DDCA_Trace_Group trace_group, const char * filename, const char * funcname) {
   bool debug = false;  //str_starts_with(funcname, "ddca_");
   if (debug)
      printf("(%s) Starting. trace_group=0x%04x, filename=%s, funcname=%s\n",
              __func__, trace_group, filename, funcname);
   bool result = false;
// #ifdef ENABLE_TRACE
   result =  (trace_group == DDCA_TRC_ALL) || (trace_levels & trace_group); // is trace_group being traced?

   result = result || is_traced_function(funcname) || is_traced_file(filename) || trace_api_call_depth > 0;
// #endif
   if (debug)
      printf("(%s) Done.     trace_group=0x%04x, filename=%s, funcname=%s, trace_levels=0x%04x, returning %d\n",
              __func__, trace_group, filename, funcname, trace_levels, result);
   return result;
}


/** Reports the current trace settings.
 *
 *  \param depth  logical indentation depth
 */
void report_tracing(int depth) {
   int d1 = depth+1;
   rpt_label(depth, "Trace Options:");

#ifdef UNUSED
   show_trace_destination();
#endif

   char * buf = vnt_interpret_flags(trace_levels, trace_group_table, true /* use title */, ", ");
   rpt_vstring(d1, "Trace groups active:     %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   buf = get_gptrarray_as_joined_string(traced_function_table, true);
   rpt_vstring(d1, "Traced functions:        %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   buf = get_gptrarray_as_joined_string(traced_api_call_table, true);
   rpt_vstring(d1, "Traced API calls:        %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   buf = get_gptrarray_as_joined_string(traced_callstack_call_table, true);
   rpt_vstring(d1, "Traced call stack calls: %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   buf = get_gptrarray_as_joined_string(traced_file_table, true);
   rpt_vstring(d1, "Traced files:            %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

}

