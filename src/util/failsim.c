/* failsim.c
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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


#include <stdbool.h>

#include "util/file_util.h"

#include "util/failsim.h"


static GHashTable * fst = NULL;


typedef enum {FSIM_CALLOCC_RECURRING, FSIM_CALLOCC_SINGLE} Fsim_Callocc_Type;


typedef struct fsim_callocc_rec {
   Fsim_Callocc_Type    calloc_type;
   int                  occno;
   int                  rc;
} Fsim_Callocc_Rec;


typedef struct fsim_func_rec {
   char *   func_name;
   int      callct;
   GArray * callocc_recs;
} Fsim_Fun_Rec;



void fsim_insert_calloc_rec(Fsim_Callocc_Rec * fsrec) {




}


// cf load dumpload load variants

bool fsim_load_control_from_gptrarray(GPtrArray * lines) {
   bool ok = true;
   fst = g_hash_table_new(g_str_hash, g_str_equal);

   return ok;
}

bool fsim_load_control_string(char * s) {
   bool ok = false;

   return ok;
}

bool fsim_load_control_file(char * fn) {
   bool verbose = true;   // should this be argument?
   GPtrArray * lines = g_ptr_array_new();
   int linect = file_getlines(fn,  lines, verbose);
   bool result = false;
   if (linect > 0)
      result = fsim_load_control_from_gptrarray(lines);
   // need free func
   g_ptr_array_free(lines);
   return result;
}

int fsim_check_failure(char * fn, char * funcname) {
   bool debug = true;
   int result = 0;
   if (fst) {
      if (g_hash_table_contains(fst, funcname)) {
         result = 1;   // TEMP
      }
   }
   return result;
}
