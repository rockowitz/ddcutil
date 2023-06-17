/** @file failsim.h
 *
 * Functions that provide a simple failure simulation framework.
 */

// Copyright (C) 2017-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FAILSIM_H_
#define FAILSIM_H_

#include "config.h"

/** \cond */
#include <stdbool.h>
#include <glib-2.0/glib.h>

#include "error_info.h"
/** \endcond */


// Issue:  Where to send error messages?

extern bool failsim_enabled;

//
// Initialization
//

typedef bool (*Fsim_Name_To_Number_Func)(const char * name, int * p_number);

void fsim_set_name_to_number_funcs(
      Fsim_Name_To_Number_Func  func,
      Fsim_Name_To_Number_Func  unmodulated_func);

/** Indicates whether a failure should occur exactly once or be recurring */
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
void fsim_report_failure_simulation_table(int depth);
void fsim_reset_callct(char * funcname);


//
// Bulk load error table
//

bool fsim_load_control_from_gptrarray(GPtrArray * lines, GPtrArray * err_msg_accumulator);
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

#ifdef UNUSED
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
#endif

bool        fsim_bool_injector(bool status, const char * fn, const char * func);
int         fsim_int_injector(int status, const char * fn, const char * func);
Error_Info* fsim_errinfo_injector(Error_Info* status, const char * fn, const char * func);

#ifdef UNUSED
#ifdef ENABLE_FAILSIM
#define INJECT_BOOL_ERROR(status)    fsim_bool_injector(status, __FILE__, __func__)
#define INJECT_INT_ERROR(status)     fsim_int_injector(status, __FILE__, __func__)
#define INJECT_ERRINFO_ERROR(status) fsim_errinfo_injector(status, __FILE__, __func__)
#else
#define INJECT_BOOL_ERROR(status)  status
#define INJECT_INT_ERROR(status)   status
#define INJECT_ERRINFO_ERROR(status) status
#endif
#endif

#endif /* FAILSIM_H_ */
