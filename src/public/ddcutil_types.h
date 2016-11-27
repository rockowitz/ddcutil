/* ddcutil_types.h
 *
 * Publicly visible type definitions.
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDCUTIL_TYPES_H_
#define DDCUTIL_TYPES_H_

#include <stdint.h>

/** @file ddcutil_types.h
 *  @brief ddcutil public types
 */


//
// Build Information
//

/** ddcutil version */
typedef struct {
   uint8_t    major;
   uint8_t    minor;
   uint8_t    build;
} DDCA_Ddcutil_Version_Spec;


//
// I2C Protocol Control
//

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


//
// Message Control
//

/** Output Level
 *
 * Values assigned to constants allow them to be or'd in bit flags.
 *
 *  matches enum Output_Level in core.h
 *  TODO: merge
 */
//typedef enum {DDCA_OL_DEFAULT=0x01,   /**< used how? */
//              DDCA_OL_PROGRAM=0x02,   /**< Special level for programmatic use */
//              DDCA_OL_TERSE  =0x04,   /**< Brief   output  */
//              DDCA_OL_NORMAL =0x08,   /**< Normal  output */
//              DDCA_OL_VERBOSE=0x10    /**< Verbose output */
//} DDCA_Output_Level;

/** Output Level
 *
 * Values assigned to constants allow them to be or'd in bit flags.
 *
 * Values are ascending in order of verbosity, except for OL_DEFAULT
 */
typedef enum {OL_DEFAULT=0x01,
              OL_PROGRAM=0x02,
              OL_TERSE  =0x04,
              OL_NORMAL =0x08,
              OL_VERBOSE=0x10
} DDCA_Output_Level;


//
//  Display Specification
//

/** Opaque display identifier
 *
 * A display identifier holds the criteria for selecting a display,
 * typically as specified by the user.
 *
 * It can take several forms:
 * - the display number assigned by dccutil
 * - an I2C bus number
 * - an ADL (adapter index, display index) pair
 * - a  USB (bus number, device number) pair
 * - an EDID
 * - model and serial number strings
 * */
typedef void * DDCA_Display_Identifier;

/** Opaque display reference.
 *
 * A display reference is a reference to a display.  It takes
 * one of three forms:
 * - an I2C bus number
 * - an ADL (adapter index, display index) pair
 * - a  USB (bus number, device number pair)
 */
typedef void * DDCA_Display_Ref;

/** Opaque display handle
 *
 * A display handle represents an open display on which actions can be performed.
 */
typedef void * DDCA_Display_Handle;


//
// VCP Feature Information
//

/** MCCS Version in binary form */
typedef struct {
   uint8_t    major;
   uint8_t    minor;
} DDCA_MCCS_Version_Spec;

// in sync w constants MCCS_V.. in vcp_feature_codes.c
/** MCCS (VCP) Feature Version IDs */
typedef enum {DDCA_VANY = 0,          /**< Match any MCCS version */
              DDCA_V10  = 1,          /**< MCCS v1.0 */
              DDCA_V20  = 2,          /**< MCCS v2.0 */
              DDCA_V21  = 4,          /**< MCCS v2.1 */
              DDCA_V30  = 8,          /**< MCCS v3.0 */
              DDCA_V22  =16           /**< MCCS v2.2 */
} DDCA_MCCS_Version_Id;

#define DDCA_VUNK DDCA_VANY

/** MCCS VCP Feature Id */
typedef uint8_t DDCA_VCP_Feature_Code;


// flags for ddca_get_feature_info():
#define DDCA_CONTINUOUS   0x4000    /**< Continuous feature */
#define DDCA_SIMPLE_NC    0x2000    /**< Non-continuous feature, having a define list of values in byte SL */
#define DDCA_COMPLEX_NC   0x1000    /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
#define DDCA_NC           (DDCA_SIMPLE_NC | DDCA_COMPLEX_NC)  /**< Non-continous feature, of any type */
#define DDCA_TABLE        0x0800    /**< Table type feature */
#define DDCA_KNOWN        (DDCA_CONTINUOUS | DDCA_NC | DDCA_TABLE)
#define DDCA_RO           0x0400    /**< Read only feature */
#define DDCA_WO           0x0200    /**< Write only feature */
#define DDCA_RW           0x0100    /**< Feature is both readable and writable */
#define DDCA_READABLE     (DDCA_RO | DDCA_RW)  /**< Feature is either RW or RO */
#define DDCA_WRITABLE     (DDCA_WO | DDCA_RW)  /**< Feature is either RW or WO */


/** One entry in array listing defined simple NC values.
 *
 * An entry of {0x00,NULL} terminates the list.
 */
typedef
struct {
   uint8_t  value_code;
   char *   value_name;
} DDCA_Feature_Value_Entry;              // identical to Feature_Value_Entry

typedef DDCA_Feature_Value_Entry * DDCA_Feature_Value_Table;


//
// Get and set VCP feature values
//

// include interpreted string?
typedef struct {
   uint8_t  mh;
   uint8_t  ml;
   uint8_t  sh;
   uint8_t  sl;
   int      max_value;
   int      cur_value;
   char *   formatted_value;
} DDCT_Non_Table_Value_Response;

typedef struct {
   int      bytect;
   uint8_t  bytes[];     // or uint8_t * ?
} DDCT_Table_Value_Response;


#endif /* DDCUTIL_TYPES_H_ */
