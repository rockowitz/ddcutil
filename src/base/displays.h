/* displays.h
 *
 * Display specification
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

/** \file
 * Display specification
 */

#ifndef DISPLAYS_H_
#define DISPLAYS_H_

/** \cond **/
#include <glib.h>
#include <stdbool.h>

#include "util/coredefs.h"
#include "util/edid.h"
/** \endcond */

#include "../public/ddcutil_types.h"

#include "core.h"
#include "feature_sets.h"
#include "vcp_version.h"

typedef void * Global_Display_Lock;

/** \file
Display Specification

Monitors are specified in different ways in different contexts:

1) Display_Identifier contains the identifiers specified on the command line.

2) Display_Ref is a logical display identifier.   It can be an I2C identifier,
an ADL identifier, or a USB identifier.

For Display_Identifiers containing either busno (for I2C) or ADL
adapter.display numbers the translation from Display_Identier to Display_Ref
is direct.   Otherwise, displays are searched to find the monitor.

3) Display_Handle is passed as an argument to "open" displays.

For ADL displays, the translation from Display_Ref to Display_Handle is direct.
For I2C displays, the device must be opened.  Display_Handle then contains the open file handle.
*/


// *** Display_Identifier ***

/** Display_Identifier type */
typedef enum {
   DISP_ID_BUSNO,      ///< /dev/i2c bus number
   DISP_ID_ADL,        ///< ADL iAdapterIndex/iDisplayIndex pair
   DISP_ID_MONSER,     ///< monitor mfg id, model name, and/or serial number
   DISP_ID_EDID,       ///< 128 byte EDID
   DISP_ID_DISPNO,     ///< ddcutil assigned sisplay number
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
   int             iAdapterIndex;
   int             iDisplayIndex;
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
Display_Identifier* create_adlno_display_identifier(int iAdapterIndex, int iDisplayIndex);
Display_Identifier* create_edid_display_identifier(const Byte* edidbytes);
Display_Identifier* create_mfg_model_sn_display_identifier(const char* mfg_code, const char* model_name, const char* serial_ascii);
Display_Identifier* create_usb_display_identifier(int bus, int device);
Display_Identifier* create_usb_hiddev_display_identifier(int hiddev_devno);
char *              did_repr(Display_Identifier * pdid);
void                dbgrpt_display_identifier(Display_Identifier * pdid, int depth);
void                free_display_identifier(Display_Identifier * pdid);

#ifdef FUTURE
// new way
#define DISPLAY_SELECTOR_MARKER "DSEL"
typedef struct {
   char            marker[4];         // always "DSEL"
   int             dispno;
   int             busno;
   int             iAdapterIndex;
   int             iDisplayIndex;
   char *          mfg_id;
   char *          model_name;
   char *          serial_ascii;
   int             usb_bus;
   int             usb_device;
   Byte *          edidbytes;   // always 128 bytes
} Display_Selector;

Display_Selector * dsel_new();
void               dsel_free(              Display_Selector * dsel);
Display_Selector * dsel_set_display_number(Display_Selector* dsel, int dispno);
Display_Selector * dsel_set_i2c_busno(     Display_Selector* dsel, int busno);
Display_Selector * dsel_set_adl_numbers(   Display_Selector* dsel, int iAdapterIndex, int iDisplayIndex);
Display_Selector * dsel_set_usb_numbers(   Display_Selector* dsel, int bus, int device);
Display_Selector * dsel_set_mfg_id(        Display_Selector* dsel, char*  mfg_id);
Display_Selector * dsel_set_model_name(    Display_Selector* dsel, char* model_name);
Display_Selector * dsel_set_sn(            Display_Selector* dsel, char * serial_ascii);
Display_Selector * dsel_set_edid_bytes(    Display_Selector* dsel, Byte * edidbytes);
Display_Selector * dsel_set_edid_hex(      Display_Selector* dsel, char * hexstring);
bool               dsel_validate(          Display_Selector * dsel);
#endif

// *** DDCA_Display_Path ***

char *  io_mode_name(DDCA_IO_Mode val);
bool    dpath_eq(DDCA_IO_Path p1, DDCA_IO_Path p2);
char *  dpath_repr_t(DDCA_IO_Path * dpath);  // value valid until next call



// *** Display_Ref ***

typedef Byte Dref_Flags;
#define DREF_DDC_COMMUNICATION_CHECKED              0x80
#define DREF_DDC_COMMUNICATION_WORKING              0x40
#define DREF_DDC_NULL_RESPONSE_CHECKED              0x20
#define DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED 0x10
#define DREF_DDC_IS_MONITOR_CHECKED                 0x08
#define DREF_DDC_IS_MONITOR                         0x04
#define DREF_TRANSIENT                              0x02

#define DISPLAY_REF_MARKER "DREF"
/** A **Display_Ref** is a logical display identifier.
 * It can contain an I2C bus number, and ADL adapter/display number pair,
 * or a USB bus number/device number pair.
 */
typedef struct _display_ref {
   char          marker[4];

#ifdef OLD
   DDCA_IO_Mode  io_mode;
   int           busno;
   int           iAdapterIndex;
   int           iDisplayIndex;
   int           usb_hiddev_devno;       // added 4/2017
#endif
   // alt:
   DDCA_IO_Path  io_path;

   int           usb_bus;
   int           usb_device;
   char *        usb_hiddev_name;
   DDCA_MCCS_Version_Spec vcp_version;
   Dref_Flags    flags;

   char *        capabilities_string;    // added 4/2017, private copy
   Parsed_Edid * pedid;                  // added 4/2017
   Global_Display_Lock gdl;

   // for merger with Display_Rec:
   int           dispno;
   void *        detail2;

   // for future request queue structure
   GQueue *      request_queue;
   GMutex        request_queue_lock;
} Display_Ref;

// n. works for both Display_Ref and Display_Handle
// #define ASSERT_DISPLAY_IO_MODE(_dref, _mode) assert(_dref && _dref->io_mode == _mode)

Display_Ref * create_bus_display_ref(int busno);
Display_Ref * create_adl_display_ref(int iAdapterIndex, int iDisplayIndex);
Display_Ref * create_usb_display_ref(int bus, int device, char * hiddev_devname);
void          dbgrpt_display_ref(Display_Ref * dref, int depth);
char *        dref_short_name_t(Display_Ref * dref);
char *        dref_repr_t(Display_Ref * dref);  // value valid until next call
Display_Ref * clone_display_ref(Display_Ref * old);
void          free_display_ref(Display_Ref * dref);

// Do two Display_Ref's identify the same device?
bool dref_eq(Display_Ref* this, Display_Ref* that);

// n. returned on stack
// DDCA_IO_Path dpath_from_dref(Display_Ref * dref);


// *** Display_Handle ***

#define DISPLAY_HANDLE_MARKER "DSPH"
/** Describes an open display device. */
typedef struct {
   char         marker[4];
   Display_Ref* dref;
   int          fh;     // file handle if ddc_io_mode == DDC_IO_DEVI2C or USB_IO                           // added 7/2016
   char *       repr;
} Display_Handle;

Display_Handle * create_bus_display_handle_from_display_ref(int fh, Display_Ref * dref);
Display_Handle * create_adl_display_handle_from_display_ref(Display_Ref * dref);
Display_Handle * create_usb_display_handle_from_display_ref(int fh, Display_Ref * dref);
void             report_display_handle(Display_Handle * dh, const char * msg, int depth);
char *           dh_repr_a(Display_Handle * dh);
char *           dh_repr(Display_Handle * dh);
char *           dh_repr_t(Display_Handle * dh);
void             free_display_handle(Display_Handle * dh);


// *** Video_Card_Info ***

#define VIDEO_CARD_INFO_MARKER "VIDC"
/** Video card information */
typedef struct {
   char     marker[4];
   int      vendor_id;
   char *   adapter_name;
   char *   driver_name;
} Video_Card_Info;

Video_Card_Info * create_video_card_info();


// *** Miscellaneous ***

bool is_adlno_defined(DDCA_Adlno adlno);

/** Reserved #DDCA_Adlno value indicating undefined */
#define ADLNO_UNDEFINED {-1,-1}

// For internal display selection functions

#define DISPSEL_NONE       0x00
#define DISPSEL_VALID_ONLY 0x80
#ifdef FUTURE
#define DISPSEL_I2C        0x40
#define DISPSEL_ADL        0x20
#define DISPSEL_USB        0x10
#define DISPSEL_ANY        (DISPSEL_I2C | DISPSEL_ADL | DISPSEL_USB)
#endif

//* Option flags for display selection functions */
typedef Byte Display_Selection_Options;

int    hiddev_name_to_number(char * hiddev_name);
char * hiddev_number_to_name(int hiddev_number);


Global_Display_Lock get_display_lock(Display_Ref * dref);
bool lock_display_lock(Global_Display_Lock dlock, bool wait);
void unlock_display_lock(Global_Display_Lock dlock);

#endif /* DISPLAYS_H_ */
