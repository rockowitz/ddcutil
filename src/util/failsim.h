/* failsim.h
 *
 * Functions to enable runtime error simulation
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

/** @file failsim.h
 * Functions that provide a simple failure simulation framework.
 */

#ifndef FAILSIM_H_
#define FAILSIM_H_

#include "config.h"

/** \cond */
#include <stdbool.h>
#include <glib.h>
/** \endcond */


// Issue:  Where to send error messages?

//
// Initialization
//

typedef bool (*Fsim_Name_To_Number_Func)(const char * name, int * p_number);

void fsim_set_name_to_number_funcs(
      Fsim_Name_To_Number_Func  func,
      Fsim_Name_To_Number_Func  unmodulated_func);

/** Indicates whether a failure should occur exactly one or be recurring */
typedef enum {FSIM_CALL_OCC_RECURRING, FSIM_CALL_OCC_SINGLE} Fsim_Call_Occ_Type;


//
// Error table manipulation
//

void fsim_add_error(
       char *               funcname,
       Fsim_Call_Occ_Type   call_occ_type,
       int                  occno,
       int                  rc);
void fsim_clear_errors_for_func(char * funcname);
void fsim_clear_error_table();
void fsim_report_error_table(int depth);
void fsim_reset_callct(char * funcname);


//
// Bulk load error table
//

bool fsim_load_control_from_gptrarray(GPtrArray * lines);

// bool fsim_load_control_string(char * s);         // unimplemented

bool fsim_load_control_file(char * fn);


//
// Runtime error check
//

// int fsim_check_failure(const char * fn, const char * funcname);

/* Return value for fsim_check_failure().
 * Indicates whether a failure should be forced and if so the
 * simulated status code the function should return.
 */
typedef struct {
   bool   force_failure;
   int    failure_value;
} Failsim_Result;

Failsim_Result fsim_check_failure(const char * fn, const char * funcname);

#ifdef ENABLE_FAILSIM

#define FAILSIM \
   do { \
      Failsim_Result __rcsim = fsim_check_failure(__FILE__, __func__); \
      if (__rcsim.force_failure)        \
         return __rcsim.failure_value;  \
   } while(0);


#define FAILSIM_EXT(__addl_cmds) \
   do { \
      Failsim_Result __rcsim = fsim_check_failure(__FILE__, __func__); \
      if (__rcsim.force_failure) {      \
         __addl_cmds;     \
         return __rcsim.failure_value;  \
      } \
   } while(0);

#else

#define FAILSIM

#define FAILSIM_EXT(__addl_cmds)

#endif

#endif /* FAILSIM_H_ */
