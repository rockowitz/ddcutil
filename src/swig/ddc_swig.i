%module ddc_swig

%header %{
// Start of copy block
#include "swig/ddc_swig.h"
void ddct_init(void);
bool ddcs_built_with_adl(void);
const char * ddcutil_version(void);
// enum Retries{DDCT_WRITE_ONLY_TRIES3, DDCT_WRITE_READ_TRIES3, DDCT_MULTI_PART_TRIES3};
// end of copy block
%}

%wrapper %{

%}

%init %{
ddct_init();
%}

//%rename("%(regex:/(.*)_swigconstant$/\\1/)s") "";


%exception {
  clear_exception();     // redundant
  $action
  char * emsg = check_exception(); 
  if (emsg) {
     PyErr_SetString( PyExc_RuntimeError, emsg);
     return NULL;
  }
}


//
// General
//

void ddct_init(void);

bool ddcs_built_with_adl(void);
 
const char * ddcutil_version(void);

typedef int DDCT_Status;    // for now
// need to handle illegal status_code 
char * ddct_status_code_name(DDCT_Status status_code);
char * ddct_status_code_desc(DDCT_Status status_code);

// DDC Retry Control
typedef enum{DDCT_WRITE_ONLY_TRIES, DDCT_WRITE_READ_TRIES, DDCT_MULTI_PART_TRIES} DDCT_Retry_Type;
int  ddct_get_max_tries(DDCT_Retry_Type retry_type);
DDCT_Status  ddct_set_max_tries(DDCT_Retry_Type retry_type, int max_tries);


//
// Message Control
//

typedef enum {DDCT_OL_DEFAULT=0x01,
              DDCT_OL_PROGRAM=0x02,
              DDCT_OL_TERSE  =0x04,
              DDCT_OL_NORMAL =0x08,
              DDCT_OL_VERBOSE=0x10
} DDCT_Output_Level;

DDCT_Output_Level ddct_get_output_level();
void         ddct_set_output_level(DDCT_Output_Level newval);
char *       ddct_output_level_name(DDCT_Output_Level val);

void ddct_set_report_ddc_errors(bool onoff);
bool ddct_get_report_ddc_errors();

 void ddc_set_fout(void  *fpy);
 // void ddc_set_fout(PyFileObject *fpy);


//
// Reports
//

int ddcs_report_active_displays(int depth);


//
// VCP Feature Code Information
//

typedef int DDCS_VCP_Feature_Code;

typedef struct {
   int    major;
   int    minor;
} DDCS_MCCS_Version_Spec;

char *      ddcs_get_feature_name(DDCS_VCP_Feature_Code feature_code);

unsigned long ddcs_get_feature_info_by_vcp_version(
               DDCS_VCP_Feature_Code    feature_code, 
               DDCS_MCCS_Version_Spec   vspec);


//
// Display Identifiers
//

typedef void * DDCS_Display_Identifier_p;

DDCS_Display_Identifier_p ddcs_create_dispno_display_identifier(
               int dispno);
DDCS_Display_Identifier_p ddcs_create_adlno_display_identifier(
               int iAdapterIndex,
               int iDisplayIndex);
DDCS_Display_Identifier_p ddcs_create_busno_display_identifier(
               int busno);
DDCS_Display_Identifier_p ddcs_create_model_sn_display_identifier(
               const char * model,
               const char * sn);
DDCS_Display_Identifier_p ddcs_create_edid_display_identifier(
               const Byte * edid);
DDCS_Display_Identifier_p ddcs_create_usb_display_identifier(
               int bus,
               int device);
void ddcs_free_display_identifier(DDCS_Display_Identifier_p ddcs_did);
char * ddcs_repr_display_identifier(DDCS_Display_Identifier_p ddcs_did);


//
// Display References
//
typedef void * DDCS_Display_Ref_p;
DDCS_Display_Ref_p ddcs_get_display_ref(   DDCS_Display_Identifier_p did);
void               ddcs_free_display_ref(  DDCS_Display_Ref_p dref);
char *             ddcs_repr_display_ref(  DDCS_Display_Ref_p dref);
void               ddcs_report_display_ref(DDCS_Display_Ref_p dref, int depth);


//
// Display Handles
//

typedef void * DDCS_Display_Handle_p;
DDCS_Display_Handle_p ddcs_open_display(DDCS_Display_Ref_p dref);
void                  ddcs_close_display(DDCS_Display_Handle_p dh);
char *                ddcs_repr_display_handle(DDCS_Display_Handle_p dh);


//
// Miscellaneous Display Specific Functions
//

unsigned long ddcs_get_feature_info_by_display(
               DDCS_Display_Handle_p  dh,
               DDCS_VCP_Feature_Code  feature_code);

//
// Monitor Capabilities
//

char * ddcs_get_capabilities_string(DDCS_Display_Handle_p dh);


//
// Get and Set VCP Feature Values
//

typedef struct {
   Byte  mh;
   Byte  ml;
   Byte  sh;
   Byte  sl;
   int   max_value;
   int   cur_value;
} DDCS_Non_Table_Value_Response;


DDCS_Non_Table_Value_Response ddcs_get_nontable_vcp_value(
               DDCS_Display_Handle_p   dh,
               DDCS_VCP_Feature_Code   feature_code);

void ddcs_set_nontable_vcp_value(
               DDCS_Display_Handle_p   dh,
               DDCS_VCP_Feature_Code   feature_code,
               int                     new_value);

char * ddcs_get_profile_related_values(DDCS_Display_Handle_p dh);

void ddcs_set_profile_related_values(char * profile_values_string);

 