/* ddcg_types.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDCG_TYPES_H_
#define DDCG_TYPES_H_


#include <glib-2.0/glib-object.h>     // glib-2.0 to avoid bogus eclipse error

#include "public/ddcutil_types.h"

typedef gint32 DdcgStatusCode;
typedef guint8 DdcgFeatureCode;





//
// Build Information
//

// /** ddcutil version */

typedef struct {
   uint8_t    major;
   uint8_t    minor;
   uint8_t    micro;
} DdcgDdcutilVersionSpec;    //  DDCA_Ddcutil_Version_Spec;

// /**
//  * DDCA_Ddcutil_Version_Spec: (rename-to DdcgDdcutilVersionSpec);
//  *
//  */


// typedef DDCA_Ddcutil_Version_Spec DdcgDdcutilVersionSpec;



#ifdef REF

/** I2C timeout types */
typedef enum{
   DDCA_TIMEOUT_STANDARD,      /**< Normal retry interval */
   DDCA_TIMEOUT_TABLE_RETRY    /**< Special timeout for Table reads and writes */
} DDCA_Timeout_Type;

/** I2C retry limit types */
typedef enum{
   DDCA_WRITE_ONLY_TRIES,     /**< Maximum write-only operation tries */
   DDCA_WRITE_READ_TRIES,     /**< Maximum read-write operation tries */
   DDCA_MULTI_PART_TRIES      /**< Maximum multi-part operation tries */
} DDCA_Retry_Type;
#endif

typedef enum {
   DDCG_WRITE_ONLY_TRIES = DDCA_WRITE_ONLY_TRIES,
   DDCG_WRITE_READ_TRIES = DDCA_WRITE_READ_TRIES,
   DDCG_MULTI_PART_TRIES = DDCA_MULTI_PART_TRIES
}
   DdcgRetryType;





#endif /* DDCG_TYPES_H_ */
