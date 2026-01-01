/** @file displays.h  Display Specification  */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DISPLAYS_H_
#define DISPLAYS_H_

/** \cond **/
#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "util/coredefs.h"
#include "util/edid.h"
/** \endcond */

#include "public/ddcutil_types.h"

#include "core.h"
#include "ddcutil_types_internal.h"
#include "dynamic_features.h"
#include "feature_set_ref.h"
#include "i2c_bus_base.h"
#include "monitor_model_key.h"
#include "vcp_version.h"


/** \file
Display Specification

Monitors are specified in different ways in different contexts:

1) Display_Identifier represents an identifier specified on the command line.

2) Display_Ref is a logical display identifier.   It can be an I2C identifier
or a USB identifier.

For Display_Identifiers containing a busno (for I2C) or hiddev device number (USB),
the translation from Display_Identier to Display_Ref is direct.
Otherwise, displays are searched to find the monitor.

3) Display_Handle is passed as an argument to "open" displays.

For I2C displays, the device must be opened.  Display_Handle then contains the open file handle.
*/

// *** Globals ***

extern GPtrArray * all_display_refs;         // all detected displays, array of Display_Ref *
extern GMutex      all_display_refs_mutex;
extern bool        debug_locks;

// *** Initialization ***

void init_displays();
void terminate_displays();


// *** DDCA_IO_Path ***

#define BUSNO_NOT_SET 255

char *       io_mode_name(DDCA_IO_Mode val);
bool         dpath_eq(DDCA_IO_Path p1, DDCA_IO_Path p2);
char *       dpath_short_name_t(DDCA_IO_Path * dpath);
char *       dpath_repr_t(DDCA_IO_Path * dpath);  // value valid until next call
int          dpath_hash(DDCA_IO_Path path);
DDCA_IO_Path i2c_io_path(int busno);
DDCA_IO_Path usb_io_path(int hiddev_devno);


// *** Display_Identifier ***

/** Display_Identifier type */
typedef enum {
   DISP_ID_BUSNO,      ///< /dev/i2c bus number
   DISP_ID_MONSER,     ///< monitor mfg id, model name, and/or serial number
   DISP_ID_EDID,       ///< 128 byte EDID
   DISP_ID_DISPNO,     ///< ddcutil assigned display number
   DISP_ID_USB,        ///< USB bus/device number pair
   DISP_ID_HIDDEV      ///< /dev/usb/hiddev device number
} Display_Id_Type;

char * display_id_type_name(Display_Id_Type val);

#define DISPLAY_IDENTIFIER_MARKER "DPID"
/** Specifies the identifiers to be used to select a display. */
typedef struct {
   char            marker[4];         // always "DPID"
   Display_Id_Type id_type;
   int             dispno;
   int             busno;
   char            mfg_id[EDID_MFG_ID_FIELD_SIZE];
   char            model_name[EDID_MODEL_NAME_FIELD_SIZE];
   char            serial_ascii[EDID_SERIAL_ASCII_FIELD_SIZE];
   int             usb_bus;
   int             usb_device;
   int             hiddev_devno;           // 4/1027
   Byte            edidbytes[128];
   char *          repr;
} Display_Identifier;

Display_Identifier* create_dispno_display_identifier(int dispno);
Display_Identifier* create_busno_display_identifier(int busno);
Display_Identifier* create_edid_display_identifier(const Byte* edidbytes);
Display_Identifier* create_mfg_model_sn_display_identifier(const char* mfg_code, const char* model_name, const char* serial_ascii);
Display_Identifier* create_usb_display_identifier(int bus, int device);
Display_Identifier* create_usb_hiddev_display_identifier(int hiddev_devno);
char *              did_repr(Display_Identifier * pdid);
void                dbgrpt_display_identifier(Display_Identifier * pdid, int depth);
void                free_display_identifier(Display_Identifier * pdid);

// Display_Selector will replace Display_Identifier
// Maintain both in parallel for testing
#define DISPLAY_SELECTOR_MARKER "DSEL"
typedef struct {
   char            marker[4];         // always "DSEL"
   int             dispno;
   int             busno;
   char *          mfg_id;
   char *          model_name;
   char *          serial_ascii;
   int             usb_bus;
   int             usb_device;
   int             hiddev_devno;
   Byte *          edidbytes;   // always 128 bytes
} Display_Selector;


Display_Selector * dsel_new();
void               dsel_free(              Display_Selector* dsel);
bool               dsel_is_empty(          Display_Selector* dsel);
bool               dsel_only_busno(        Display_Selector* dsel);
char *             dsel_repr_t(              Display_Selector* dsel);
void               dbgrpt_display_selector(Display_Selector* dsel, int depth);
Display_Selector * display_id_to_dsel(     Display_Identifier * pdid);
#ifdef NOT_NEEDED
void               dsel_set_display_number(Display_Selector* dsel, int dispno);
void               dsel_set_i2c_busno(     Display_Selector* dsel, int busno);
void               dsel_set_hiddev_devno(  Display_Selector* dsel, int hiddev_devno);
void               dsel_set_usb_numbers(   Display_Selector* dsel, int bus, int device);
void               dsel_set_mfg_id(        Display_Selector* dsel, char*  mfg_id);
void               dsel_set_model_name(    Display_Selector* dsel, char* model_name);
void               dsel_set_sn(            Display_Selector* dsel, char * serial_ascii);
void               dsel_set_edid_bytes(    Display_Selector* dsel, Byte * edidbytes);
void               dsel_set_edid_hex(      Display_Selector* dsel, char * hexstring);
bool               dsel_validate(          Display_Selector* dsel);
#endif


// *** Display_Ref ***

extern bool ddc_never_uses_null_response_for_unsupported;
// extern bool ddc_always_uses_null_response_for_unsupported;

// Must be kept in sync with dref_flags_table
typedef uint16_t Dref_Flags;
#define DREF_DDC_COMMUNICATION_CHECKED                 0x0001
#define DREF_DDC_COMMUNICATION_WORKING                 0x0002
#define DREF_DDC_IS_MONITOR_CHECKED                    0x0004
#define DREF_DDC_IS_MONITOR                            0x0008

#define DREF_UNSUPPORTED_CHECKED                       0x0010
#define DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED    0x0020
#define DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED 0x0040
#define DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED         0x0080
#define DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED         0x0100

#define DREF_DYNAMIC_FEATURES_CHECKED                  0x0200
#define DREF_TRANSIENT                                 0x0400
#define DREF_OPEN                                      0x0800
#define DREF_DDC_BUSY                                  0x1000
#define DREF_DISCONNECTED                                   0x2000
#define DREF_DDC_DISABLED                              0x4000
#define DREF_DPMS_SUSPEND_STANDBY_OFF                  0x8000

char * interpret_dref_flags_t(Dref_Flags flags);

// define in ddcutil_types.h?, or perhaps use -1 for generic invalid, put type of invalid in Dref_Flags?
#define DISPNO_NOT_SET  0
#define DISPNO_INVALID -1
#define DISPNO_PHANTOM -2
#define DISPNO_REMOVED -3
#define DISPNO_BUSY    -4
#define DISPNO_DDC_DISABLED -5

#define DISPLAY_REF_MARKER "DREF"
/** A **Display_Ref** is a logical display identifier.
 * It can contain an I2C bus number or a USB bus number/device number pair.
 */
typedef struct _display_ref {
   char                     marker[4];
   uint                     dref_id;
   DDCA_IO_Path             io_path;
   int                      usb_bus;
   int                      usb_device;
   char *                   usb_hiddev_name;
   DDCA_MCCS_Version_Spec   vcp_version_xdf;
   DDCA_MCCS_Version_Spec   vcp_version_cmdline;
   Dref_Flags               flags;
   char *                   capabilities_string;   // added 4/2017, private copy
   Parsed_Edid *            pedid;                 // added 4/2017
   Monitor_Model_Key *      mmid;                  // will be set iff pedid
   int                      dispno;
   void *                   detail;                // I2C_Bus_Info or Usb_Monitor_Info
   Dynamic_Features_Rec *   dfr;                   // user defined feature metadata
   uint64_t                 next_i2c_io_after;     // nanosec
   struct _display_ref *    actual_display;        // if dispno == -2
   DDCA_IO_Path *           actual_display_path;   // alt to actual_display
   struct Per_Display_Data* pdd;
   char *                   drm_connector;         // e.g. card0-HDMI-A-1  // REDUNDANT - IDENTICAL TO Bus_Info.drm_connector
   int                      drm_connector_id;      // identical to Bus_Info.drm_connector_id
   Drm_Connector_Found_By   drm_connector_found_by;  // identical to Bus_Info.drm_connector_found_by
   char *                   communication_error_summary;
   uint64_t                 creation_timestamp;
   GMutex                   access_mutex;
   GMutex                   disconnect_mutex;
} Display_Ref;


void             dbgrpt_published_dref_hash(const char * msg, int depth);
void             published_dref_hash_to_syslog(int priority, const char * msg);
void             init_published_dref_hash();
void             reset_published_dref_hash();
void             add_published_dref_id_by_dref(Display_Ref * dref);
Display_Ref *    dref_from_published_ddca_dref(DDCA_Display_Ref ddca_dref);
DDCA_Display_Ref dref_to_ddca_dref(Display_Ref * dref);

#define DREF_BUSNO(_dref) ((_dref)->io_path.path.i2c_busno)
#define ASSERT_DREF_IO_MODE(_dref, _mode)  \
   assert(_dref && \
          memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0) && \
          _dref->io_path.io_mode == _mode)

Display_Ref * create_base_display_ref(DDCA_IO_Path io_path);
Display_Ref * create_bus_display_ref(int busno);
Display_Ref * create_usb_display_ref(int bus, int device, char * hiddev_devname);
void          dbgrpt_display_ref(Display_Ref * dref, bool include_businfo, int depth);
void          dbgrpt_display_ref0(Display_Ref * dref, int depth);
void          dbgrpt_display_ref_summary(Display_Ref * dref, bool include_businfo, int depth);
char *        dref_short_name_t(Display_Ref * dref);
char *        dref_repr_t(Display_Ref * dref);  // value valid until next call
char *        dref_reprx_t(Display_Ref * dref);  // value valid until next call
char *        ddci_dref_repr_t(DDCA_Display_Ref * ddca_dref);  // value valid until next call
DDCA_Status   free_display_ref(Display_Ref * dref);
Display_Ref * copy_display_ref(Display_Ref * dref);
DDCA_Status   dref_lock(Display_Ref * dref);
void          dref_unlock(Display_Ref * dref);

// Do two Display_Ref's identify the same device?
bool          dref_eq(Display_Ref* this, Display_Ref* that);

const char *  dref_get_i2c_driver(Display_Ref* dref);

#ifdef UNUSED
bool dref_set_alive(Display_Ref * dref, bool alive);
bool dref_get_alive(Display_Ref * dref);
#endif

Display_Ref* get_dref_by_busno_or_connector(int busno, const char * connector, bool ignore_invalid);
#define      GET_DREF_BY_BUSNO(_busno, _ignore) \
             get_dref_by_busno_or_connector(_busno,NULL, (_ignore))
#define      GET_DREF_BY_CONNECTOR(_connector_name, _ignore_invalid) \
             get_dref_by_busno_or_connector(-1, _connector_name, _ignore_invalid)

// *** Display_Handle ***

#define DISPLAY_HANDLE_MARKER "DSPH"
/** Describes an open display device. */
typedef struct {
   char         marker[4];
   Display_Ref* dref;
   int          fd;     // file descriptor
   char *       repr;
   char *       repr_p;
   bool         testing_unsupported_feature_active;
} Display_Handle;

Display_Handle * create_base_display_handle(int fd, Display_Ref * dref);
void             dbgrpt_display_handle(Display_Handle * dh, const char * msg, int depth);
char *           dh_repr(Display_Handle * dh);
char *           dh_repr_p(Display_Handle * dh);
void             free_display_handle(Display_Handle * dh);

// For internal display selection functions

#define DISPSEL_NONE       0x00
#define DISPSEL_VALID_ONLY 0x80
#ifdef FUTURE
#define DISPSEL_I2C        0x40
#define DISPSEL_USB        0x10
#define DISPSEL_ANY        (DISPSEL_I2C | DISPSEL_USB)
#endif

//* Option flags for display selection functions */
typedef Byte Display_Selection_Options;

int    hiddev_name_to_number(const char * hiddev_name);
#ifdef UNUSED
char * hiddev_number_to_name(int hiddev_number);
#endif

/** For recording /dev/i2c and hiddev open errors */
typedef struct {
   DDCA_IO_Mode io_mode;
   int    devno;   // i2c bus number or hiddev device number
   int    error;
   char * detail;
} Bus_Open_Error;

void free_bus_open_error(Bus_Open_Error * boe);

typedef enum {
   Watch_Mode_Dynamic,
   Watch_Mode_Poll,
   Watch_Mode_Xevent,
   Watch_Mode_Udev,
} DDC_Watch_Mode;

const char * watch_mode_name(DDC_Watch_Mode mode);

bool ignore_mmk(Monitor_Model_Key * mmk);
bool ignore_mmk_by_string(const char * mmid);
void dbgrpt_ignored_mmk_table(int depth);
bool is_ignored_mmk(Monitor_Model_Key mmk);

#endif /* DISPLAYS_H_ */
