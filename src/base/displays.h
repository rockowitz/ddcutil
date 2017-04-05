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

#ifndef DISPLAYS_H_
#define DISPLAYS_H_

/** \cond **/
#include <stdbool.h>
/** \endcond */

#include "util/coredefs.h"
#include "util/edid.h"

#include "base/core.h"
#include "base/feature_sets.h"
#include "base/vcp_version.h"


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

// *** DisplayIdentifier ***

/** Display_Identifier type */
typedef enum {
   DISP_ID_BUSNO,
   DISP_ID_ADL,
   DISP_ID_MONSER,
   DISP_ID_EDID,
   DISP_ID_DISPNO,
   DISP_ID_USB
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
   Byte            edidbytes[128];
} Display_Identifier;


Display_Identifier* create_dispno_display_identifier(int dispno);
Display_Identifier* create_busno_display_identifier(int busno);
Display_Identifier* create_adlno_display_identifier(int iAdapterIndex, int iDisplayIndex);
Display_Identifier* create_edid_display_identifier(const Byte* edidbytes);
Display_Identifier* create_mfg_model_sn_display_identifier(const char* mfg_code, const char* model_name, const char* serial_ascii);
Display_Identifier* create_usb_display_identifier(int bus, int device);
void                report_display_identifier(Display_Identifier * pdid, int depth);
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



// *** Display_Ref ***

// moved MCCS_IO_Mode to ddcutil_types.h


char * mccs_io_mode_name(DDCA_IO_Mode val);

typedef Byte Dref_Flags;
#define DREF_DDC_COMMUNICATION_CHECKED 0x80
#define DREF_DDC_COMMUNICATION_WORKING 0x40
#define DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED 0x20
#define DISPLAY_REF_MARKER "DREF"
/** A **Display_Ref** is a logical display identifier.
 * It can be an I2C bus number, and ADL adapter/display number pair,
 * or a USB bus number/device number pair.
 */
typedef struct {
   char         marker[4];
   DDCA_IO_Mode io_mode;
   int          busno;
   int          iAdapterIndex;
   int          iDisplayIndex;
   int          usb_bus;
   int          usb_device;
   char *       usb_hiddev_name;
   DDCA_MCCS_Version_Spec vcp_version;
   Dref_Flags   flags;
} Display_Ref;

// n. works for both Display_Ref and Display_Handle
#define ASSERT_DISPLAY_IO_MODE(_dref, _mode) assert(_dref && _dref->io_mode == _mode)

Display_Ref * create_bus_display_ref(int busno);
Display_Ref * create_adl_display_ref(int iAdapterIndex, int iDisplayIndex);
Display_Ref * create_usb_display_ref(int bus, int device, char * hiddev_devname);
void          report_display_ref(Display_Ref * dref, int depth);
char *        dref_short_name_r(Display_Ref * dref, char * buf, int bufsize);
char *        dref_short_name(Display_Ref * dref);  // value valid until next call
char *        dref_repr(Display_Ref * dref);  // value valid until next call
Display_Ref * clone_display_ref(Display_Ref * old);
void          free_display_ref(Display_Ref * dref);

// are two Display_Ref's equal?
bool dreq(Display_Ref* this, Display_Ref* that);


// *** Display_Handle ***

// TODO: simplify, remove redundant fields for values obtainable from dref

#define DISPLAY_HANDLE_MARKER "DSPH"
/** Describes an open display device. */
typedef struct {
   char         marker[4];
   DDCA_IO_Mode io_mode;
   Display_Ref* dref;                               // added 4/2016
   int          busno;  // used for messages
   int          fh;     // file handle if ddc_io_mode == DDC_IO_DEVI2C or USB_IO
   int          iAdapterIndex;
   int          iDisplayIndex;
   int          usb_bus;
   int          usb_device;
   char *       hiddev_device_name;
   DDCA_MCCS_Version_Spec vcp_version;
   char *       capabilities_string;
   Parsed_Edid* pedid;                             // added 7/2016
} Display_Handle;

Display_Handle * create_bus_display_handle_from_display_ref(int fh, Display_Ref * dref);
Display_Handle * create_adl_display_handle_from_display_ref(Display_Ref * dref);
Display_Handle * create_usb_display_handle_from_display_ref(int fh, Display_Ref * dref);
void   report_display_handle(Display_Handle * dh, const char * msg, int depth);
char * display_handle_repr_r(Display_Handle * dh, char * buf, int bufsize);
char * display_handle_repr(Display_Handle * dh);

#define VIDEO_CARD_INFO_MARKER "VIDC"
/** Video card information */
typedef struct {
   char     marker[4];
   int      vendor_id;
   char *   adapter_name;
   char *   driver_name;
} Video_Card_Info;

Video_Card_Info * create_video_card_info();



#define DISPLAY_INFO_MARKER "DINF"
/**
 * For surfacing display information at higher levels than i2c and adl, without creating
 * circular dependencies
 */
typedef struct {
   char          marker[4];
   int           dispno;
   Display_Ref * dref;
   Parsed_Edid * edid;
} Display_Info;


// #define DISPLAY_INFO_LIST_MARKER "DINL"
/** List of #Display_Info */
typedef struct {
   // char           marker[4];   // complicates an otherwise simple initialization, not worth it
   int            ct;
   Display_Info * info_recs;      // n. this is a pointer to an array of Display_Info, not an array of Display_Info *
} Display_Info_List;

void report_display_info(Display_Info * dinfo, int depth);
void report_display_info_list(Display_Info_List * pinfo_list, int depth);

void free_display_info(Display_Info * dinfo);
void free_display_info_list(Display_Info_List * pinfo_list);


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

#endif /* DISPLAYS_H_ */
