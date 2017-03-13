/* failsim.c
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file failsim.c
 * Functions that provide a simple failure simulation framework.
 */

/** \cond */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "debug_util.h"
#include "file_util.h"
#include "report_util.h"
#include "string_util.h"

#include "failsim.h"


static Fsim_Name_To_Number_Func name_to_number_func             = NULL;
static Fsim_Name_To_Number_Func unmodulated_name_to_number_func = NULL;

/** Sets the functions to bu used to interpret a symbolic value
 *  in a control file.
 */
void fsim_set_name_to_number_funcs(
      Fsim_Name_To_Number_Func func,
      Fsim_Name_To_Number_Func unmodulated_func)
{
   name_to_number_func = func;
   unmodulated_name_to_number_func = unmodulated_func;
}


// singleton
static GHashTable * fst = NULL;


// Describes a call occurrence for which an error is to be simulated
typedef struct fsim_call_occ_rec {
   Fsim_Call_Occ_Type   call_occ_type;
   int                  occno;
   int                  rc;
   bool                 modulated;
} Fsim_Call_Occ_Rec;


// This struct describes the failure simulation state of a function. It
// a) contains an array of FsimCall_Occ_Rec defining the simulated errors
// b) a counter updated at runtime of the number of times the function has been called.
#define FSIM_FUNC_REC_MARKER "FSFR"
typedef struct fsim_func_rec {
   char     marker[4];
   char *   func_name;
   int      callct;
   GArray * call_occ_recs;   // array of Fsim_Call_Occ_Rec
} Fsim_Func_Rec;


char * fsim_call_occ_type_names[] = {"FSIM_CALL_OCC_RECURRING",
                                     "FSIM_CALL_OCC_SINGLE"
};


// GHashTable destroy function for hash value (i.e. pointer to Fsim_Func_Rec)
static void fsim_destroy_func_rec(gpointer data) {
   Fsim_Func_Rec * frec = (Fsim_Func_Rec *) data;
   assert(memcmp(frec->marker, FSIM_FUNC_REC_MARKER, 4) == 0);
   g_array_unref(frec->call_occ_recs);
   frec->marker[3] = 'x';
   free(data);
}


// GHashTable destroy function for hash key (i.e. pointer to string)
void fsim_destroy_key(gpointer data) {
   free(data);
}


/* Reports a single entry in the error simulation table,
 * i.e. the conditions under which an error will be simulated
 * for the function and the error value for the function to return.
 *
 * Arguments:
 *   key_ptr         pointer to function name
 *   value_ptr       pointer to GArray of Fsim_Call_Occ_Rec's
 *   user_data_ptr   pointer to logical indentation depth
 */
static void report_error_table_entry(
      gpointer key_ptr,
      gpointer value_ptr,
      gpointer user_data_ptr)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  value_ptr=%p, user_data_ptr=%p\n", __func__, value_ptr, user_data_ptr);
    char * key           = (char *) key_ptr;
    Fsim_Func_Rec * frec = (Fsim_Func_Rec *) value_ptr;
    int * depth_ptr      = (int *) user_data_ptr;
    int depth            = *depth_ptr;

    rpt_vstring(depth, "function:      %s", key);
    for (int ndx = 0; ndx < frec->call_occ_recs->len; ndx++) {
       Fsim_Call_Occ_Rec  occ_rec = g_array_index(frec->call_occ_recs, Fsim_Call_Occ_Rec, ndx);
       rpt_vstring(depth+1, "rc = %d, occurrences=(%s, %d)",
                             occ_rec.rc,
                             (occ_rec.call_occ_type == FSIM_CALL_OCC_RECURRING) ? "recurring" : "single",
                             occ_rec.occno);
    }

}


/* Returns the failure simulation table.
 * If the table does not already exist it is created.
 *
 * Arguments:    none
 *
 * Returns:      pointer to failure simulation table
 */
static GHashTable * fsim_get_or_create_failsim_table() {
   if (!fst) {
      fst = g_hash_table_new_full(
               g_str_hash,
               g_str_equal,
               fsim_destroy_key,
               fsim_destroy_func_rec);
   }
   return fst;
}


/* Gets the record for a function in the failure simulation table.
 * If a record does not already exist, a new one is created.
 *
 * Arguments:
 *   funcname     function mame
 *
 * Returns:       pointer to Fsim_Func_Rec for the function
 */
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


/** Adds an error description to the failure simulation table entry for a function.
 *
 * @param  funcname       function name
 * @param  call_occ_type  recurring or single
 * @param  occno          occurrence number
 * @param  rc             return code to simulate
 */
void fsim_add_error(
       char *               funcname,
       Fsim_Call_Occ_Type   call_occ_type,
       int                  occno,
       int                  rc)
{
   bool debug = false;
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


/** Resets the call counter in a failure simulation table entry
 *
 *  @param  funcname   function name
 */
void fsim_reset_callct(char * funcname) {
   if (fst) {
      Fsim_Func_Rec * frec = g_hash_table_lookup(fst, funcname);
      if (frec)
         frec->callct = 0;
   }
}


/** Delete all error descriptors for a function.
 *
 *  @param funcname    function name
 */
void fsim_clear_errors_for_func(char * funcname) {
   if (fst) {
      // not an error if key doesn't exist
      g_hash_table_remove(fst, funcname);
   }
}


/* Clears the entire failure simulation table.
 */
void fsim_clear_error_table() {
   if (fst) {
      g_hash_table_destroy(fst);
      fst = NULL;
   }
}


/** Report the failure simulation table.
 *
 *  @param depth       logical indentation depth
 */
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


/* Evaluates a string status code expression.
 *
 * The string can take the following forms:
 *     integer   e.g.     "-42"
 *     boolean   i.e.  "true" or "false"
 *     a symbolic status code name, optionally prefixed by
 *     "modulated:" or "base:".
 *     If neither "modulated" nor "vase" is specified, "modulated" is assumed
 *     e.g. "DDC_RC_ALL_ZERO", "base:EBUSY"
 *
 * Arguments:
 *   rc_string      string to evaluate
 *   evaluated_rc   where to return evaluated value
 *
 * Returns:         true if evaluation successful, false if error
 */

// Issue: should unmodulated values be negative, or should
// an optional minus sign be recognized?
// e.g. should we specify -EBUSY for -22?
bool eval_fsim_rc(char * rc_string, int * evaluated_rc) {
   bool debug = true;
   if (debug)
      printf("(%s) Starting.  rc_string=%s\n", __func__, rc_string);
   char * end;
   bool ok = false;
   *evaluated_rc = 0;
   long int answer = strtol(rc_string, &end, 10);
   if (*end == '\0') {
      *evaluated_rc = (int) answer;
      ok = true;
   }
   else if (streq(rc_string, "true")) {
      *evaluated_rc = true;
      ok = true;
   }
   else if (streq(rc_string, "false")) {
      *evaluated_rc = false;
      ok = true;
   }
   else {
      bool is_modulated = true;
      if (str_starts_with(rc_string, "modulated:")) {
         is_modulated = true;
         rc_string = rc_string + strlen("modulated:");
      }
      else if (str_starts_with(rc_string, "base:")) {
         is_modulated = false;
         rc_string = rc_string + strlen("base:");
      }
      if (strlen(rc_string) == 0)
         ok = false;
      else if (is_modulated && name_to_number_func)
         ok = name_to_number_func(rc_string, evaluated_rc);
      else if (!is_modulated && unmodulated_name_to_number_func)
         ok = unmodulated_name_to_number_func(rc_string, evaluated_rc);
      else
         ok = false;
   }

   if (debug)
      printf("(%s) Starting.  rc_string=%s. Returning: %s, *evaluated_rc=%d\n",
             __func__, rc_string, bool_repr(ok), *evaluated_rc);
   return ok;
}


//
// Bulk load the failure simulation table
//
// cf dumpload load variants
//

/** Load the failure simulation table from an array of strings.
 *  Each string describes one simulated error for a function, and has
 *  the form:
 * @verbatim
   function_name  status_code occurrence_descriptor
 @endverbatim
 * where:
 * - **status_code** has a form documented for eval_fsim_rc()
 * - **occurrence_descriptor** has the form "[*]integer"\n
 *   examples:
 *     - *7   every 7th call fails
 *     - 7    the 7th call fails
 *     - *1   every call fails
 *
 * Examples:
 * \verbatim
   i2c_set_addr  base:EBUSY 6
   ddc_verify    false      *1
 \endverbatim
 *
 * @param lines     array of lines
 */
bool fsim_load_control_from_gptrarray(GPtrArray * lines) {
   bool debug = false;
   if (debug)
      printf("(%s) lines.len = %d\n", __func__, lines->len);

   bool dummy_data_flag = false;
   // Dummy data for development
   if (dummy_data_flag) {
      printf("(%s) Loading mock data\n", __func__);
      fsim_add_error("i2c_set_addr", FSIM_CALL_OCC_RECURRING, 2, -EBUSY);
      return true;
   }

   bool ok = true;
   fst = g_hash_table_new(g_str_hash, g_str_equal);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * aline = g_ptr_array_index(lines, ndx);
      if (debug)
         printf("(%s) line: %s\n", __func__, aline);
      char * trimmed_line = strtrim(aline);
      if (strlen(trimmed_line) > 0 && trimmed_line[0] != '#' && trimmed_line[0] != '*') {
         Null_Terminated_String_Array pieces = strsplit(trimmed_line, " ");
         if (debug)
            ntsa_show(pieces);
         bool valid_line = true;
         char * funcname = NULL;
         int    fsim_rc  = 0;
         Fsim_Call_Occ_Type occtype = FSIM_CALL_OCC_SINGLE;
         int  occno = 0;
         if (ntsa_length(pieces) != 3)
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

         ntsa_free(pieces);
      }
      free(trimmed_line);
   }

   // fsim_report_error_table(0);

   if (debug)
      printf("(%s) Returnind: %s\n", __func__, bool_repr(ok));
   return ok;
}


// TODO: implement
bool fsim_load_control_string(char * s) {
   bool ok = false;

   return ok;
}


/** Loads the failure simulation table from a control file.
 *
 *  @param fn   file name
 *
 *  @return true if success, fails if error
 */
bool fsim_load_control_file(char * fn) {
   bool debug = false;
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

/** This function is called at runtime to check if a failure should be
 * simulated.
 *
 * @param  fn         name of file from which check is performed
 * @param  funcname   name of function for which check is performed
 *
 * @return  struct indicating whether failure should be simulated
 *          and if so what the status code should be.
 *          Note that the entire struct is returned on the stack, not
 *          a pointer to the struct.
 */
Failsim_Result fsim_check_failure(const char * fn, const char * funcname) {
   bool debug = false;
   Failsim_Result result = {false, 0};
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
                  result.force_failure = true;
                  result.failure_value = occ_rec->rc;
                  break;
               }
            }
            else {
               if (frec->callct == occ_rec->occno) {
                  result.force_failure = true;
                  result.failure_value = occ_rec->rc;
                  break;
               }
            }
         }
         if (result.force_failure) {
            printf("Simulating failure for call %d of function %s, returning %d\n",
                   frec->callct, funcname, result.failure_value);
            // printf("Call stack:\n");
            // why wasn't this here in the original version?
            show_backtrace(2);
         }

      }
   }

   if (debug)
      printf("(%s) funcname=%s, returning (%d,%d)\n",
             __func__, funcname, result.force_failure, result.failure_value);
   return result;
}
