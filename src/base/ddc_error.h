/* ddc_error.h
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
 *  Struct for reporting errors that collects causes
 */

#ifndef DDC_ERROR_H_
#define DDC_ERROR_H_

// #include <glib-2.0/glib.h>

// #ifdef TRANSITIONAL
#include "base/retry_history.h"
// #endif
#include "base/parms.h"
#include "base/status_code_mgt.h"


#define DDC_ERROR_MARKER "DERM"

/** Struct for reporting errors, designed for collecting retry failures */
typedef struct ddc_error_struct {
   char               marker[4];     //<:  always DERM
   Public_Status_Code psc;           //<:  status code
   char *             func;          //<:  name of function generating status code
   int                cause_ct;      //<:  number of causal errors
   struct ddc_error_struct * causes[MAX_MAX_TRIES];
   // alt:
   // GPointerArray * causes_alt;   // GPointerArray of Ddc_Error *
   // alt as linked list
   // problems:  creates confusions of cause hierarchies
   // struct ddc_error_struct * next;
} Ddc_Error;

void ddc_error_free(
      Ddc_Error * error);

Ddc_Error * ddc_error_new(
      Public_Status_Code    psc,
      const char *          func);

Ddc_Error * ddc_error_new_with_cause(
      Public_Status_Code    psc,
      Ddc_Error *           cause,
      const char *          func);

Ddc_Error * ddc_error_new_chained(
      Ddc_Error *           cause,
      const char *          func);

Ddc_Error * ddc_error_new_with_causes(
      Public_Status_Code    psc,
      Ddc_Error **          causes,
      int                   cause_ct,
      const char *          func);

Ddc_Error * ddc_error_new_with_callee_status_codes(
      Public_Status_Code    status_code,
      Public_Status_Code *  callee_status_codes,
      int                   callee_status_code_ct,
      const char *          callee_func,
      const char *          func);

Ddc_Error * ddc_error_new_retries(
      Public_Status_Code *  status_codes,
      int                   status_code_ct,
      const char *          called_func,
      const char *          func);

void ddc_error_add_cause(
      Ddc_Error *           erec,
      Ddc_Error *           cause);

void ddc_error_set_status(
      Ddc_Error *           erec,
      Public_Status_Code    psc);

char * ddc_error_causes_string(
      Ddc_Error *           erec);

void ddc_error_report(
      Ddc_Error *           erec,
      int                   depth);

char * ddc_error_summary(
      Ddc_Error *           erec);


#ifdef TRANSITIONAL
void ddc_error_fill_retry_history(Ddc_Error * erec, Retry_History * hist);
Retry_History * ddc_error_to_new_retry_history(Ddc_Error * erec);
Ddc_Error * ddc_error_from_retry_history(Retry_History * hist, char * func);
bool ddc_error_comp(Ddc_Error * erec, Retry_History * hist);
#endif

#endif /* DDC_ERROR_H_ */
