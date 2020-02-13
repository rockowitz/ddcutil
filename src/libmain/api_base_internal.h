/** @file api_base_internal.h
 *
 *  For use only by other api_... files
 */

// Copyright (C) 2015-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_BASE_INTERNAL_H_
#define API_BASE_INTERNAL_H_

#include <stdbool.h>
#include <assert.h>

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_c_api.h"

#define DDCA_PRECOND_STDERR 0x01
#define DDCA_PRECOND_RETURN 0x02

typedef enum {
   DDCA_PRECOND_STDERR_ABORT  = DDCA_PRECOND_STDERR,
   DDCA_PRECOND_STDERR_RETURN = DDCA_PRECOND_STDERR | DDCA_PRECOND_RETURN,
   DDCA_PRECOND_RETURN_ONLY   = DDCA_PRECOND_RETURN
} DDCA_Api_Precondition_Failure_Mode;


extern bool library_initialized;

extern DDCA_Api_Precondition_Failure_Mode api_failure_mode;

void __precond_fail (const char *__assertion, const char *__file,
            unsigned int __line, const char *__function)
     __THROW __attribute__ ((__noreturn__));

void __precond_abort ()
     __THROW __attribute__ ((__noreturn__));

#ifdef ALT1
#define PRECOND(expr)  \
      ((expr)                      \
        ? __ASSERT_VOID_CAST (0)                \
       : __precond_fail (#expr, __FILE__, __LINE__, __func__))
#endif

#ifdef ALT2
#define PRECOND(expr) assert(expr)
#endif

#ifdef ALT3
#define PRECOND(expr) \
   if (!(expr)) return DDCRC_ARG;
#endif

#ifdef ALTA
#define PRECOND_NORC(expr) PRECOND(expr)
#endif

#ifdef ALT4
#define PRECOND(expr) \
   if (!(expr)) { \
      fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                      #expr, __func__, __LINE__, __FILE__); \
      return DDCRC_ARG;  \
   }


#define PRECOND_NORC(expr) \
      if (!(expr)) { \
         fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                         #expr, __func__, __LINE__, __FILE__); \
         return; \
      }
#endif

#define PRECOND(expr) \
   if (!(expr)) { \
      if (api_failure_mode & DDCA_PRECOND_STDERR) { \
          fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                          #expr, __func__, __LINE__, __FILE__); \
      } \
      if (api_failure_mode & DDCA_PRECOND_RETURN)  \
         return DDCRC_ARG;  \
      else \
         abort(); \
   }

#define PRECOND_NORC(expr) \
   if (!(expr)) { \
      if (api_failure_mode & DDCA_PRECOND_STDERR) { \
          fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                          #expr, __func__, __LINE__, __FILE__); \
      } \
      if (api_failure_mode & DDCA_PRECOND_RETURN)  \
         return;  \
      else \
         abort(); \
   }

//
// Precondition Failure Mode
//

DDCA_Api_Precondition_Failure_Mode
ddca_set_precondition_failure_mode(
      DDCA_Api_Precondition_Failure_Mode failure_mode);

DDCA_Api_Precondition_Failure_Mode
ddca_get_precondition_failure_mode();

#endif /* API_BASE_INTERNAL_H_ */
