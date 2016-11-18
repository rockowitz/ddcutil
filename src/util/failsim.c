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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util/file_util.h"
#include "util/report_util.h"
#include "string_util.h"

#include "util/failsim.h"



static GHashTable * fst = NULL;


typedef struct fsim_call_occ_rec {
   Fsim_Call_Occ_Type   call_occ_type;
   int                  occno;
   int                  rc;
} Fsim_Call_Occ_Rec;


#define FSIM_FUNC_REC_MARKER "FSFR"
typedef struct fsim_func_rec {
   char     marker[4];
   char *   func_name;
   int      callct;
   GArray * call_occ_recs;
} Fsim_Func_Rec;


char * fsim_call_occ_type_names[] = {"FSIM_CALL_OCC_RECURRING",
                                     "FSIM_CALL_OCC_SINGLE"
};


static void fsim_destroy_func_rec(gpointer data) {
   Fsim_Func_Rec * frec = (Fsim_Func_Rec *) data;
   assert(memcmp(frec->marker, FSIM_FUNC_REC_MARKER, 4) == 0);
   g_array_unref(frec->call_occ_recs);
   frec->marker[3] = 'x';
   free(data);
}

void fsim_destroy_key(gpointer data) {
   free(data);
}

static void report_error_table_entry(gpointer key_ptr, gpointer value_ptr, gpointer user_data_ptr) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  value_ptr=%p, user_data_ptr=%p\n", __func__, value_ptr, user_data_ptr);
    char * key = (char *) key_ptr;
    Fsim_Func_Rec * frec = (Fsim_Func_Rec *) value_ptr;
    int * depth_ptr = (int *) user_data_ptr;
    int depth = *depth_ptr;

    rpt_vstring(depth, "function:      %s", key);
    for (int ndx = 0; ndx < frec->call_occ_recs->len; ndx++) {
       Fsim_Call_Occ_Rec  occ_rec = g_array_index(frec->call_occ_recs, Fsim_Call_Occ_Rec, ndx);
       rpt_vstring(depth+1, "rc = %d, occurrences=(%s, %d)",
                             occ_rec.rc,
                             (occ_rec.call_occ_type == FSIM_CALL_OCC_RECURRING) ? "recurring" : "single",
                             occ_rec.occno);
    }

}


static GHashTable * fsim_get_or_create_failsim_table() {
   if (!fst) {
      fst = g_hash_table_new_full(g_str_hash, g_str_equal, fsim_destroy_key, fsim_destroy_func_rec);
   }
   return fst;
}

static Fsim_Func_Rec * fsim_get_or_create_func_rec(char * funcname) {
   GHashTable * fst = fsim_get_or_create_failsim_table();
   Fsim_Func_Rec * frec = g_hash_table_lookup(fst, funcname);
   if (!frec) {
      frec = calloc(1, sizeof(Fsim_Func_Rec));
      memcpy(frec->marker, FSIM_FUNC_REC_MARKER, 4);
      frec->func_name = strdup(funcname);
      frec->callct = 0;
      frec->call_occ_recs = g_array_new(
                              false,        // zero_terminated
                              true,         // clear_
                              sizeof(Fsim_Call_Occ_Rec) );
      g_hash_table_insert(fst, frec->func_name, frec);
   }
   return frec;
}


void fsim_add_error(
       char *               funcname,
       Fsim_Call_Occ_Type   call_occ_type,
       int                  occno,
       int                  rc)
{
   bool debug = true;
   if (debug)
      printf("(%s) funcname=|%s|, call_occ_type=%d, occ type: %s, occno=%d, fsim_rc=%d\n", __func__,
              funcname,
              call_occ_type,
              (call_occ_type == FSIM_CALL_OCC_RECURRING) ? "recurring" : "single",
              occno,
              rc);

   Fsim_Call_Occ_Rec callocc_rec;
   callocc_rec.call_occ_type = call_occ_type;
   callocc_rec.occno = occno;
   callocc_rec.rc = rc;

   Fsim_Func_Rec* frec = fsim_get_or_create_func_rec(funcname);

   g_array_append_val(frec->call_occ_recs, callocc_rec);
}

void fsim_reset_callct(char * funcname) {
   if (fst) {
      Fsim_Func_Rec * frec = g_hash_table_lookup(fst, funcname);
      if (frec)
         frec->callct = 0;
   }
}


void fsim_clear_errors_for_func(char * funcname) {
   if (fst) {
      // not an error if key doesn't exist
      g_hash_table_remove(fst, funcname);
   }
}


void fsim_clear_error_table() {
   if (fst) {
      g_hash_table_destroy(fst);
      fst = NULL;
   }
}



void fsim_report_error_table(int depth) {
   bool debug = false;
   int d1 = depth+1;
   if (debug)
      printf("(%s) d1=%d, &d1=%p\n", __func__, d1, &d1);

   if (fst) {
      rpt_vstring(depth, "Failure simulation table:");
      g_hash_table_foreach(fst, report_error_table_entry, &d1);
   }
   else
      rpt_vstring(depth, "Failure simulation table not initialized");
}




bool eval_fsim_rc(char * rc_string, int * evaluated_rc) {
   char * end;
   bool ok = false;
   long int answer = strtol(rc_string, &end, 10);
   if (*end == '\0') {
      *evaluated_rc = (int) answer;
      ok = true;
   }
   // to do: add else case for string
   else
      ok = false;

   return ok;
}


// cf load dumpload load variants

bool fsim_load_control_from_gptrarray(GPtrArray * lines) {
   bool debug = false;
   bool ok = true;
   bool dummy_data_flag = false;
   fst = g_hash_table_new(g_str_hash, g_str_equal);

   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * aline = g_ptr_array_index(lines, ndx);
      if (debug)
         printf("(%s) line: %s\n", __func__, aline);
      char * trimmed_line = strtrim(aline);
      if (strlen(trimmed_line) > 0 && trimmed_line[0] != '#' && trimmed_line[0] != '*') {
         Null_Terminated_String_Array pieces = strsplit(trimmed_line, " ");
         if (debug)
            null_terminated_string_array_show(pieces);
         bool valid_line = true;
         char * funcname = NULL;
         int    fsim_rc  = 0;
         Fsim_Call_Occ_Type occtype = FSIM_CALL_OCC_SINGLE;
         int  occno = 0;
         if (null_terminated_string_array_length(pieces) != 3)
            valid_line = false;
         else {
            funcname = pieces[0];
            valid_line = eval_fsim_rc(pieces[1], &fsim_rc);
            if (valid_line) {
               char * occdef = pieces[2];
               if (*occdef == '*') {
                  occtype = FSIM_CALL_OCC_RECURRING;
                  occdef++;
               }
               if (strlen(occdef) == 0)
                  valid_line = false;
               else {
                  char * end;
                  occno = strtol(occdef, &end, 10);
                  if (*end != '\0')
                     valid_line = false;
               }

            }

            if (valid_line) {
               fsim_add_error( funcname,
                      occtype,
                      occno,
                      fsim_rc);
            }

         }
         if (!valid_line) {
            printf("(%s) Invalid control file line: %s\n", __func__, aline);
            ok = false;
         }

         null_terminated_string_array_free(pieces);
      }
      free(trimmed_line);
   }



   // Dummy data for now:
   if (dummy_data_flag) {
      printf("(%s) Ignoring failure simulation control file, loading mock data\n", __func__);
      fsim_add_error("i2c_set_addr", FSIM_CALL_OCC_RECURRING, 2, -EBUSY);
   }

   fsim_report_error_table(1);

   if (debug)
      printf("(%s) Returnind: %s\n", __func__, bool_repr(ok));
   return ok;
}

bool fsim_load_control_string(char * s) {
   bool ok = false;

   return ok;
}

bool fsim_load_control_file(char * fn) {
   bool debug = true;
   if (debug)
      printf("(%s) fn=%s\n", __func__, fn);
   bool verbose = true;   // should this be argument?
   GPtrArray * lines = g_ptr_array_new();
   int linect = file_getlines(fn,  lines, verbose);
   if (debug)
      printf("(%s) Read %d lines\n", __func__, linect);
   bool result = false;
   if (linect > 0)
      result = fsim_load_control_from_gptrarray(lines);
   // need free func
   g_ptr_array_free(lines, true);
   if (debug)
      printf("(%s) Returning: %s\n", __func__, bool_repr(result));
   return result;
}


//
// Execution time error check
//


int fsim_check_failure(const char * fn, const char * funcname) {
   bool debug = false;
   int result = 0;
   if (fst) {
      Fsim_Func_Rec * frec = g_hash_table_lookup(fst, funcname);
      if (frec) {
         frec->callct++;
         for (int ndx = 0; ndx < frec->call_occ_recs->len; ndx++) {
            Fsim_Call_Occ_Rec * occ_rec = &g_array_index(frec->call_occ_recs, Fsim_Call_Occ_Rec, ndx);
            if (debug)
               printf("(%s) call_occ_type=%d, callct = %d, occno=%d\n", __func__,
                      occ_rec->call_occ_type, frec->callct, occ_rec->occno);
            if (occ_rec->call_occ_type == FSIM_CALL_OCC_RECURRING) {

               if ( frec->callct % occ_rec->occno == 0) {
                  result = occ_rec->rc;
                  break;
               }
            }
            else {
               if (frec->callct == occ_rec->occno) {
                  result = occ_rec->rc;
                  break;
               }
            }
         }
      }
   }
   if (result)
      printf("Simulating failure for function %s, returning %d\n", funcname, result);
   if (debug)
      printf("(%s) funcname=%s, returning %d\n", __func__, funcname, result);
   return result;
}
