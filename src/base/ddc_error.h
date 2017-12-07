/* ddc_error.h
 *
 * Created on: Oct 22, 2017
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDC_ERROR_H_
#define DDC_ERROR_H_


#include <glib-2.0/glib.h>

// #ifdef TRANSITIONAL
#include "base/retry_history.h"
// #endif
#include "base/parms.h"
#include "base/status_code_mgt.h"


#define DDC_ERROR_MARKER "DERM"

typedef struct ddc_error_struct {
   char               marker[4];
   Public_Status_Code psc;
   char *             func;
   int                cause_ct;
   struct ddc_error_struct * causes[MAX_MAX_TRIES];
   // alt:
   // GPointerArray * causes_alt;   // GPointerArray of Ddc_Error *
   // alt as linked list
   // problems:  creates confusions of cause hierarchies
   // struct ddc_error_struct * next;
} Ddc_Error;

void ddc_error_free(Ddc_Error * error);

Ddc_Error *  ddc_error_new(Public_Status_Code psc, const char * func);

Ddc_Error * ddc_error_new_retries(
      Public_Status_Code *  status_codes,
      int                   status_code_ct,
      const char *                called_func,
      const char *                func);

void ddc_error_add_cause(Ddc_Error * parent, Ddc_Error * cause);

void ddc_error_set_status(Ddc_Error * erec, Public_Status_Code psc);

char * ddc_error_causes_string(Ddc_Error * erec);

void report_ddc_error(Ddc_Error * erec, int depth);


void ddc_error_fill_retry_history(Ddc_Error * erec, Retry_History * hist);
Retry_History * ddc_error_to_new_retry_history(Ddc_Error * erec);

#ifdef TRANSITIONAL
Ddc_Error * ddc_error_from_retry_history(Retry_History * hist, char * func);
bool ddc_error_comp(Ddc_Error * erec, Retry_History * hist);
#endif


#endif /* DDC_ERROR_H_ */
