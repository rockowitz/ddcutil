/*  displays.h
 *
 *  Created on: Jul 21, 2014
 *      Author: rock
 *
 *  Display specification
 */

#ifndef DISPLAYS_H_
#define DISPLAYS_H_

#include <stdbool.h>

#include <util/coredefs.h>

#include <base/ddc_base_defs.h>
#include <base/edid.h>
#include <base/util.h>


/*
Monitors are specified in different ways in different contexts:

1) Display_Identifier contains the identifiers specified on the command line.

2) Display_Ref is a logical display identifier.   It can be either an I2C identifier,
or an ADL identifier.

For Display_Identifiers containing either busno (for I2C) or ADL
adapter.display numbers the translation from Display_Identier to Display_Ref
is direct.   Otherwise, displays are searched to find the monitor.

3) Display_Handle is passed as an argument to "open" displays.

For ADL displays, the translation from Display_Ref to Display_Handle is direct.
For I2C displays, the device must be opened.  Display_Handle then contains the open file handle.
*/

// *** DisplayIdentifier ***

typedef enum {DISP_ID_BUSNO, DISP_ID_ADL, DISP_ID_MONSER, DISP_ID_EDID, DISP_ID_DISPNO} Display_Id_Type;
char * display_id_type_name(Display_Id_Type val);

#define DISPLAY_IDENTIFIER_MARKER "DPID"

typedef
struct {
   char            marker[4];         // always "DPID"
// int             ui_display_number; // is this appropriate here?
   Display_Id_Type id_type;
   int             dispno;
   int             busno;
   int             iAdapterIndex;
   int             iDisplayIndex;
// char            mfg_id[EDID_MFG_ID_FIELD_SIZE];   // not used
   char            model_name[EDID_MODEL_NAME_FIELD_SIZE];
   char            serial_ascii[EDID_SERIAL_ASCII_FIELD_SIZE];
   Byte            edidbytes[128];
} Display_Identifier;


Display_Identifier* create_dispno_display_identifier(int dispno);
Display_Identifier* create_busno_display_identifier(int busno);
Display_Identifier* create_adlno_display_identifier(int iAdapterIndex, int iDisplayIndex);
Display_Identifier* create_edid_display_identifier(Byte* edidbytes);
Display_Identifier* create_mon_ser_display_identifier(char* model_name, char* serial_ascii);
void                report_display_identifier(Display_Identifier * pdid, int depth);
void                free_display_identifier(Display_Identifier * pdid);

//  Display_Ref, potentially also Display_Handle:
bool is_version_unqueried(Version_Spec vspec);


// *** Display_Ref ***

typedef enum {DDC_IO_DEVI2C, DDC_IO_ADL} DDC_IO_Mode;
char * ddc_io_mode_name(DDC_IO_Mode val);

#define DISPLAY_REF_MARKER "DREF"
typedef
struct {
   char         marker[4];
   DDC_IO_Mode  ddc_io_mode;
   int          busno;
   int          iAdapterIndex;
   int          iDisplayIndex;
   Version_Spec vcp_version;
} Display_Ref;

#define ASSERT_VALID_DISPLAY_REF(dref, io_mode) assert(dref && dref->ddc_io_mode == io_mode)


Display_Ref * create_bus_display_ref(int busno);
Display_Ref * create_adl_display_ref(int iAdapterIndex, int iDisplayIndex);
void          report_display_ref(Display_Ref * dref, int depth);
char *        display_ref_short_name_r(Display_Ref * dref, char * buf, int bufsize);
char *        display_ref_short_name(Display_Ref * dref);  // value valid until next call
Display_Ref * clone_display_ref(Display_Ref * old);
void          free_display_ref(Display_Ref * dref);

// are two Display_Ref's equal?
bool dreq(Display_Ref* this, Display_Ref* that);


// *** Display_Handle ***

#define DISPLAY_HANDLE_MARKER "DSPH"
typedef
struct {
   char         marker[4];
   DDC_IO_Mode  ddc_io_mode;
   // include pointer to Display_Ref?
   int          busno;  // used for messages
   int          fh;     // file handle if ddc_io_mode == DDC_IO_DEVI2C
   int          iAdapterIndex;
   int          iDisplayIndex;
   Version_Spec vcp_version;
   char *       capabilities_string;
} Display_Handle;

Display_Handle * create_bus_display_handle(int fh, int busno);
Display_Handle * create_adl_display_handle(int iAdapterIndex, int iDisplayIndex);
Display_Handle * create_adl_display_handle_from_display_ref(Display_Ref * ref);
void report_display_handle(Display_Handle * dh, const char * msg);
char * display_handle_repr_r(Display_Handle * dh, char * buf, int bufsize);
char * display_handle_repr(Display_Handle * dh);


#define VIDEO_CARD_INFO_MARKER "VIDC"
typedef struct {
   char     marker[4];
   int      vendor_id;
   char *   adapter_name;
   char *   driver_name;
} Video_Card_Info;

Video_Card_Info * create_video_card_info();


// for surfacing display information at higher levels than i2c and adl, without creating
// circular dependencies
typedef struct {
   int           dispno;
   Display_Ref * dref;
   Parsed_Edid * edid;
} Display_Info;

typedef struct {
   int ct;
   Display_Info * info_recs;
} Display_Info_List;

void report_display_info(Display_Info * dinfo, int depth);
void report_display_info_list(Display_Info_List * pinfo_list, int depth);

#endif /* DISPLAYS_H_ */
