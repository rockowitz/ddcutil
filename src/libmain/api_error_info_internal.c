// api_error_info_internal.c

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "ddcutil_types.h"

#include "util/error_info.h"
#include "util/report_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"

#include "libmain/api_error_info_internal.h"


//
// DDCA_Error_Detail related functions
//

/** Frees a #DDCA_Error_Detail instance
 *
 *  @param  instance to free
 */
void
free_error_detail(DDCA_Error_Detail * ddca_erec)
{
   if (ddca_erec) {
      assert(memcmp(ddca_erec->marker, DDCA_ERROR_DETAIL_MARKER, 4) == 0);
      for (int ndx = 0; ndx < ddca_erec->cause_ct; ndx++) {
         free_error_detail(ddca_erec->causes[ndx]);
      }
      free(ddca_erec->detail);
      ddca_erec->marker[3] = 'x';
      free(ddca_erec);
   }
}


/** Converts an internal #Error_Info instance to a publicly visible #DDCA_Error_Detail
 *
 *  @param  erec  instance to convert
 *  @return new #DDCA_Error_Detail instance
 */
DDCA_Error_Detail *
error_info_to_ddca_detail(Error_Info * erec)
{
   bool debug = false;
   DBGMSF(debug, "Starting. erec=%p", erec);
   if (debug)
      errinfo_report(erec, 2);

   DDCA_Error_Detail * result = NULL;
   if (erec) {
      // ???
      int reqd_size = sizeof(DDCA_Error_Detail) + erec->cause_ct * sizeof(DDCA_Error_Detail*);
      result = calloc(1, reqd_size);
      memcpy(result->marker, DDCA_ERROR_DETAIL_MARKER, 4);
      result->status_code = erec->status_code;
      if (erec->detail)
         result->detail = strdup(erec->detail);
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         DDCA_Error_Detail * cause = error_info_to_ddca_detail(erec->causes[ndx]);
         result->causes[ndx] = cause;
      }
      result->cause_ct = erec->cause_ct;
   }

   DBGMSF(debug, "Done. Returning: %p", result);
   if (debug)
      report_error_detail(result, 2);
   return result;
}


/** Makes a deep copy of a #DDC_Error_Detail instance.
 *
 *  @param  old  instance to copy
 *  @return new copy
 */
DDCA_Error_Detail *
dup_error_detail(DDCA_Error_Detail * old) {
   bool debug = false;
   DBGMSF(debug, "Starting. old=%p", old);
   if (debug)
      report_error_detail(old, 2);

   DDCA_Error_Detail * result = NULL;
   if (old) {
      // ???
      int reqd_size = sizeof(DDCA_Error_Detail) + old->cause_ct * sizeof(DDCA_Error_Detail*);
      result = calloc(1, reqd_size);
      memcpy(result->marker, DDCA_ERROR_DETAIL_MARKER, 4);
      result->status_code = old->status_code;
      if (old->detail)
         result->detail = strdup(old->detail);
      for (int ndx = 0; ndx < old->cause_ct; ndx++) {
         DDCA_Error_Detail * cause = dup_error_detail(old->causes[ndx]);
         result->causes[ndx] = cause;
      }
      result->cause_ct = old->cause_ct;
   }

   DBGMSF(debug, "Done. Returning: %p", result);
   if (debug)
      report_error_detail(result, 2);
   return result;
}


/** Emits a detailed report of a #DDCA_Error_Detail struct.
 *  Output is written to the current report output destination.
 *
 *  @param ddca_erec  instance to report
 *  @param depth      logical indentation depth
 */
void report_error_detail(DDCA_Error_Detail * ddca_erec, int depth)
{
   if (ddca_erec) {
      rpt_vstring(depth, "status_code=%s, detail=%s", ddcrc_desc_t(ddca_erec->status_code), ddca_erec->detail);
      if (ddca_erec->cause_ct > 0) {
         rpt_label(depth,"Caused by: ");
         for (int ndx = 0; ndx < ddca_erec->cause_ct; ndx++) {
            struct ddca_error_detail * cause = ddca_erec->causes[ndx];
            report_error_detail(cause, depth+1);
         }
      }
   }
}


// Thread-specific functions

/** Frees the #DDCA_Error_Detail (if any) for the current thread.
 */
void free_thread_error_detail() {
   Thread_Output_Settings * settings = get_thread_settings();
   if (settings->error_detail) {
      free_error_detail(settings->error_detail);
      settings->error_detail = NULL;
   }
}


/** Gets the #DDCA_Error_Detail record for the current thread
 *
 *  @return #DDCA_Error_Detail instance, NULL if none
 */
DDCA_Error_Detail * get_thread_error_detail() {
   Thread_Output_Settings * settings = get_thread_settings();
   return settings->error_detail;
}


/** Set the #DDCA_Error_Detail record for the current thread.
 *
 *  @param error_detail  #DDCA_Error_Detail record to set
 */
void save_thread_error_detail(DDCA_Error_Detail * error_detail) {
   bool debug = false;
   DBGMSF(debug, "Starting. error_detail=%p", error_detail);
   if (debug)
      report_error_detail(error_detail, 2);

   Thread_Output_Settings * settings = get_thread_settings();
   if (settings->error_detail)
      free_error_detail(settings->error_detail);
   settings->error_detail = error_detail;

   DBGMSF(debug, "Done");
}


