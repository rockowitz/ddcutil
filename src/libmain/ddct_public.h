/*  ddct_public.h
 *
 *  Created on: Nov 30, 2015
 *      Author: rock
 *
 *  Initial cut at a public library API.   Currently unimplemented.
 */

#ifndef DDCT_PUBLIC_H_
#define DDCT_PUBLIC_H_


#include "base/ddc_base_defs.h"
#include "base/displays.h"
// #include "base/ddc_packets.h"      // temp, for Interpreted_Vcp_Code

typedef struct {
   int    major;
   int    minor;
   int    build;
} DDCT_Version_Spec;


typedef struct {
   Byte  mh;
   Byte  ml;
   Byte  sh;
   Byte  sl;
   int   max_value;
   int   cur_value;
} DDCT_Non_Table_Value_Response;


typedef struct {
   int   bytect;
   Byte  bytes[];     // or Byte * ?
} DDCT_Table_Value_Response;

typedef void * DDCT_Display_Identifier;

void ddct_free_table_value_response(DDCT_Table_Value_Response * table_value_response);


typedef int DDCT_Status;    // for now

char * ddct_status_code_name(DDCT_Status status_code);
char * ddct_status_code_desc(DDCT_Status status_code);

typedef Byte VCP_Feature_Code;

DDCT_Version_Spec ddct_get_version();       // ddctool version

bool ddct_built_with_adl();
#define  DDCT_BUILT_WITH_ADL  0x01
unsigned long get_ddct_global_flags();
// or: more generalizable: return a byte of flags, one of which is DDCT_SUPPORTS_ADL


// Get and set max retries
// Get and set timeouts

typedef enum{DDCT_WRITE_ONLY_TRIES, DDCT_WRITE_READ_TRIES, DDCT_MULTIPART_TRIES} DDCT_Retry_Type;

int  ddct_get_max_tries(DDCT_Retry_Type retry_type);
void ddct_set_max_tries(DDCT_Retry_Type retry_type, int max_tries);

typedef enum{DDCT_TIMEOUT_STANDARD, DDCT_TIMEOUT_TABLE_RETRY} DDCT_Timeout_Type;
int ddct_get_timeout_millis(DDCT_Timeout_Type timeout_type);
void ddct_set_timeout_millis(DDCT_Timeout_Type timeout_type, int millisec);


Display_Info_List ddct_get_displays();


// if want Display_Identifier to be opaque, need to return pointer
#ifdef NO
Display_Identifier create_dispno_display_indentifier(int dispno);
Display_Identifier create_bus_display_identifier(int busno);
Display_Identifier create_adl_display_identifier(int iAdapterIndex, int iDisplayIndex);
Display_Identifier create_model_sn_display_identifier(char * model, char * sn);
#endif

DDCT_Status ddct_create_dispno_display_identifier(
               int dispno,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_busno_display_identifier(
               int busno,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_model_sn_display_identifier(
               char * model,
               char * sn,
               DDCT_Display_Identifier* pdid);
DDCT_Status ddct_create_edid_display_identifier(
               Byte * edid,
               Display_Identifier * pdid);      // 128 byte edid
DDCT_Status ddct_free_display_identifier(DDCT_Display_Identifier ddct_did);


typedef void * DDCT_Display_Ref;
typedef void * DDCT_Display_Handle;

DDCT_Status ddct_get_display_ref(DDCT_Display_Identifier did, DDCT_Display_Ref* ddct_dref);

DDCT_Status ddct_open_display(DDCT_Display_Ref dref, DDCT_Display_Handle * pdh);

DDCT_Status ddct_close_display(DDCT_Display_Handle ddct_dh);

DDCT_Status ddct_get_mccs_version(DDCT_Display_Handle ddct_dh, Version_Spec* pspec);

DDCT_Status ddct_get_edid(Display_Handle * dh, Byte * edid_buffer);    // edid_buffer must be >= 128 bytes
DDCT_Status ddct_get_edid_by_display_ref(DDCT_Display_Ref ddct_dref, Byte ** pbytes);   // pointer into ddctool data structures, do not free

DDCT_Status ddct_get_nontable_vcp_value(
               DDCT_Display_Handle             ddct_dh,
               VCP_Feature_Code                feature_code,
               DDCT_Non_Table_Value_Response * response);

DDCT_Status ddct_get_table_vcp_value(
               DDCT_Display_Handle ddct_dh,
               VCP_Feature_Code    feature_code,
               int *               value_len,
               Byte**              value_bytes);

// flags for
// DDCT_VCP_CONTINUOUS
// DDCT_VCP_NONCONT
// DDCT_VCP_NONCONT_SL
// DDCT_VCP_TABLE

// or return a struct?
DDCT_Status ddct_get_feature_info(VCP_Feature_Code feature_code, unsigned long * flags);
char *      ddct_get_feature_name(VCP_Feature_Code feature_code);

typedef void * Feature_Value_Table;   // temp

DDCT_Status ddct_get_feature_sl_value_table(
               DDCT_Display_Handle   ddct_dh,
               VCP_Feature_Code      feature_code,
               Feature_Value_Table * value_table);

DDCT_Status ddct_set_nontable_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               int                  new_value);

DDCT_Status ddct_set_table_vcp_value(
               DDCT_Display_Handle  ddct_dh,
               VCP_Feature_Code     feature_code,
               int                  value_len,
               Byte *               value_bytes);

// caller allocate buffer, or should function?
DDCT_Status ddct_get_capabilities_string(DDCT_Display_Ref ddct_dref, char** buffer);

typedef void Parsed_Capabilities;    // TEMP

DDCT_Status ddct_parse_capabilities_string(char * capabilities_string, Parsed_Capabilities ** parsed_capabilities);


#endif /* DDCT_PUBLIC_H_ */
