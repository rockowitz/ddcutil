/* @file rtti.c
 * Primitive runtime type information facilities
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include <glib-2.0/glib.h>
 
#include "util/report_util.h"
#include "base/rtti.h"


static GHashTable * func_name_table = NULL;


void rtti_func_name_table_add(void * func_addr, const char * func_name) {
   if (!func_name_table)
      func_name_table =  g_hash_table_new(g_direct_hash, g_direct_equal);
   g_hash_table_insert(func_name_table, func_addr, strdup(func_name));
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


void dbgrpt_rtti_func_name_table(int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Function name table at %p", func_name_table);
   if (func_name_table) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, func_name_table);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         rpt_vstring(d1, "%p: %s", key, value);
      }
   }
}

