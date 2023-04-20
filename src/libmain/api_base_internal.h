/** @file api_base_internal.h
 *
 *  For use only by other api_... files
 */

// Copyright (C) 2015-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_BASE_INTERNAL_H_
#define API_BASE_INTERNAL_H_

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#ifdef ENABLE_SYSLOG
#include <syslog.h>
#endif
#include <threads.h>

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_c_api.h"

#include "base/per_thread_data.h"

#define DDCA_PRECOND_STDERR 0x01
#define DDCA_PRECOND_RETURN 0x02

typedef enum {
   DDCA_PRECOND_STDERR_ABORT  = DDCA_PRECOND_STDERR,
   DDCA_PRECOND_STDERR_RETURN = DDCA_PRECOND_STDERR | DDCA_PRECOND_RETURN,
   DDCA_PRECOND_RETURN_ONLY   = DDCA_PRECOND_RETURN
} DDCA_Api_Precondition_Failure_Mode;


extern bool library_initialized;

extern DDCA_Api_Precondition_Failure_Mode api_failure_mode;

#define API_PRECOND(expr) \
   do { \
      if (!(expr)) { \
         SYSLOG(LOG_ERR, "Precondition failed: \"%s\" in file %s at line %d",  \
                         #expr, __FILE__,  __LINE__);   \
         if (api_failure_mode & DDCA_PRECOND_STDERR) {  \
            DBGTRC_NOPREFIX(true, DDCA_TRC_ALL, "Precondition failure (%s) in function %s at line %d of file %s", \
                         #expr, __func__, __LINE__, __FILE__); \
            fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                             #expr, __func__, __LINE__, __FILE__); \
         } \
         if (api_failure_mode & DDCA_PRECOND_RETURN)  \
            return DDCRC_ARG;  \
         /* __assert_fail(#expr, __FILE__, __LINE__, __func__); */ \
         abort();  \
      } \
   } while (0)

#define API_PRECOND_W_EPILOG(expr) \
   do { \
      if (!(expr)) { \
         SYSLOG(LOG_ERR, "Precondition failed: \"%s\" in file %s at line %d",  \
                         #expr, __FILE__,  __LINE__);   \
         if (api_failure_mode & DDCA_PRECOND_STDERR) {  \
            DBGTRC_NOPREFIX(true, DDCA_TRC_ALL, "Precondition failure (%s) in function %s at line %d of file %s", \
                         #expr, __func__, __LINE__, __FILE__); \
            fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                             #expr, __func__, __LINE__, __FILE__); \
         } \
         if (!(api_failure_mode & DDCA_PRECOND_RETURN))  \
            abort();  \
         trace_api_call_depth--; \
         DBGTRC_RET_DDCRC(true, DDCA_TRC_ALL, DDCRC_ARG, "Precondition failure: %s=NULL", (expr)); \
         return DDCRC_ARG;  \
      } \
   } while (0)


#define API_PRECOND_RVALUE(expr) \
   ( { DDCA_Status ddcrc = 0; \
       if (!(expr)) { \
          SYSLOG(LOG_ERR, "Precondition failed: \"%s\" in file %s at line %d",  \
                          #expr, __FILE__,  __LINE__);   \
          if (api_failure_mode & DDCA_PRECOND_STDERR) {  \
             DBGTRC_NOPREFIX(true, DDCA_TRC_ALL, "Precondition failure (%s) in function %s at line %d of file %s", \
                          #expr, __func__, __LINE__, __FILE__); \
             fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                             #expr, __func__, __LINE__, __FILE__); \
          } \
          if (!(api_failure_mode & DDCA_PRECOND_RETURN))  \
             abort(); \
          ddcrc = DDCRC_ARG;  \
       } \
       ddcrc; \
    } )

#ifdef UNUSED
#define API_PRECOND_NORC(expr) \
   do { \
      if (!(expr)) { \
         SYSLOG(LOG_ERR, "Precondition failed: \"%s\" in file %s at line %d",  \
                            #expr, __FILE__,  __LINE__);   \
         if (api_failure_mode & DDCA_PRECOND_STDERR) \
             fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                             #expr, __func__, __LINE__, __FILE__); \
         if (api_failure_mode & DDCA_PRECOND_RETURN)  \
            return;  \
         abort(); \
      } \
   } while (0)
#endif


//
// Precondition Failure Mode
//

DDCA_Api_Precondition_Failure_Mode
ddca_set_precondition_failure_mode(
      DDCA_Api_Precondition_Failure_Mode failure_mode);

DDCA_Api_Precondition_Failure_Mode
ddca_get_precondition_failure_mode();



#ifdef UNUSED
#define ENSURE_LIBRARY_INITIALIZED() \
      do { \
         if (!library_initialized)  { \
            ddca_init(DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE); \
         } \
      } while (0)
#endif

#define API_PROLOG(debug_flag, format, ...) \
   do { \
      if (!library_initialized)  { \
         ddca_init(NULL, DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE); \
      } \
      if (trace_api_call_depth > 0 || is_traced_api_call(__func__) ) \
         trace_api_call_depth++; \
      dbgtrc( (debug_flag) ? DDCA_TRC_ALL : DDCA_TRC_API, DBGTRC_OPTIONS_NONE, \
            __func__, __LINE__, __FILE__, "Starting  "format, ##__VA_ARGS__); \
      if (ptd_api_profiling_enabled) ptd_profile_function_start(__func__); \
  } while(0)

#ifdef UNUSED
#define API_PROLOGX(debug_flag, _trace_groups, format, ...) \
   do { \
      if (!library_initialized)  { \
         ddca_init(DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE); \
      } \
      trace_api_call_depth++; \
      dbgtrc( (debug_flag) ? DDCA_TRC_ALL : (_trace_groups), DBGTRC_OPTIONS_NONE, \
            __func__, __LINE__, __FILE__, "Starting  "format, ##__VA_ARGS__); \
  } while(0)
#endif


#define API_EPILOG(_debug_flag, _rc, _format, ...) \
   do { \
        dbgtrc_ret_ddcrc( \
          (_debug_flag) ? DDCA_TRC_ALL : DDCA_TRC_API, DBGTRC_OPTIONS_NONE, \
          __func__, __LINE__, __FILE__, _rc, _format, ##__VA_ARGS__); \
        if (trace_api_call_depth > 0) \
           trace_api_call_depth--; \
        if (ptd_api_profiling_enabled) ptd_profile_function_end(__func__); \
        return _rc; \
   } while(0)


#define API_EPILOG_WO_RETURN(_debug_flag, _rc, _format, ...) \
   do { \
        dbgtrc_ret_ddcrc( \
          (_debug_flag) ? DDCA_TRC_ALL : DDCA_TRC_API, DBGTRC_OPTIONS_NONE, \
          __func__, __LINE__, __FILE__, _rc, _format, ##__VA_ARGS__); \
        if (trace_api_call_depth > 0) \
           trace_api_call_depth--; \
        if (ptd_api_profiling_enabled) ptd_profile_function_end(__func__); \
   } while(0)

#ifdef UNUSED
#define API_EPILOGX(_debug_flag, _trace_groups, _rc, _format, ...) \
   do { \
        dbgtrc_ret_ddcrc( \
          (_debug_flag) ? DDCA_TRC_ALL : _trace_groups, DBGTRC_OPTIONS_NONE, \
          __func__, __LINE__, __FILE__, _rc, _format, ##__VA_ARGS__); \
        trace_api_call_depth--; \
        return rc; \
   } while(0)
#endif


#ifdef UNUSED
#define ENABLE_API_CALL_TRACING() \
   do { if (is_traced_api_call(__func__)) {\
         tracing_cur_api_call = true; \
         trace_api_call_depth++; \
         /* printf("(%s) Setting tracing_cur_api_call = %s\n", __func__,  sbool(tracing_cur_api_call) ); */ \
      } \
   } while(0)
#endif

#ifdef OLD
#define DISABLE_API_CALL_TRACING() \
   do { \
      if (tracing_cur_api_call) { \
          printf("(%s) Setting tracing_cur_api_call = false\n", __func__);  \
          tracing_cur_api_call = false; \
      } \
      trace_api_call_depth--; \
   } while(0)
#endif

#define DISABLE_API_CALL_TRACING() \
   do { \
      if (trace_api_call_depth > 0) \
         trace_api_call_depth--; \
   } while(0)



//
// Unpublished
//


#endif /* API_BASE_INTERNAL_H_ */
