/** @file trace_control.c
 *
 *  Manage whether tracing is performed.
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "config.h"

#include "public/ddcutil_types.h"

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/glib_util.h"
#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "rtti.h"

#include "trace_control.h"

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
      VNT(DDCA_TRC_CONN,  "CONN"),
      VNT(DDCA_TRC_SYSFS, "SYSFS"),
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

DDCA_Trace_Group trace_levels = DDCA_TRC_NONE;   // 0x00

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


void dbgrpt_traced_object_table(GPtrArray * table, const char * table_name, int depth) {
   if (table) {
      rpt_vstring(depth, "%s:", table_name);
      if (table->len == 0)
         rpt_vstring(depth, "%s: empty", table_name);
      else {
         for (int ndx = 0; ndx < table->len; ndx++)
            rpt_vstring(depth+1, g_ptr_array_index(table, ndx));
      }
   }
   else {
      rpt_vstring(depth, "%s: NULL", table_name);
   }
}


void dbgrpt_traced_function_table(int depth) {
   dbgrpt_traced_object_table(traced_function_table, "traced_function_table", depth);
}


void dbgrpt_traced_callstack_call_table(int depth) {
   dbgrpt_traced_object_table(traced_callstack_call_table, "traced_callstack_call_table", depth);
}



/** Adds a function to the list of functions to be traced.
 *
 *  @param  funcname function name
 *  @return true  if funcname has been registered as a traceable function,
 *          false if not
 *
 *  @emark
 *  If the **traced_function_table** does not already exist, it is created.
 */
bool add_traced_function(const char * funcname) {
   bool debug = false;
   if (debug) {
      DBG("Starting. funcname=|%s|", funcname);
      //  report_rtti_func_name_table(2, "current function name table:");
   }

   bool result = false;
   bool missing = false;
   if (rtti_get_func_addr_by_name(funcname)) {  // if it's a traceable function
      DBGF(debug, "%s is a tracable function", funcname);
      if (!traced_function_table)
         traced_function_table = g_ptr_array_new();
      // n. g_ptr_array_find_with_equal_func() requires glib 2.54
      missing = (gaux_string_ptr_array_find(traced_function_table, funcname) < 0);
      if (missing)
         g_ptr_array_add(traced_function_table, g_strdup(funcname));
      result = true;
   }

   DBGF(debug, "Done. funcname=|%s|, missing=%s, returning: %s", funcname,
               sbool(missing), sbool(result));
   return result;
}


/** Adds an API function name to the list of API calls to be traced.
 *
 *  @param  funcname function name
 *  @return true if the API function name has been registered as a traceable function,
 *          false if not
 *
 *  @remark
 *  If the **traced_api_call_table** does not already exist, it is created.
 */
bool add_traced_api_call(const char * funcname) {
   bool debug = false;
   DBGF(debug, "Starting. funcname=|%s|", funcname);

   bool result = false;
   bool missing = false;
   if (rtti_get_func_addr_by_name(funcname)) {
      if (!traced_api_call_table)
         traced_api_call_table = g_ptr_array_new();
      // n. g_ptr_array_find_with_equal_func() requires glib 2.54
      bool missing = (gaux_string_ptr_array_find(traced_api_call_table, funcname) < 0);
      if (missing)
         g_ptr_array_add(traced_api_call_table, g_strdup(funcname));
      result = true;
   }

   DBGF(debug, "Done. funcname=|%s|, missing=%s, returning: %s",
               funcname, sbool(missing), sbool(result));
   return result;
}


/** Adds a function to the traced callstack call list.
 *
 *  @param  funcname function name
 *  @return true  if funcname has been registered as a traceable function,
 *          false if not
 *
 *  @emark
 *  If the **traced_callstack_call_table** does not already exist, it is created.
 */
bool add_traced_callstack_call(const char * funcname) {
   bool debug = false;
   DBGF(debug, "Starting. funcname=|%s|", funcname);

   bool result = false;
   bool missing = false;
   if (rtti_get_func_addr_by_name(funcname)) {
      if (!traced_callstack_call_table)
         traced_callstack_call_table = g_ptr_array_new();
      // n. g_ptr_array_find_with_equal_func() requires glib 2.54
      missing = (gaux_string_ptr_array_find(traced_callstack_call_table, funcname) < 0);
      if (missing)
         g_ptr_array_add(traced_callstack_call_table, g_strdup(funcname));
      result = true;
   }

   DBGF(debug, "Done. funcname=|%s|, missing=%s, returning: %s",
               funcname, sbool(missing), sbool(result));
   return result;
}


/** Adds a file to the list of files to be traced.
 *
 *  @param filename file name
 *
 *  @remark
 *  Only the basename portion of the specified file name is used.
 *  @remark
 *  If the file name does not end in ".c", that suffix is appended.
 *  @remark
 *  The file name is not checked for validity.
 */
void add_traced_file(const char * filename) {
   bool debug = false;
   DBGF(debug, "Starting. filename = |%s|", __func__, filename);

   if (!traced_file_table)
      traced_file_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54

   bool missing = false;
   gchar * bname = NULL;
   if (filename) {
      bname = g_path_get_basename(filename);
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
   }

   DBGF(debug, "Done. filename=|%s|, bname=|%s|, missing=%s", filename, bname, SBOOL(missing));
}


// n.b. caller must free result
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
   bool debug = false;

   bool result = (traced_function_table &&
                  gaux_string_ptr_array_find(traced_function_table, funcname) >= 0);

   DBGF(debug, "funcname=|%s|, returning: %s", funcname, SBOOL(result));
   return result;
}


/** Checks if an API call function is being traced.
 *
 *  @param funcname function name
 *  @return **true** if the function is being traced, **false** if not
 */
bool is_traced_api_call(const char * funcname) {
   bool debug = false;
   if (debug) {
      DBG("Starting. funcname = %s", funcname);
      char * buf = get_gptrarray_as_joined_string(traced_api_call_table, true);
      DBG("traced_api_calls: %s", buf);
      free(buf);
   }

   bool result = (traced_api_call_table &&
                  gaux_string_ptr_array_find(traced_api_call_table, funcname) >= 0);

   DBGF(debug, "funcname=|%s|, returning: %s\n", funcname, SBOOL(result));
   return result;
}


/** Checks if a function is in #traced_callstack_call_table.
 *
 *  @param funcname function name
 *  @return **true** if the function is in the table, **false** if not
 */
bool is_traced_callstack_call(const char * funcname) {
   bool debug = false;
   if (debug) {
      DBG("Starting. funcname = %s", funcname);
      char * buf = get_gptrarray_as_joined_string(traced_callstack_call_table, true);
      DBG("traced_callstack_calls: %s", buf );
      free(buf);
   }

   bool result = (traced_callstack_call_table &&
                  gaux_string_ptr_array_find(traced_callstack_call_table, funcname) >= 0);

   DBGF(debug, "funcname=|%s|, returning: %s", __func__, funcname, SBOOL(result));
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

