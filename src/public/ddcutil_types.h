/* ddcutil_types.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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
 *  **ddcutil** implementation, whereas the function declarations are only referenced
 *  within the code that implements the API.
 */

#ifndef DDCUTIL_TYPES_H_
#define DDCUTIL_TYPES_H_

/** \cond */
// #include <linux/limits.h>
// #include <stdbool.h>            // used only in obsolete DDCA_Global_Failure_Information
#include <stdint.h>                // for uint8_t, unit16_t
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

//! ddcutil version
//!
typedef struct {
   uint8_t    major;          ///< Major release number
   uint8_t    minor;          ///< Minor release number
   uint8_t    micro;          ///< Micro release number
} DDCA_Ddcutil_Version_Spec;


//
// Global Settings
//

#ifdef OBSOLETE
//! Failure information filled in at the time of a program abort,
//! before longjmp() is called.
typedef struct {
   bool       info_set_fg;
   char       funcname[64];
   int        lineno;
   char       fn[PATH_MAX];
   int        status;
} DDCA_Global_Failure_Information;
#endif


//
// I2C Protocol Control
//

//! I2C timeout types
//!
typedef enum{
   DDCA_TIMEOUT_STANDARD,      /**< Normal retry interval */
   DDCA_TIMEOUT_TABLE_RETRY    /**< Special timeout for Table reads and writes */
} DDCA_Timeout_Type;

//! I2C retry limit types
//
typedef enum{
   DDCA_WRITE_ONLY_TRIES,     /**< Maximum write-only operation tries */
   DDCA_WRITE_READ_TRIES,     /**< Maximum read-write operation tries */
   DDCA_MULTI_PART_TRIES      /**< Maximum multi-part operation tries */
} DDCA_Retry_Type;


//
// Message Control
//

//! Output Level
//!
//! Values assigned to constants allow them to be or'd in bit flags.
//!
//! Values are ascending in order of verbosity
//!
typedef enum {
   DDCA_OL_TERSE  =0x04,         /**< Brief   output  */
   DDCA_OL_NORMAL =0x08,         /**< Normal  output */
   DDCA_OL_VERBOSE=0x10          /**< Verbose output */
} DDCA_Output_Level;


//
// Performance statistics
//

//! Used as values to specify a single statistics type, and as
//! bitflags to select statistics types.
typedef enum {
   DDCA_STATS_NONE     = 0x00,    ///< no statistics
   DDCA_STATS_TRIES    = 0x01,    ///< retry statistics
   DDCA_STATS_ERRORS   = 0x02,    ///< error statistics
   DDCA_STATS_CALLS    = 0x04,    ///< system calls
   DDCA_STATS_ELAPSED  = 0x08,    ///< total elapsed time
   DDCA_STATS_ALL      = 0xFF     ///< indicates all statistics types
} DDCA_Stats_Type;


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


/** ADL adapter number/display number pair, which identifies a display */
typedef struct {
   int iAdapterIndex;  /**< adapter number */
   int iDisplayIndex;  /**< display number */
} DDCA_Adlno;
// uses -1,-1 for unset



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

extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V10;        ///< MCCS version 1.0
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V20;        ///< MCCS version 2.0
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V21;        ///< MCCS version 2.1
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V30;        ///< MCCS version 3.0
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_V22;        ///< MCCS version 2.2
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_ANY;        ///< used as query specifier
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_UNKNOWN;    ///< value for monitor has been queried unsuccessfully
extern const DDCA_MCCS_Version_Spec DDCA_VSPEC_UNQUERIED;  ///< indicates version not queried



/** @name version_id
 *  Ids for MCCS/VCP versions, reflecting the fact that
 *  there is a small set of valid version values.
 */
///@{

// in sync w constants MCCS_V.. in vcp_feature_codes.c
/** MCCS (VCP) Feature Version IDs */
typedef enum {
   DDCA_MCCS_VNONE =   0,     /**< As response, version unknown */
   DDCA_MCCS_V10   =   1,     /**< MCCS v1.0 */
   DDCA_MCCS_V20   =   2,     /**< MCCS v2.0 */
   DDCA_MCCS_V21   =   4,     /**< MCCS v2.1 */
   DDCA_MCCS_V30   =   8,     /**< MCCS v3.0 */
   DDCA_MCCS_V22   =  16,     /**< MCCS v2.2 */
   DDCA_MCCS_VANY  = 255      /**< On queries, match any VCP version */
} DDCA_MCCS_Version_Id;

#define DDCA_MCCS_VUNK  DDCA_MCCS_VNONE    /**< For use on responses, indicates version unknown   */

///@}

/** MCCS VCP Feature Id */
typedef uint8_t DDCA_Vcp_Feature_Code;


/** Bitfield specifying a collection of VCP feature codes
 *
 *  @remark
 *  This struct might be more appropriately named DDCA_Feature_Set, but
 *  that results in confusing function names such as ddca_feature_set_set()
 */
typedef struct {
   uint8_t bytes[32];
} DDCA_Feature_List;


/** Identifiers for publicly useful VCP feature subsets
 *
 * @remark
 * These subset identifiers represent a subset of the much
 * larger collection of subset ids used internally.
 */
typedef enum {
   DDCA_SUBSET_KNOWN,          ///< All features defined in a MCCS spec
   DDCA_SUBSET_COLOR,          ///< Color related features
   DDCA_SUBSET_PROFILE,        ///< Features saved and restored by loadvcp/setvcp
   DDCA_SUBSET_MFG             ///< Feature codes reserved for manufacturer use (0x0e..0xff)
} DDCA_Feature_Subset_Id;


//
// Display Information
//

/** Indicates how a display is accessed */
typedef enum {
   DDCA_IO_I2C,     /**< Use DDC to communicate with a /dev/i2c-n device */
   DDCA_IO_ADL,     /**< Use ADL API */
   DDCA_IO_USB      /**< Use USB reports for a USB connected monitor */
} DDCA_IO_Mode;


/** Describes a display's physical access mode and the location identifiers for that mode  */
typedef struct {
   DDCA_IO_Mode io_mode;        ///< physical access mode
   union {
      int        i2c_busno;     ///< I2C bus number
      DDCA_Adlno adlno;         ///< ADL iAdapterIndex/iDisplayIndex pair
      int        hiddev_devno;  ///* USB hiddev device  number
   } path;
} DDCA_IO_Path;


#define DDCA_DISPLAY_INFO_MARKER "DDIN"
/** Describes one monitor detected by ddcutil. */
typedef struct {
   char              marker[4];        ///< always "DDIN"
   int               dispno;           ///< ddcutil assigned display number
   DDCA_IO_Path      path;             ///< physical access path to display
   int               usb_bus;          ///< USB bus number, if USB connection
   int               usb_device;       ///< USB device number, if USB connection
   // or should these be actual character/byte arrays instead of pointers?
   const char *      mfg_id;          ///< 3 character manufacturer id, from EDID
   const char *      model_name;      ///< model name, from EDID
   const char *      sn;              ///< ASCII serial number string from EDID
   const uint8_t *   edid_bytes;      ///< raw bytes (128) of first EDID block
   DDCA_MCCS_Version_Spec vcp_version;
   DDCA_MCCS_Version_Id   vcp_version_id;
   DDCA_Display_Ref  dref;            ///< opaque display reference
} DDCA_Display_Info;


/** Collection of #DDCA_Display_Info */
typedef struct {
   int                ct;       ///< number of records
   DDCA_Display_Info  info[];   ///< array whose size is determined by ct
} DDCA_Display_Info_List;




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
 * of the VESA MCCS specification.  Exactly 1 of these bits is set.
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
// Exactly 1 of the following 8 bits is set
#define DDCA_STD_CONT     0x0080       /**< Normal continuous feature */
#define DDCA_COMPLEX_CONT 0x0040       /**< Continuous feature with special interpretation */
#define DDCA_SIMPLE_NC    0x0020       /**< Non-continuous feature, having a defined list of values in byte SL */
#define DDCA_COMPLEX_NC   0x0010       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
#define DDCA_NC_CONT      0x0800       /**< NC feature combining reserved values with continuous range */
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define DDCA_WO_NC        0x0008       /**< Used internally for write-only non-continuous features */
#define DDCA_NORMAL_TABLE 0x0004       /**< Normal RW table type feature */
#define DDCA_WO_TABLE     0x0002       /**< Write only table feature */

#define DDCA_CONT         (DDCA_STD_CONT|DDCA_COMPLEX_CONT)            /**< Continuous feature, of any subtype */
#define DDCA_NC           (DDCA_SIMPLE_NC|DDCA_COMPLEX_NC|DDCA_WO_NC|DDCA_NC_CONT)  /**< Non-continuous feature of any subtype */
#define DDCA_NON_TABLE    (DDCA_CONT | DDCA_NC)                        /**< Non-table feature of any type */

#define DDCA_TABLE        (DDCA_NORMAL_TABLE | DDCA_WO_TABLE)        /**< Table type feature, of any subtype */
// #define DDCA_KNOWN     (DDCA_CONT | DDCA_NC | DDCA_TABLE)           // *** unused ***

// Additional bits:
#define DDCA_DEPRECATED   0x0001     /**< Feature is deprecated in the specified VCP version */

///@}

typedef uint16_t DDCA_Global_Feature_Flags;

// Bits in DDCA_Global_Feature_Flags:
#define DDCA_SYNTHETIC    0x8000

typedef uint16_t DDCA_Feature_Flags;    // union (DDCA_Version_Feature_Flags, DDCA_Global_Feature_Flags)


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
/**  \deprecated
 * Describes a VCP feature code, tailored for a specific VCP version
 */
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
   // VCP_Feature_Subset                 vcp_subsets;   // Need it?
   char *                                feature_name;  /**< feature name */
   DDCA_Feature_Flags                    feature_flags;
} DDCA_Version_Feature_Info;


/** Describes a VCP feature code, tailored for a specific VCP version */
typedef
struct {
   DDCA_Vcp_Feature_Code                 feature_code;   /**< VCP feature code */
   DDCA_MCCS_Version_Spec                vspec;          /**< MCCS version    */
// DDCA_MCCS_Version_Id                  version_id;       // which ?
   DDCA_Feature_Flags                    feature_flags;  /**< feature type description */
} DDCA_Simplified_Version_Feature_Info;


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
   DDCA_NON_TABLE_VCP_VALUE = 1,   /**< Continuous (C) or Non-Continuous (NC) value */
   DDCA_TABLE_VCP_VALUE     = 2,       /**< Table (T) value */
} DDCA_Vcp_Value_Type;


/** #DDCA_Vcp_Value_Type_Parm extends #DDCA_Vcp_Value_Type to allow for its use as a
    function call parameter where the type is unknown */
typedef enum {
   DDCA_UNSET_VCP_VALUE_TYPE_PARM = 0,   /**< Unspecified */
   DDCA_NON_TABLE_VCP_VALUE_PARM  = 1,   /**< Continuous (C) or Non-Continuous (NC) value */
   DDCA_TABLE_VCP_VALUE_PARM      = 2,   /**< Table (T) value */
} DDCA_Vcp_Value_Type_Parm;


typedef struct {
   uint8_t    mh;
   uint8_t    ml;
   uint8_t    sh;
   uint8_t    sl;
} DDCA_Non_Table_Vcp_Value;


/** Represents a single table VCP value.   Consists of a count, and a pointer to the bytes */
typedef struct {
   uint16_t bytect;        /**< Number of bytes in value */
   uint8_t*  bytes;        /**< Bytes of the value */
} DDCA_Table_Vcp_Value;


/** Stores a VCP feature value of any type */
typedef struct {
   DDCA_Vcp_Feature_Code  opcode;         /**< VCP feature code */
   DDCA_Vcp_Value_Type    value_type;      // probably a different type would be better
   union {
      struct {
         uint8_t *  bytes;          /**< pointer to bytes of table value */
         uint16_t   bytect;         /**< number of bytes in table value */
      }         t;                  /**< table value */
      struct {
         uint8_t    mh;
         uint8_t    ml;
         uint8_t    sh;
         uint8_t    sl;
      }    c_nc;                /**< continuous non-continuous, i.e. non-table, value */
   }       val;
} DDCA_Any_Vcp_Value;

#define VALREC_CUR_VAL(valrec) ( valrec->val.c_nc.sh << 8 | valrec->val.c_nc.sl )
#define VALREC_MAX_VAL(valrec) ( valrec->val.c_nc.mh << 8 | valrec->val.c_nc.ml )


//
// Experimental - Not for public use
//

// values are in sync with CMD_ constants defined in ddc_command_codes.h, unify?
typedef enum {
    DDCA_Q_VCP_GET         = 0x01,    // CMD_VCP_REQUEST
    DDCA_Q_VCP_SET         = 0x03,    // CMD_VCP_SET
    DDCA_Q_VCP_RESET       = 0x09,    // CMD_VCP_RESET
    DDCA_Q_SAVE_SETTINGS  =  0x0c,    // CMD_SAVE_SETTINGS
    DDCA_Q_TABLE_READ     =  0xe2,    // CMD_TABLE_READ_REQUST
    DDCA_Q_TABLE_WRITE    = -0xe7,    // CMD_TABLE_WRITE
    DDCA_Q_CAPABILITIES   =  0xf3,    // CMD_CAPABILITIES_REQUEST
} DDCA_Queued_Request_Type;


typedef struct {
   DDCA_Queued_Request_Type   request_type;
   DDCA_Vcp_Feature_Code      vcp_code;
   // for DDCA_Q_SET
   DDCA_Non_Table_Vcp_Value       non_table_value;
} DDCA_Queued_Request;


/** Callback function to report VCP value change */
typedef void (*DDCA_Notification_Func)(DDCA_Status psc, DDCA_Any_Vcp_Value* valrec);

typedef int (*Simple_Callback_Func)(int val);

#endif /* DDCUTIL_TYPES_H_ */
