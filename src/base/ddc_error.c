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
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>

#include "util/glib_util.h"
#include "util/report_util.h"
/** \endcond */

#include "ddc_errno.h"
#include "retry_history.h"
#include "status_code_mgt.h"

#include "ddc_error.h"

/** Validates a pointer to a #Ddc_Error, using asserts */
#define VALID_DDC_ERROR_PTR(ptr) \
   assert(ptr); \
   assert(memcmp(ptr->marker, DDC_ERROR_MARKER, 4) == 0);


/** Releases a #Ddc_Error instance, including
 *  all instances it points to.
 *
 *  \param erec pointer to #Ddc_Error instance,
 *              do nothing if NULL
 */
void ddc_error_free(Ddc_Error * erec){
   if (erec) {
      VALID_DDC_ERROR_PTR(erec);
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         ddc_error_free(erec->causes[ndx]);
      }
      free(erec->func);
      erec->marker[3] = 'x';
      free(erec);
   }
}

void ddc_error_add_cause(Ddc_Error * parent, Ddc_Error * cause) {
   VALID_DDC_ERROR_PTR(parent);
   VALID_DDC_ERROR_PTR(cause);

   assert(parent->cause_ct < MAX_MAX_TRIES);
   parent->causes[parent->cause_ct++] = cause;
}

void ddc_error_set_status(Ddc_Error * erec, Public_Status_Code psc) {
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
Ddc_Error *  ddc_error_new(Public_Status_Code psc, const char * func) {
   Ddc_Error * erec = calloc(1, sizeof(Ddc_Error));
   memcpy(erec->marker, DDC_ERROR_MARKER, 4);
   erec->psc = psc;
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
Ddc_Error * ddc_error_new_with_cause(
      Public_Status_Code psc,
      Ddc_Error *        cause,
      const char *       func)
{
   VALID_DDC_ERROR_PTR(cause);
   Ddc_Error * erec = ddc_error_new(psc, func);
   erec->causes[0] = cause;
   erec->cause_ct  = 1;
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
Ddc_Error * ddc_error_new_chained(
      Ddc_Error * cause,
      const char * func)
{
   VALID_DDC_ERROR_PTR(cause);
   Ddc_Error * erec = ddc_error_new_with_cause(cause->psc, cause, func);
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
Ddc_Error * ddc_error_new_with_causes(
      Public_Status_Code    psc,
      Ddc_Error **          causes,
      int                   cause_ct,
      const char *          func)
{
   Ddc_Error * result = ddc_error_new(psc, func);
   for (int ndx = 0; ndx < cause_ct; ndx++) {
      ddc_error_add_cause(result, causes[ndx]);
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
Ddc_Error * ddc_error_new_with_callee_status_codes(
      Public_Status_Code    status_code,
      Public_Status_Code *  callee_status_codes,
      int                   callee_status_code_ct,
      const char *          callee_func,
      const char *          func)
{
   Ddc_Error * result = ddc_error_new(status_code, func);
   for (int ndx = 0; ndx < callee_status_code_ct; ndx++) {
      Ddc_Error * cause = ddc_error_new(callee_status_codes[ndx],callee_func);
      ddc_error_add_cause(result, cause);
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
Ddc_Error * ddc_error_new_retries(
      Public_Status_Code *  status_codes,
      int                   status_code_ct,
      const char *          called_func,
      const char *          func)
{
   Ddc_Error * result = ddc_error_new(DDCRC_RETRIES, func);
   for (int ndx = 0; ndx < status_code_ct; ndx++) {
      Ddc_Error * cause = ddc_error_new(status_codes[ndx],called_func);
      ddc_error_add_cause(result, cause);
   }
   return result;
}





#ifdef TRANSITIONAL
char * ddc_error_causes_string_old(Ddc_Error * erec) {
   // return strdup("unimplemented");
   // *** Temporary hacked up implementation ***
   // TODO: Reimplement
   Retry_History * hist = ddc_error_to_new_retry_history(erec);
   char * result = retry_history_string(hist);
   free(hist);
   return result;
}
#endif

/** Returns a comma separated string of the status code names in the
 *  causes of the specified #Ddc_Error.
 *  Multiple consecutive identical names are replaced with a
 *  single name and a parenthesized instance count.
 *
 *  \param  erec  pointer to #Ddc_Error instance
 *  \return comma separated string, caller is responsible for freeing
 */
char * ddc_error_causes_string(Ddc_Error * erec) {
   bool debug = false;
   // DBGMSF(debug, "history=%p, history->ct=%d", history, history->ct);

   GString * gs = g_string_new(NULL);

   if (erec) {
      assert(memcmp(erec->marker, DDC_ERROR_MARKER, 4) == 0);

      bool first = true;

      int ndx = 0;
      while (ndx < erec->cause_ct) {
         Public_Status_Code this_psc = erec->causes[ndx]->psc;
         int cur_ct = 1;
         for (int i = ndx+1; i < erec->cause_ct; i++) {
            if (erec->causes[i]->psc != this_psc)
               break;
            cur_ct++;
         }
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

   }

   char * result = gs->str;
   g_string_free(gs, false);

   DBGMSF(debug, "Done.  Returning: |%s|", result);
   return result;
}





void ddc_error_report(Ddc_Error * erec, int depth) {
   int d1 = depth+1;

   // rpt_vstring(depth, "Status code: %s", psc_desc(erec->psc));
   // rpt_vstring(depth, "Location: %s", (erec->func) ? erec->func : "not set");
   rpt_vstring(depth, "Exception in function %s: status=%s",
         (erec->func) ? erec->func : "not set", psc_desc(erec->psc) );
   if (erec->cause_ct > 0) {
      rpt_vstring(depth, "Caused by: ");
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         ddc_error_report(erec->causes[ndx], d1);
      }
   }
}


/** Returns a string summary of the specified #Ddc_Error.
 *  The returned value is valid until the next call to this function in the
 *  current thread, and should not be freed by the caller.
 *
 *  \param erec  pointer to #Ddc_Error instance
 *  \return string summmay of error
 */
char * ddc_error_summary(Ddc_Error * erec) {
   if (!erec)
      return "NULL";
   VALID_DDC_ERROR_PTR(erec);

   static GPrivate  esumm_key     = G_PRIVATE_INIT(g_free);
   static GPrivate  esumm_len_key = G_PRIVATE_INIT(g_free);

   char * desc = psc_desc(erec->psc);

   gchar * buf1 = NULL;
   if (erec->cause_ct == 0) {
      buf1 = gaux_asprintf("Ddc_Error[%s in %s]", desc, erec->func);
   }
   else {
      char * causes   = ddc_error_causes_string(erec);
      buf1 = gaux_asprintf("Ddc_Error[%s in %s, causes: %s]", desc, erec->func, causes);
      free(causes);
   }
   free(desc);
   int required_size = strlen(buf1) + 1;

   char * buf = get_thread_dynamic_buffer(&esumm_key, &esumm_len_key, required_size);
   g_strlcpy(buf, buf1, required_size);
   free(buf1);
   return buf;
}


#ifdef TRANSITIONAL
//
// Transitional functions
//

void ddc_error_fill_retry_history(Ddc_Error * erec, Retry_History * hist) {
   if (erec && hist) {
      VALID_DDC_ERROR_PTR(erec);
      assert(erec->psc == DDCRC_RETRIES);

      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         retry_history_add(hist, erec->causes[ndx]->psc);
      }
   }
}


Retry_History * ddc_error_to_new_retry_history(Ddc_Error * erec) {
   VALID_DDC_ERROR_PTR(erec);
   assert(erec->psc == DDCRC_RETRIES);

   Retry_History * hist = retry_history_new();
   ddc_error_fill_retry_history(erec, hist);

   return hist;
}



Ddc_Error * ddc_error_from_retry_history(Retry_History * hist, char * func) {
   assert(hist);
   assert(memcmp(hist->marker, RETRY_HISTORY_MARKER, 4) == 0);

   Ddc_Error * erec = ddc_error_new(DDCRC_RETRIES, func);
   for (int ndx = 0; ndx < hist->ct; ndx++) {
      Ddc_Error * cause = ddc_error_new(hist->psc[ndx], "dummy");
      erec->causes[ndx] = cause;
   }
   return erec;
}

bool ddc_error_comp(Ddc_Error * erec, Retry_History * hist) {
   bool match = false;

   if (!erec && !hist) {
      DBGMSG("erec == NULL, hist == NULL");
      match = true;
   }
   else if (erec && !hist) {
      DBGMSG("erec non-null, hist is null");
      match = false;
   }
   else if (!erec && hist) {
      DBGMSG("erec is null, hist is non-null");
      if (hist->ct != 0) {
         DBGMSG("Retry_History non-empty");
         match = false;
      }
      else
         match = true;
   }
   else {
      match = true;
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         DBGMSG("erec->causes[%d]->psc = %d", ndx, erec->causes[ndx]->psc);
      }
      for (int ndx = 0; ndx < hist->ct; ndx++) {
         DBGMSG("hist->psc[%d] = %d", ndx, hist->psc[ndx]);
      }
      if (erec->cause_ct != hist->ct) {
         DBGMSG("erec->cause_ct == %d, hist->ct == %d", erec->cause_ct, hist->ct);

         match = false;
      }
      else for (int ndx = 0; ndx < erec->cause_ct;  ndx++) {
         if (erec->causes[ndx]->psc  != hist->psc[ndx]) {
            DBGMSG("erec->causes[%d]->psc == %d, hist->psc[%d] = %d",
                   ndx, erec->causes[ndx]->psc, ndx, hist->psc[ndx]);
            match = false;
         }
      }
   }

   DBGMSG("Ddc_Error and Retry_History %smatch", (match) ? "" : "DO NOT");
   return match;
}

#endif
