/* @file rtti.c
 *
 * Runtime trace information
 */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/rtti.h"

static GHashTable * func_name_table = NULL;


void rtti_func_name_table_add(void * func_addr, const char * func_name) {
   if (!func_name_table)
      func_name_table =  g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
   g_hash_table_insert(func_name_table, func_addr, g_strdup(func_name));
}


char * rtti_get_func_name_by_addr(void * ptr) {
   char * result = "";
   if (func_name_table && ptr) {
      result = g_hash_table_lookup(func_name_table, ptr);
      if (!result)
         result = "<Not Found>";
   }
   return result;
}


void * rtti_get_func_addr_by_name(const char * name) {
   bool debug = false;
   DBGF(debug, "func_name_table=%p, name=|%s|", func_name_table, name);

   void * result = NULL;
   if (func_name_table) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, func_name_table);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         if (streq(name, (char *) value)) {
            result = key;
            break;
         }
      }
   }

   DBGF(debug, "name=%s, returning %s", name, SBOOL(result));
   return result;
}


void dbgrpt_rtti_func_name_table(int depth, bool show_internal) {
   if (show_internal) {
      rpt_vstring(depth, "Function name table at %p", func_name_table);
      depth=depth+1;
   }
   if (func_name_table) {
      GHashTableIter iter;
      gpointer key, value;
      GPtrArray * values = g_ptr_array_new();
      g_hash_table_iter_init(&iter, func_name_table);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         if (show_internal) {
            rpt_vstring(depth, "%p: %s", key, (char *) value);
         }
         g_ptr_array_add(values, value);
      }
      g_ptr_array_sort(values, gaux_ptr_scomp);
      for (int ndx = 0; ndx < values->len; ndx++) {
         rpt_vstring(depth, "   %s", (char *) g_ptr_array_index(values, ndx));
      }
      g_ptr_array_free(values, true);
   }
   else {
      if (!show_internal) {
         rpt_label(depth, "None");
      }
   }
}


void report_rtti_func_name_table(int depth, char * msg) {
   bool saved_prefix_report_output = rpt_set_ornamentation_enabled(false);

   if (msg) {
      rpt_label(depth, msg);
      depth++;
   }
   dbgrpt_rtti_func_name_table(depth, false);

   rpt_set_ornamentation_enabled(saved_prefix_report_output);
}



void terminate_rtti() {
   g_hash_table_destroy(func_name_table);
}

