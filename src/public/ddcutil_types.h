/* ddcutil_types.h
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


/** @file ddcutil_types.h
 *  @brief File ddcutil_types.h contains type declarations for the C API.
 *
 *  API function declarations are specified in a separate file, ddcutil_c_api.h.
 *  The reason for this split is that the type declarations are used throughout the
 *  **ddcutil** implementation, whereas the function declarations are used only
 *  within the code that implements the API.
 */


#ifndef DDCUTIL_TYPES_H_
#define DDCUTIL_TYPES_H_

/** \cond */
#include <linux/limits.h>
#include <stdbool.h>
#include <stdint.h>
/** \endcond */


//
// Status Code
//

/**
 * **ddcutil** Status Code
 *
 *  Most public **ddcutil** functions return a status code.
 *  These status codes have 3 sources:
 *  - Linux
 *  - ADL (AMD Display Library)
 *  - **ddcutil** itself
 *
 *  These multiple status code sources are consolidated by "modulating"
 *  the raw values into non-overlapping ranges.
 *  - Linux errno values are returned as negative numbers (e.g. -EIO)
 *  - ADL values are modulated by 2000 (i.e., 2000 subtracted from negative ADL status codes,
 *         or added to positive ADL status codes)
 *  - ddcutil errors are always in the -3000 range
 *
 *  In summary:
 *  - 0 always indicates a normal successful status
 *  - Positive values (possible with ADL) represent qualified success of some sort
 *  - Negative values indicate an error condition.
 */
typedef int DDCA_Status;


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
// Global Settings
//

/** Failure information filled in at the time of a program abort,
 *  before longjmp() is called.
 */
typedef struct {
   bool       info_set_fg;
   char       funcname[64];
   int        lineno;
   char       fn[PATH_MAX];
   int        status;
} DDCA_Global_Failure_Information;


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
 * Values are ascending in order of verbosity
 */
typedef enum {
   // OL_DEFAULT=0x01,    // used only within command line parser
   // OL_PROGRAM=0x02,
   DDCA_OL_TERSE  =0x04,         /**< Brief   output  */
   DDCA_OL_NORMAL =0x08,         /**< Normal  output */
   DDCA_OL_VERBOSE=0x10          /**< Verbose output */
} DDCA_Output_Level;


//
//  Display Specification
//

/** \defgroup api_display_spec API Display Specification */


/** @name Display Specification

Monitors are referenced in 3 different ways depending on contexts:

1) A #DDCA_Display_Identifier contains criteria for selecting a monitor,
typically as entered by a user.
These may directly identify a monitor (e.g. by I2C bus number), or entail a
search (e.g. EDID).

Resolving a #DDCA_Display_Identifier resolves to a #DDCA_Display_Ref.

2) #DDCA_Display_Ref indicates the operating system path to a display.
It can be an I2C identifier,an ADL identifier, or a USB identifier.

For Display_Identifiers containing an I2C bus number or ADL adapter.display numbers,
the translation from DDCA_Display_Identifier to DDCA_Display_Ref is direct.  
Otherwise, a search of detected monitors must be performed.

Opening a #DDCA_Display_Ref results in a #DDCA_Display_Handle.

3) A #DDCA_Display_Handle references a display that has been "opened".  This is used
for most function calls performing an operation on a display.

For I2C and USB connected displays, an operating system open is performed when
creating DDCA_Display_Handle from a DDCA_Display_Ref.
DDCA_Display_Handle then contains the open file handle.

For ADL displays, no actual open is performed when creating a DDCA_Display_Handle from
a DDCA_Display_Ref.  The adapter number.device number pair are simply copied.

\ingroup api_display_spec
*/
///@{

/** Opaque display identifier
 *
 * A **DDCA_Display_Identifier** holds the criteria for selecting a display,
 * typically as specified by the user.
 *
 * It can take several forms:
 * - the display number assigned by **dccutil**
 * - an I2C bus number
 * - an ADL (adapter index, display index) pair
 * - a  USB (bus number, device number) pair
 * - an EDID
 * - manufacturer, model, and serial number strings
 *
 * \ingroup api_display_spec
 * */
typedef void * DDCA_Display_Identifier;

/** Opaque display reference.
 *
 * A **DDCA_Display_Ref** references a display using the identifiers by which it is accessed
 * in Linux.  It takes one of three forms:
 * - an I2C bus number
 * - an ADL (adapter index, display index) pair
 * - a  USB (bus number, device number pair)
 *
 * \ingroup api_display_spec
 */
typedef void * DDCA_Display_Ref;


/** Opaque display handle
 *
 * A **DDCA_Display_Handle** represents an open display on which actions can be performed.
 *
 * \ingroup api_display_spec
 */
typedef void * DDCA_Display_Handle;

///@}


//
// Display Information
//

/** Indicates how a display is accessed
 *
 */
typedef enum {
   DDCA_IO_DEVI2C,     /**< Use DDC to communicate with a /dev/i2c-n device */
   DDCA_IO_ADL,        /**< Use ADL API */
   DDCA_IO_USB         /**< Use USB reports for a USB connected monitor */
} DDCA_IO_Mode;


// Does this make the API and data structures clearer or more obscure?
/** Describes a display's access mode and the location identifiers for that mode
 *
 */
typedef struct {
   DDCA_IO_Mode io_mode;
   union {
      int   i2c_busno;
      struct {
         int    iAdapterIndex;
         int    iDisplayIndex;
      } adl;
      struct {
         int    usb_bus;
         int    usb_device;
         int    hiddev_devno;       // replaced hiddev_device_name
         // char * hiddev_device_name;
      } usb;
   };
} DDCA_Display_Location;


#define DDCA_DISPLAY_INFO_MARKER "DDIN"
/** DDCA_Display_Info describes one monitor detected by ddcutil.
 *
 */
typedef struct {
   char             marker[4];
   int              dispno;
#ifdef OLD
   DDCA_IO_Mode     io_mode;
   int              i2c_busno;
   int              iAdapterIndex;
   int              iDisplayIndex;
   int              usb_bus;
   int              usb_device;
#endif
   DDCA_Display_Location loc;

   // or should these be actual character/byte arrays instead of pointers?
   const char *     mfg_id;
   const char *     model_name;
   const char *     sn;
   const uint8_t *  edid_bytes;
   DDCA_Display_Ref ddca_dref;
} DDCA_Display_Info;


typedef struct {
   int                ct;
   DDCA_Display_Info  info[];   // array whose size is determined by ct
} DDCA_Display_Info_List;



//
// VCP Feature Information
//

// Both DDCA_MCCS_Version_Spec and DDCA_MCCS_Version_Id exist for historical reasons.
// DDCA_MCCS_Version_Spec is the form in which the version number is returned from a
// GETVCP of feature xDF.  This form is used throughout much of ddcutil.
// DDCA_MCCS_Version_Id reflects the fact that there are a small number of versions
// and simplifies program logic that varies among versions.

/** MCCS Version in binary form */
typedef struct {
   uint8_t    major;           /**< major version number */
   uint8_t    minor;           /*** minor version number */
} DDCA_MCCS_Version_Spec;


/** @name version_id
 *  Ids for MCCS/VCP versions, reflecting the fact that
 *  there is a smaill set of valid version values.
 */
///@{

// in sync w constants MCCS_V.. in vcp_feature_codes.c
/** MCCS (VCP) Feature Version IDs */
typedef enum {
   DDCA_VNONE =  0,     /**< As query, match any MCCS version, as response, version unknown */
   DDCA_V10   =  1,     /**< MCCS v1.0 */
   DDCA_V20   =  2,     /**< MCCS v2.0 */
   DDCA_V21   =  4,     /**< MCCS v2.1 */
   DDCA_V30   =  8,     /**< MCCS v3.0 */
   DDCA_V22   = 16      /**< MCCS v2.2 */
} DDCA_MCCS_Version_Id;

#define DDCA_VANY  DDCA_VNONE    /**< For use on queries,   indicates match any version */
#define DDCA_VUNK  DDCA_VNONE    /**< For use on responses, indicates version unknown   */

///@}

/** MCCS VCP Feature Id */
typedef uint8_t DDCA_Vcp_Feature_Code;

/** @name Version Feature Flags
 *
 * #DDCA_Version_Feature_Flags is a byte of flags describing attributes of a
 * VCP feature that can vary by MCCS version.
 *
 * @remark
 * Exactly 1 of #DDCA_RO, #DDCA_WO, #DDCA_RW is set.
 * @remark
 * Flags #DDCA_STD_CONT, #DDCA_COMPLEX_CONT, #DDCA_SIMPLE_NC, #DDCA_COMPLEX_NC,
 * #DDCA_WO_NC, #DDCA_NORMAL_TABLE, #DDCA_WO_TABLE refine  the C/NC/TABLE categorization
 * of the VESA MCCS specification.  Exactly 1 of these bits is st.
 */
///@{

/** Flags specifying VCP feature attributes, which can be VCP version dependent. */
typedef uint16_t DDCA_Version_Feature_Flags;

// Bits in DDCA_Version_Feature_Flags:

// Exactly 1 of DDCA_RO, DDCA_WO, DDCA_RW is set
#define DDCA_RO           0x0400               /**< Read only feature */
#define DDCA_WO           0x0200               /**< Write only feature */
#define DDCA_RW           0x0100               /**< Feature is both readable and writable */
#define DDCA_READABLE     (DDCA_RO | DDCA_RW)  /**< Feature is either RW or RO */
#define DDCA_WRITABLE     (DDCA_WO | DDCA_RW)  /**< Feature is either RW or WO */

// Further refine the C/NC/TABLE categorization of the MCCS spec
// Exactly 1 of the following 7 bits is set
#define DDCA_STD_CONT       0x80       /**< Normal continuous feature */
#define DDCA_COMPLEX_CONT   0x40       /**< Continuous feature with special interpretation */
#define DDCA_SIMPLE_NC      0x20       /**< Non-continuous feature, having a defined list of values in byte SL */
#define DDCA_COMPLEX_NC     0x10       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define DDCA_WO_NC          0x08       /**< Used internally for write-only non-continuous features */
#define DDCA_NORMAL_TABLE   0x04       /**< Normal RW table type feature */
#define DDCA_WO_TABLE       0x02       /**< Write only table feature */

#define DDCA_CONT           (DDCA_STD_CONT|DDCA_COMPLEX_CONT)            /**< Continuous feature, of any subtype */
#define DDCA_NC             (DDCA_SIMPLE_NC|DDCA_COMPLEX_NC|DDCA_WO_NC)  /**< Non-continuous feature of any subtype */
#define DDCA_NON_TABLE      (DDCA_CONT | DDCA_NC)                        /**< Non-table feature of any type */

#define DDCA_TABLE          (DDCA_NORMAL_TABLE | DDCA_WO_TABLE)        /**< Table type feature, of any subtype */
// #define DDCA_KNOWN          (DDCA_CONT | DDCA_NC | DDCA_TABLE)           // *** unused ***

// Additional bits:
#define DDCA_DEPRECATED     0x01     /**< Feature is deprecated in the specified VCP version */

///@}


// Bits in vcp_global_flags:
#define DDCA_SYNTHETIC      0x80


/** One entry in array listing defined simple NC values.
 *
 * An entry of {0x00,NULL} terminates the list.
 */
typedef
struct {
   uint8_t   value_code;
   char *    value_name;
} DDCA_Feature_Value_Entry;

// Makes reference to feature value table less implementation specific
typedef DDCA_Feature_Value_Entry * DDCA_Feature_Value_Table;


#define VCP_VERSION_SPECIFIC_FEATURE_INFO_MARKER "VSFI"
/** Describes a VCP feature code, tailored for a specific VCP version */
typedef
struct {
   char                                  marker[4];      /**< equals VCP_VERSION_SPECIFIC_FEATURE_INFO_MARKER */
   DDCA_Vcp_Feature_Code                 feature_code;   /**< VCP feature code */
   DDCA_MCCS_Version_Spec                vspec;            // ???
   DDCA_MCCS_Version_Id                  version_id;       // which ?
   char *                                desc;           /**< feature description */
   // Format_Normal_Feature_Detail_Function nontable_formatter;
   // Format_Table_Feature_Detail_Function  table_formatter;
    DDCA_Feature_Value_Table             sl_values;     /**< valid when DDCA_SIMPLE_NC set */
   uint8_t                               global_flags;
   // VCP_Feature_Subset                 vcp_subsets;   // Need it?
   char *                                feature_name;  /**< feature name */
   DDCA_Version_Feature_Flags            feature_flags;
} DDCA_Version_Feature_Info;


//
// Represent the Capabilities string returned by a monitor
//

#define DDCA_CAP_VCP_MARKER  "DCVP"
/** Represents one feature code in the vcp() section of the capabilities string. */
typedef
struct {
   char                                 marker[4];     /**< Always DDCA_CAP_VCP_MARKER */
   DDCA_Vcp_Feature_Code                feature_code;  /**< VCP feature code */
   int                                  value_ct;      /**< number of values declared */
   uint8_t *                            values;        /**< array of declared values */
} DDCA_Cap_Vcp;

#define DDCA_CAPABILITIES_MARKER   "DCAP"
/** Represents a monitor capabilities string */
typedef
struct {
   char                                 marker[4];       /**< always DDCA_CAPABILITIES_MARKER */
   char *                               unparsed_string; /**< unparsed capabilities string */
   DDCA_MCCS_Version_Spec               version_spec;    /**< parsed mccs_ver() field */
   int                                  vcp_code_ct;     /**< number of features in vcp() field */
   DDCA_Cap_Vcp *                       vcp_codes;       /**< array of pointers to structs describing each vcp code */
} DDCA_Capabilities;


//
// Get and set VCP feature values
//

/** Indicates the physical data type.  At the DDC level, continuous (C) and
 *  non-continuous (NC) features are treated identically.  They share the same
 *  DDC commands (Get VCP Feature and VCP Feature Reply) and data structure.
 *  Table (T) features use DDC commands Table Write and Table Read, which take
 *  different data structures.
 */
typedef enum {
   DDCA_NON_TABLE_VCP_VALUE,   /**< Continuous (C) or Non-Continuous (NC) value */
   DDCA_TABLE_VCP_VALUE,       /**< Table (T) value */
} DDCA_Vcp_Value_Type;


#ifdef OLD
typedef struct {
   uint8_t  mh;
   uint8_t  ml;
   uint8_t  sh;
   uint8_t  sl;
   int      max_value;
   int      cur_value;
   // include interpreted string?
} DDCA_Non_Table_Value_Response;
#endif


/** Represents a single non-table VCP value */
typedef struct {
   DDCA_Vcp_Feature_Code  feature_code;
   union {
      struct {
         uint16_t   max_val;        /**< maximum value (mh, ml bytes) for continuous value */
         uint16_t   cur_val;        /**< current value (sh, sl bytes) for continuous value */
      }         c;                  /**< continuous (C) value */
      struct {
   // WORDS_BIGENDIAN ifdef ensures proper overlay of ml/mh on max_val, sl/sh on cur_val
   #ifdef WORDS_BIGENDIAN
         uint8_t    mh;
         uint8_t    ml;
         uint8_t    sh;
         uint8_t    sl;
   #else
         uint8_t    ml;            /**< ML byte for NC value */
         uint8_t    mh;            /**< MH byte for NC value */
         uint8_t    sl;            /**< SL byte for NC value */
         uint8_t    sh;            /**< SH byte for NC value */
   #endif
      }         nc;                /**< non-continuous (NC) value */
   };
} DDCA_Non_Table_Value_Response;

/** Represents a single table VCP value.   Consists of a count, followed by the bytes */
typedef struct {
   uint16_t bytect;        /**< Number of bytes in value */
   uint8_t  bytes[];       /**< Bytes of the value */
} DDCA_Table_Value;

/** Represents a single VCP value of any type */
typedef struct {
   DDCA_Vcp_Feature_Code  opcode;         /**< VCP feature code */
   DDCA_Vcp_Value_Type    value_type;      // probably a different type would be better
   union {
      struct {
         uint8_t *  bytes;          /**< pointer to bytes of table value */
         uint16_t   bytect;         /**< number of bytes in table value */
      }         t;                  /**< table value */
      struct {
         uint16_t   max_val;        /**< maximum value (mh, ml bytes) for continuous value */
         uint16_t   cur_val;        /**< current value (sh, sl bytes) for continuous value */
      }         c;                  /**< continuous (C) value */
      struct {
// WORDS_BIGENDIAN ifdef ensures proper overlay of ml/mh on max_val, sl/sh on cur_val
#ifdef WORDS_BIGENDIAN
         uint8_t    mh;
         uint8_t    ml;
         uint8_t    sh;
         uint8_t    sl;
#else
         uint8_t    ml;            /**< ML byte for NC value */
         uint8_t    mh;            /**< MH byte for NC value */
         uint8_t    sl;            /**< SL byte for NC value */
         uint8_t    sh;            /**< SH byte for NC value */
#endif
      }         nc;                /**< non-continuous (NC) value */
   }       val;
} DDCA_Single_Vcp_Value;

#endif /* DDCUTIL_TYPES_H_ */
