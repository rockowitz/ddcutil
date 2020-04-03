/** \f error_info.h
 *
 *  Struct for reporting errors that collects causes
 *
 *  #Error_Info provides a pseudo-exception framework that can be integrated
 *  with more traditional status codes.  Instead of returning a status code,
 *  a C function returns a pointer to an #Error_Info instance in the case of
 *  an error, or NULL if there is no error.  Information about the cause of an
 *  error is retained for use by higher levels in the call stack.
 */

// Copyright (C) 2017-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef ERROR_INFO_H_
#define ERROR_INFO_H_

#include <stdbool.h>

#define ERRINFO_STATUS(_erec)  ( (_erec) ? _erec->status_code : 0 )

#define ERROR_INFO_MARKER "EINF"

/** Struct for reporting errors, designed for collecting retry failures */
typedef struct error_info {
   char               marker[4];    ///<  always EINF
   int                status_code;  ///<  status code
   char *             func;         ///<  name of function generating status code
   char *             detail;       ///<  explanation (may be NULL)
   int                max_causes;   ///<  max number entries in array currently pointed to by **causes**
   int                cause_ct;     ///<  number of causal errors
   struct error_info **  causes;    ///<  pointer to array of pointers to Error_Info

#ifdef ALT
   GPtrArray *        causes_alt;   // GPointerArray of Ddc_Error *
#endif
} Error_Info;


typedef char * (*ErrInfo_Status_String)(int code);

void errinfo_init(
      ErrInfo_Status_String  name_func,
      ErrInfo_Status_String  desc_func);

void errinfo_free(
      Error_Info *   erec);

#define ERRINFO_FREE_WITH_REPORT(_erec, _report) \
   errinfo_free_with_report(_erec, (_report), __func__)

void errinfo_free_with_report(
      Error_Info *  erec,
      bool          report,
      const char *  func);

Error_Info * errinfo_new(
      int            status_code,
      const char *   func);

#define ERRINFO_NEW(_status_code) \
   errinfo_new(_status_code, __func__)

Error_Info * errinfo_new2(
      int            status_code,
      const char *   func,
      const char *   detail,
      ...);

Error_Info * errinfo_new_with_cause(
      int            status_code,
      Error_Info *   cause,
      const char *   func);

Error_Info * errinfo_new_with_cause2(
      int            status_code,
      Error_Info *   cause,
      const char *   func,
      char *         detail);

Error_Info * errinfo_new_with_cause3(
      int            status_code,
      Error_Info *   cause,
      const char *   func,
      const char *   detail_fmt,
      ...);

#ifdef UNUSED
Error_Info * errinfo_new_chained(
      Error_Info *   cause,
      const char *   func);
#endif

Error_Info * errinfo_new_with_causes(
      int            status_code,
      Error_Info **  causes,
      int            cause_ct,
      const char *   func);

Error_Info * errinfo_new_with_causes2(
      int            status_code,
      Error_Info **  causes,
      int            cause_ct,
      const char *   func,
      char *         detail);

#ifdef UNUSED
Error_Info * errinfo_new_with_callee_status_codes(
      int            status_code,
      int *          callee_status_codes,
      int            callee_status_code_ct,
      const char *   callee_func,
      const char *   func);
#endif

void errinfo_add_cause(
      Error_Info *   erec,
      Error_Info *   cause);

void errinfo_set_status(
      Error_Info *   erec,
      int    rc);

void errinfo_set_detail(
      Error_Info *   erec,
      char *         detail);

void errinfo_set_detail3(
      Error_Info *   erec,
      const char *  detail_fmt,
      ...);

char * errinfo_array_summary(
      Error_Info **  errors,
      int            error_ct);

char * errinfo_causes_string(
      Error_Info *   erec);

void errinfo_report(
      Error_Info *   erec,
      int            depth);

char * errinfo_summary(
      Error_Info *   erec);

#endif /* ERROR_INFO_H_ */
