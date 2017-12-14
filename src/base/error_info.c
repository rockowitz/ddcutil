/* ddc_error.c
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

/** \f
 *  Struct for reporting errors.
 *
 *  #Ddc_Error provides a pseudo-exception framework that can be integrated
 *  with more traditional status codes.  Instead of returning a status code,
 *  a C function returns a #Ddc_Error instance in the case of an error, or
 *  NULL if there is no error.  Information about the cause of an error is
 *  retained for use by higher levels in the call stack.
 */

/** \cond */

#define _GNU_SOURCE     // for reallocarray() in stdlib.h

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>

#include "util/glib_util.h"
#include "util/report_util.h"
/** \endcond */

#include "core.h"
#include "ddc_errno.h"
#include "status_code_mgt.h"

#include "error_info.h"


static Error_Info ** empty_list = {NULL};
static int CAUSE_ALLOC_INCREMENT = 10;

/** Validates a pointer to a #Ddc_Error, using asserts */
#define VALID_DDC_ERROR_PTR(ptr) \
   assert(ptr); \
   assert(memcmp(ptr->marker, ERROR_INFO_MARKER, 4) == 0);


/** Releases a #Ddc_Error instance, including
 *  all instances it points to.
 *
 *  \param erec pointer to #Ddc_Error instance,
 *              do nothing if NULL
 */
void errinfo_free(Error_Info * erec){
   if (erec) {
      VALID_DDC_ERROR_PTR(erec);

      if (erec->cause_ct > 0) {
         for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
            errinfo_free(erec->causes3[ndx]);
         }
         free(erec->causes3);
      }

#ifdef ALT
      if (erec->causes_alt) {
         for (int ndx = 0; ndx < erec->causes_alt->len; ndx++) {
            errinfo_free( g_ptr_array_index(erec->causes_alt, ndx) );
         }
      }
#endif
      free(erec->func);
      erec->marker[3] = 'x';
      free(erec);
   }
}

#ifdef ALT
// signature satisfying GDestroyNotify()

static void ddc_error_free2(void * erec) {
   Error_Info* erec2 = (Error_Info *) erec;
   VALID_DDC_ERROR_PTR(erec2);
   errinfo_free(erec2);
}
#endif


void errinfo_add_cause(Error_Info * parent, Error_Info * cause) {
   VALID_DDC_ERROR_PTR(parent);
   VALID_DDC_ERROR_PTR(cause);

   if (parent->cause_ct+1 == parent->max_causes) {
      int new_max = parent->max_causes + CAUSE_ALLOC_INCREMENT;
#ifdef ALT
      Error_Info **  new_causes = calloc(new_max+1, sizeof(Error_Info *) );
      memcpy(new_causes, parent->causes3, parent->cause_ct * sizeof(Error_Info *) );
      free(parent->causes3);
      parent->causes3 = new_causes;
#endif
      if (parent->causes3 == empty_list) {
         parent->causes3 = calloc(new_max+1, sizeof(Error_Info *) );
      }
      else {
         parent->causes3 = reallocarray(parent->causes3, new_max+1, sizeof(Error_Info*) );
      }
      parent->max_causes = new_max;
   }
   parent->causes3[parent->cause_ct++] = cause;

#ifdef ALT
   if (!parent->causes_alt) {
      parent->causes_alt = g_ptr_array_new_with_free_func(ddc_error_free2);
      // parent->causes_alt = g_ptr_array_new();   // *** TRANSITIONAL ***
   }
   g_ptr_array_add(parent->causes_alt, cause);
#endif
}


void errinfo_set_status(Error_Info * erec, Public_Status_Code psc) {
   VALID_DDC_ERROR_PTR(erec);
   erec->psc = psc;
}


/** Creates a new #Ddc_Error instance with the specified status code
 *  and function name.
 *
 *  \param  psc  status code
 *  \param  func name of function generating status code
 *  \return pointer to new instance
 */
Error_Info *  errinfo_new(Public_Status_Code psc, const char * func) {
   Error_Info * erec = calloc(1, sizeof(Error_Info));
   memcpy(erec->marker, ERROR_INFO_MARKER, 4);
   erec->psc = psc;
   erec->causes3 = empty_list;
   erec->func = strdup(func);   // strdup to avoid constness warning, must free
   return erec;
}


/** Creates a new #Ddc_Error instance, including a reference to another
 *  instance that is the cause of the current error.
 *
 *  \param  psc   status code
 *  \param  cause pointer to another #Ddc_Error that is included as a cause
 *  \param  func  name of function creating new instance
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_with_cause(
      Public_Status_Code psc,
      Error_Info *       cause,
      const char *       func)
{
   VALID_DDC_ERROR_PTR(cause);
   Error_Info * erec = errinfo_new(psc, func);
   errinfo_add_cause(erec, cause);
   return erec;
}


/** Creates a new #Ddc_Error instance, including a reference to another
 *  instance that is the cause of the current error.  The status code
 *  of the new instance is the same as that of the referenced instance.
 *
 *  \param  cause pointer to another #Ddc_Error that is included as a cause
 *  \param  func  name of function creating new instance
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_chained(
      Error_Info * cause,
      const char * func)
{
   VALID_DDC_ERROR_PTR(cause);
   Error_Info * erec = errinfo_new_with_cause(cause->psc, cause, func);
   return erec;
}


/** Creates a new #Ddc_Error instance with a collection of
 *  instances specified as the causes.
 *
 *  \param  psc             status code of the new instance
 *  \param  causes          array of #Ddc_Error instances
 *  \param  cause_ct        number of causes
 *  \param  func            name of function creating the new #Ddc_Error
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_with_causes(
      Public_Status_Code    psc,
      Error_Info **         causes,
      int                   cause_ct,
      const char *          func)
{
   Error_Info * result = errinfo_new(psc, func);
   for (int ndx = 0; ndx < cause_ct; ndx++) {
      errinfo_add_cause(result, causes[ndx]);
   }
   return result;
}


// For creating a new Ddc_Error when the called functions
// return status codes not Ddc_Errors.

/** Creates a new #Ddc_Error instance, including references to multiple
 *  status codes from called functions that contribute to the current error.
 *  Each of the callee status codes is wrapped in a synthesized #Ddc_Error
 *  instance that is included as a cause.
 *
 *  \param  status_code
 *  \param  callee_status_codes    array of status codes
 *  \param  callee_status_code_ct  number of status codes in **callee_status_codes**
 *  \param  callee_func            name of function that returned **callee** status codes
 *  \param  func                   name of function generating new #Ddc_Error
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_with_callee_status_codes(
      Public_Status_Code    status_code,
      Public_Status_Code *  callee_status_codes,
      int                   callee_status_code_ct,
      const char *          callee_func,
      const char *          func)
{
   Error_Info * result = errinfo_new(status_code, func);
   for (int ndx = 0; ndx < callee_status_code_ct; ndx++) {
      Error_Info * cause = errinfo_new(callee_status_codes[ndx],callee_func);
      errinfo_add_cause(result, cause);
   }
   return result;
}


/** Special case of #ddc_Error_with_new_callee_status_codes() for the case
 *  where the **callee** status codes represent try errors.  The status code
 *  of the newly created instance is **DDCRC_RETRIES**.
 *
 *  \param  status_codes    array of status codes
 *  \param  status_code_ct  number of status codes in **callee_status_codes**
 *  \param  called_func     name of function that returned **callee** status codes
 *  \param  func            name of function generating new #Ddc_Error
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_retries(
      Public_Status_Code *  status_codes,
      int                   status_code_ct,
      const char *          called_func,
      const char *          func)
{
   Error_Info * result = errinfo_new(DDCRC_RETRIES, func);
   for (int ndx = 0; ndx < status_code_ct; ndx++) {
      Error_Info * cause = errinfo_new(status_codes[ndx],called_func);
      errinfo_add_cause(result, cause);
   }
   return result;
}


/** Returns a comma separated string of the status code names in the
 *  causes of the specified #Ddc_Error.
 *  Multiple consecutive identical names are replaced with a
 *  single name and a parenthesized instance count.
 *
 *  \param  erec  pointer to #Ddc_Error instance
 *  \return comma separated string, caller is responsible for freeing
 */
char * errinfo_causes_string(Error_Info * erec) {
   bool debug = false;
   // DBGMSF(debug, "history=%p, history->ct=%d", history, history->ct);

   GString * gs = g_string_new(NULL);

   if (erec) {
      assert(memcmp(erec->marker, ERROR_INFO_MARKER, 4) == 0);

#ifdef ALT
      if (erec->causes_alt) {
#endif
      bool first = true;

      int ndx = 0;
#ifdef ALT
      int cause_ct = erec->causes_alt->len;
#endif
      while (ndx < erec->cause_ct) {
         Public_Status_Code this_psc = erec->causes3[ndx]->psc;
#ifdef ALT
      while (ndx < cause_ct) {
         Error_Info * this_cause = g_ptr_array_index( erec->causes_alt, ndx);
         Public_Status_Code this_psc = this_cause->psc;
#endif
         int cur_ct = 1;

         for (int i = ndx+1; i < erec->cause_ct; i++) {
            if (erec->causes3[i]->psc != this_psc)
               break;
            cur_ct++;
         }

#ifdef ALT
         for (int i = ndx+1; i < cause_ct; i++) {
            Error_Info * next_cause = g_ptr_array_index( erec->causes_alt, i);
            if (next_cause->psc != this_psc)
               break;
            cur_ct++;
         }
#endif

         if (first)
            first = false;
         else
            g_string_append(gs, ", ");
         char * cur_name = psc_name(this_psc);
         g_string_append(gs, cur_name);
         if (cur_ct > 1)
            g_string_append_printf(gs, "(x%d)", cur_ct);
         ndx += cur_ct;
      }
#ifdef ALT
      }
#endif
   }

   char * result = gs->str;
   g_string_free(gs, false);

   DBGMSF(debug, "Done.  Returning: |%s|", result);
   return result;
}



void errinfo_report(Error_Info * erec, int depth) {
   int d1 = depth+1;

   // rpt_vstring(depth, "Status code: %s", psc_desc(erec->psc));
   // rpt_vstring(depth, "Location: %s", (erec->func) ? erec->func : "not set");
   rpt_vstring(depth, "Exception in function %s: status=%s",
         (erec->func) ? erec->func : "not set", psc_desc(erec->psc) );

   if (erec->cause_ct > 0) {
      rpt_vstring(depth, "Caused by: ");
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         errinfo_report(erec->causes3[ndx], d1);
      }
   }

#ifdef ALT
   if (erec->causes_alt && erec->causes_alt->len > 0) {
      rpt_vstring(depth, "Caused by: ");
      for (int ndx = 0; ndx < erec->causes_alt->len; ndx++) {
         errinfo_report( g_ptr_array_index(erec->causes_alt,ndx), d1);
      }
   }
#endif

}


#ifdef FUTURE
char * default_status_code_desc(int rc) {
   static GPrivate  status_code_key     = G_PRIVATE_INIT(g_free);

   const int default_status_code_buffer_size = 20;

   char * buf = get_thread_fixed_buffer(&status_code_key, default_status_code_buffer_size);
   g_snprintf(buf, default_status_code_buffer_size, "%d",rc);

   return buf;
}
#endif


/** Returns a string summary of the specified #Ddc_Error.
 *  The returned value is valid until the next call to this function in the
 *  current thread, and should not be freed by the caller.
 *
 *  \param erec  pointer to #Ddc_Error instance
 *  \return string summmay of error
 */
char * errinfo_summary(Error_Info * erec) {
   if (!erec)
      return "NULL";
   VALID_DDC_ERROR_PTR(erec);

   static GPrivate  esumm_key     = G_PRIVATE_INIT(g_free);
   static GPrivate  esumm_len_key = G_PRIVATE_INIT(g_free);

   // DBGMSG("erec=%p", erec);
   // DBGMSG("psc=%d", erec->psc);
   char * desc = psc_desc(erec->psc);  // thread safe buffer owned by psc_desc(), do not free()

   gchar * buf1 = NULL;
   if (erec->cause_ct == 0) {
#ifdef ALT
   if (erec->causes_alt || erec->causes_alt->len == 0) {
#endif
      buf1 = gaux_asprintf("Ddc_Error[%s in %s]", desc, erec->func);
   }
   else {
      char * causes   = errinfo_causes_string(erec);
      buf1 = gaux_asprintf("Ddc_Error[%s in %s, causes: %s]", desc, erec->func, causes);
      free(causes);
   }
   int required_size = strlen(buf1) + 1;

   char * buf = get_thread_dynamic_buffer(&esumm_key, &esumm_len_key, required_size);
   g_strlcpy(buf, buf1, required_size);
   free(buf1);
   return buf;
}

