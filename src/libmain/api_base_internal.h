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
#include <syslog.h>

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

#define API_PRECOND(expr) \
   do { \
      if (!(expr)) { \
         syslog(LOG_ERR, "Precondition failed: \"%s\" in file %s at line %d",  \
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


#define API_PRECOND_NORC(expr) \
   do { \
      if (!(expr)) { \
         syslog(LOG_ERR, "Precondition failed: \"%s\" in file %s at line %d",  \
                            #expr, __FILE__,  __LINE__);   \
         if (api_failure_mode & DDCA_PRECOND_STDERR) \
             fprintf(stderr, "Precondition failure (%s) in function %s at line %d of file %s\n", \
                             #expr, __func__, __LINE__, __FILE__); \
         if (api_failure_mode & DDCA_PRECOND_RETURN)  \
            return;  \
         abort(); \
      } \
   } while (0)


//
// Precondition Failure Mode
//

DDCA_Api_Precondition_Failure_Mode
ddca_set_precondition_failure_mode(
      DDCA_Api_Precondition_Failure_Mode failure_mode);

DDCA_Api_Precondition_Failure_Mode
ddca_get_precondition_failure_mode();


//
// Unpublished
//

/** Controls the force I2C slave address setting.
 *
 *  Normally, ioctl operation I2C_SLAVE is used to set the I2C slave address.
 *  If that returns EBUSY and this setting is in effect, slave address setting
 *  is retried using operation I2C_SLAVE_FORCE.
 *
 *  \param[in] onoff true/false
 *  \return  prior value
 *  \since 1.2.1
 */
bool
ddca_enable_force_slave_address(bool onoff);

/** Query the force I2C slave address setting.
 *
 *  \return true/false
 *  \since 1.2.1
 */
bool
ddca_is_force_slave_address_enabled(void);

#endif /* API_BASE_INTERNAL_H_ */
