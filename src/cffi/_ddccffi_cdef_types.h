
// Extracted from ddcutil_types.h.
// These files must be kept manually in sync. (UGH)

// Basic Types

typedef int     DDCA_Status;
typedef uint8_t DDCA_Vcp_Feature_Code;
typedef void *  DDCA_Display_Identifier;
typedef void *  DDCA_Display_Ref;
typedef void *  DDCA_Display_Handle;

typedef struct {
   int iAdapterIndex;  /**< adapter number */
   int iDisplayIndex;  /**< display number */
} DDCA_Adlno;


// Build Information

typedef struct {
   uint8_t    major;
   uint8_t    minor;
   uint8_t    micro;
} DDCA_Ddcutil_Version_Spec;

typedef enum {
   /** @brief ddcutil was built with support for AMD Display Library connected monitors */
   DDCA_BUILT_WITH_ADL     = 0x01,
   /** @brief ddcutil was built with support for USB connected monitors */
   DDCA_BUILT_WITH_USB     = 0x02,
  /** @brief ddcutil was built with support for failure simulation */
   DDCA_BUILT_WITH_FAILSIM = 0x04
} DDCA_Build_Option_Flags;


// I2C Protocol Control

typedef enum{
   DDCA_TIMEOUT_STANDARD,      /**< Normal retry interval */
   DDCA_TIMEOUT_TABLE_RETRY    /**< Special timeout for Table reads and writes */
} DDCA_Timeout_Type;

typedef enum{
   WRITE_ONLY_TRIES_OP,     /**< Maximum write-only operation tries */
   WRITE_READ_TRIES_OPE,     /**< Maximum read-write operation tries */
   DDCA_MULTI_PART_TRIES      /**< Maximum multi-part operation tries */
} Retry_Operation;


// Message Control

typedef enum {
   DDCA_OL_TERSE  =0x04,         /**< Brief   output  */
   DDCA_OL_NORMAL =0x08,         /**< Normal  output */
   DDCA_OL_VERBOSE=0x10          /**< Verbose output */
} DDCA_Output_Level;


// Performance Statistics

typedef enum {
   DDCA_STATS_NONE     = 0x00,    ///< no statistics
   DDCA_STATS_TRIES    = 0x01,    ///< retry statistics
   DDCA_STATS_ERRORS   = 0x02,    ///< error statistics
   DDCA_STATS_CALLS    = 0x04,    ///< system calls
   DDCA_STATS_ELAPSED  = 0x08,     ///< total elapsed time
   DDCA_STATS_ALL      = 0xFF     ///< indicates all statistics types
} DDCA_Stats_Type;

// MCCS Version

typedef struct {
   uint8_t    major;           /**< major version number */
   uint8_t    minor;           /*** minor version number */
} DDCA_MCCS_Version_Spec;


typedef enum {
   DDCA_MCCS_VNONE =  0,     /**< As query, match any MCCS version, as response, version unknown */
   DDCA_MCCS_V10   =  1,     /**< MCCS v1.0 */
   DDCA_MCCS_V20   =  2,     /**< MCCS v2.0 */
   DDCA_MCCS_V21   =  4,     /**< MCCS v2.1 */
   DDCA_MCCS_V30   =  8,     /**< MCCS v3.0 */
   DDCA_MCCS_V22   = 16      /**< MCCS v2.2 */
} DDCA_MCCS_Version_Id;

// #define DDCA_MCCS_VANY  DDCA_MCCS_VNONE    /**< For use on queries,   indicates match any version */
// #define DDCA_MCCS_VUNK  DDCA_MCCS_VNONE    /**< For use on responses, indicates version unknown   */



typedef enum {
   DDCA_IO_I2C,        /**< Use DDC to communicate with a /dev/i2c-n device */
   DDCA_IO_ADL,        /**< Use ADL API */
   DDCA_IO_USB         /**< Use USB reports for a USB connected monitor */
} DDCA_IO_Mode;

typedef struct {
   DDCA_IO_Mode io_mode;        ///< physical access mode
   union {
      int        i2c_busno;     ///< I2C bus number
      DDCA_Adlno adlno;         ///< ADL iAdapterIndex/iDisplayIndex pair
      int        hiddev_devno;  ///* USB hiddev device  number
   } path;
} DDCA_IO_Path;

// INVALID: #define DDCA_DISPLAY_INFO_MARKER "DDIN"
// const char DDCA_DISPLAY_INFO_MARKER[4] = ['D','D','I','N'];
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
   DDCA_Display_Ref  dref;            ///< opaque display reference
} DDCA_Display_Info;


/** Collection of #DDCA_Display_Info */
typedef struct {
   int                ct;       ///< number of records
   DDCA_Display_Info  info[];   ///< array whose size is determined by ct
} DDCA_Display_Info_List;


// VCP Feature Information

/** Flags specifying VCP feature attributes, which can be VCP version dependent. */
typedef uint16_t DDCA_Version_Feature_Flags;

// Bits in DDCA_Version_Feature_Flags:

// Exactly 1 of DDCA_RO, DDCA_WO, DDCA_RW is set
#define DDCA_RO           0x0400               /**< Read only feature */
#define DDCA_WO           0x0200               /**< Write only feature */
#define DDCA_RW           0x0100               /**< Feature is both readable and writable */
// cffi parser can't handle:
// #define DDCA_READABLE     (DDCA_RO | DDCA_RW)  /**< Feature is either RW or RO */
// #define DDCA_WRITABLE     (DDCA_WO | DDCA_RW)  /**< Feature is either RW or WO */
static const int DDCA_READABLE = (DDCA_RO | DDCA_RW);
static const int DDCA_WRITABLE = (DDCA_WO | DDCA_RW);

// Further refine the C/NC/TABLE categorization of the MCCS spec
// Exactly 1 of the following 7 bits is set
#define DDCA_STD_CONT     0x0080       /**< Normal continuous feature */
#define DDCA_COMPLEX_CONT 0x0040       /**< Continuous feature with special interpretation */
#define DDCA_SIMPLE_NC    0x0020       /**< Non-continuous feature, having a defined list of values in byte SL */
#define DDCA_COMPLEX_NC   0x0010       /**< Non-continuous feature, having a complex interpretation using one or more of SL, SH, ML, MH */
// For WO NC features.  There's no interpretation function or lookup table
// Used to mark that the feature is defined for a version
#define DDCA_WO_NC        0x0008       /**< Used internally for write-only non-continuous features */
#define DDCA_NORMAL_TABLE 0x0004       /**< Normal RW table type feature */
#define DDCA_WO_TABLE     0x0002       /**< Write only table feature */

// #define DDCA_CONT         (DDCA_STD_CONT|DDCA_COMPLEX_CONT)            /**< Continuous feature, of any subtype */
// #define DDCA_NC           (DDCA_SIMPLE_NC|DDCA_COMPLEX_NC|DDCA_WO_NC)  /**< Non-continuous feature of any subtype */
// #define DDCA_NON_TABLE    (DDCA_CONT | DDCA_NC)                        /**< Non-table feature of any type */

// #define DDCA_TABLE        (DDCA_NORMAL_TABLE | DDCA_WO_TABLE)        /**< Table type feature, of any subtype */

static const uint16_t  DDCA_CONT      =   (DDCA_STD_CONT|DDCA_COMPLEX_CONT);            /**< Continuous feature, of any subtype */
static const uint16_t  DDCA_NC        =   (DDCA_SIMPLE_NC|DDCA_COMPLEX_NC|DDCA_WO_NC);  /**< Non-continuous feature of any subtype */
static const uint16_t  DDCA_NON_TABLE =  (DDCA_CONT | DDCA_NC);                        /**< Non-table feature of any type */

// Additional bits:
#define DDCA_DEPRECATED   0x0001     /**< Feature is deprecated in the specified VCP version */

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


// #define VCP_VERSION_SPECIFIC_FEATURE_INFO_MARKER "VSFI"
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
   // VCP_Feature_Subset                 vcp_subsets;   // Need it?
   char *                                feature_name;  /**< feature name */
   DDCA_Feature_Flags                    feature_flags;
} DDCA_Version_Feature_Info;


// Represent the Capabilities string returned by a monitor

// #define DDCA_CAP_VCP_MARKER  "DCVP"
/** Represents one feature code in the vcp() section of the capabilities string. */
typedef
struct {
   char                                 marker[4];     /**< Always DDCA_CAP_VCP_MARKER */
   DDCA_Vcp_Feature_Code                feature_code;  /**< VCP feature code */
   int                                  value_ct;      /**< number of values declared */
   uint8_t *                            values;        /**< array of declared values */
} DDCA_Cap_Vcp;

// #define DDCA_CAPABILITIES_MARKER   "DCAP"
/** Represents a monitor capabilities string */
typedef
struct {
   char                                 marker[4];       /**< always DDCA_CAPABILITIES_MARKER */
   char *                               unparsed_string; /**< unparsed capabilities string */
   DDCA_MCCS_Version_Spec               version_spec;    /**< parsed mccs_ver() field */
   int                                  vcp_code_ct;     /**< number of features in vcp() field */
   DDCA_Cap_Vcp *                       vcp_codes;       /**< array of pointers to structs describing each vcp code */
} DDCA_Capabilities;


// Get and set VCP Feature Values

/** Indicates the physical type of a VCP value */
typedef enum {
   DDCA_NON_TABLE_VCP_VALUE = 1,   /**< Continuous (C) or Non-Continuous (NC) value */
   DDCA_TABLE_VCP_VALUE     = 2,   /**< Table (T) value */
} DDCA_Vcp_Value_Type;

/** #DDCA_Vcp_Value_Type_Parm extends #DDCA_Vcp_Value_Type to allow for its use as
    function call parameter where the type is unknown */
typedef enum {
   DDCA_UNSET_VCP_VALUE_TYPE_PARM = 0,   /**< type unknown */
   DDCA_NON_TABLE_VCP_VALUE_PARM  = 1,   /**< Continuous (C) or Non-Continuous (NC) value */
   DDCA_TABLE_VCP_VALUE_PARM      = 2,   /**< Table (T) value */
} DDCA_Vcp_Value_Type_Parm;


// VCP Values

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


/** Callback function to report VCP value change */
typedef void (*DDCA_Notification_Func)(DDCA_Status psc, DDCA_Any_Vcp_Value* valrec);

