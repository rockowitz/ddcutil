/** @file api_base_internal.h
 *
 *  For use only by other api_... files
 */

// Copyright (C) 2015-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_BASE_INTERNAL_H_
#define API_BASE_INTERNAL_H_

#include <assert.h>
#include <stdbool.h>

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
