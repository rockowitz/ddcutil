/** @file ddcutil_types_internal.h
 *
 *  Declarations removed from ddcutil_types.h
 *  because they no longer need to  public.
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_TYPES_INTERNAL_H_
#define DDCUTIL_TYPES_INTERNAL_H_



/** Callback function to report VCP value change */
typedef void (*DDCA_Notification_Func)(DDCA_Status psc, DDCA_Any_Vcp_Value* valrec);
typedef int (*Simple_Callback_Func)(int val);


//
// I2C Protocol Control
//

//! I2C retry limit types
typedef enum{
   DDCA_WRITE_ONLY_TRIES,     /**< Maximum write-only operation tries */
   DDCA_WRITE_READ_TRIES,     /**< Maximum read-write operation tries */
   DDCA_MULTI_PART_TRIES      /**< Maximum multi-part operation tries */
} DDCA_Retry_Type;


//! Trace Control
//!
//! Used as bitflags to specify multiple trace types
typedef enum {
   DDCA_TRC_BASE  = 0x0080,       /**< base functions          */
   DDCA_TRC_I2C   = 0x0040,       /**< I2C layer               */
   DDCA_TRC_ADL   = 0x0020,       /**< @deprecated ADL layer   */
   DDCA_TRC_DDC   = 0x0010,       /**< DDC layer               */
   DDCA_TRC_USB   = 0x0008,       /**< USB connected display functions */
   DDCA_TRC_TOP   = 0x0004,       /**< ddcutil mainline        */
   DDCA_TRC_ENV   = 0x0002,       /**< environment command     */
   DDCA_TRC_API   = 0x0001,       /**< top level API functions */
   DDCA_TRC_UDF   = 0x0100,       /**< user-defined, aka dynamic, features */
   DDCA_TRC_VCP   = 0x0200,       /**< VCP layer, feature definitions */
   DDCA_TRC_DDCIO = 0x0400,       /**< DDC IO functions */
   DDCA_TRC_SLEEP = 0x0800,       /**< low level sleeps */
   DDCA_TRC_RETRY = 0x1000,       /**< successful retries, subset of DDCA_TRC_DDCIO */

   DDCA_TRC_NONE  = 0x0000,       /**< all tracing disabled    */
   DDCA_TRC_ALL   = 0xffff        /**< all tracing enabled     */
} DDCA_Trace_Group;


#ifdef ADL
/** @deprecated ADL adapter number/display number pair, which identifies a display */
typedef struct {
   int iAdapterIndex;  /**< adapter number */
   int iDisplayIndex;  /**< display number */
} DDCA_Adlno;
#endif
// uses -1,-1 for unset



#endif /* DDCUTIL_TYPES_INTERNAL_H_ */
