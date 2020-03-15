
// From ddcutil_c_api.h
// Must be manually kept in sync!

// Library Build Information
DDCA_Ddcutil_Version_Spec ddca_ddcutil_version(void);  
const char * ddca_ddcutil_version_string(void);
uint8_t      ddca_build_options(void);
int          ddca_max_max_tries();

// Status Codes
char * ddca_rc_name(DDCA_Status status_code);
char * ddca_rc_desc(DDCA_Status status_code);

// Global Settings
int ddca_get_max_tries(Retry_Operation retry_type);
DDCA_Status ddca_set_max_tries(
               Retry_Operation retry_type,
                int             max_tries);
void ddca_enable_verify(bool onoff);
bool ddca_is_verify_enabled(void);

// Message Control

void
ddca_set_fout(
      FILE * fout);   /**< where to write normal messages, if NULL, suppress  */

void
ddca_set_fout_to_default(void);

void
ddca_set_ferr(
      FILE * ferr);   /**< where to write error messages, If NULL, suppress */

void
ddca_set_ferr_to_default(void);

DDCA_Output_Level                   /**< current output level */
ddca_get_output_level(void);

void
ddca_set_output_level(
      DDCA_Output_Level newval);   /**< new output level */

char *
ddca_output_level_name(
      DDCA_Output_Level val);     /**< output level id */

void
ddca_enable_report_ddc_errors(
      bool onoff);

bool
ddca_is_report_ddc_errors_enabled(void);

// Statistics
void ddca_reset_stats(void);
void ddca_show_stats(DDCA_Stats_Type stats, int depth);

// Display Descriptions

DDCA_Display_Info_List *
ddca_get_display_info_list(void);

void ddca_free_display_info_list(DDCA_Display_Info_List * dlist);

void
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth);

void
ddca_report_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth);

int
ddca_report_active_displays(
      int depth);

// Display Identifier

DDCA_Status
ddca_create_dispno_display_identifier(
      int                      dispno,
      DDCA_Display_Identifier* pdid);


DDCA_Status
ddca_create_busno_display_identifier(
      int                      busno,
      DDCA_Display_Identifier* pdid);


DDCA_Status
ddca_create_adlno_display_identifier(
      int                      iAdapterIndex,
      int                      iDisplayIndex,
      DDCA_Display_Identifier* pdid);


DDCA_Status
ddca_create_mfg_model_sn_display_identifier(
      const char *             mfg_id,
      const char *             model,
      const char *             sn,
      DDCA_Display_Identifier* pdid);


DDCA_Status
ddca_create_edid_display_identifier(
      const uint8_t*            edid,
      DDCA_Display_Identifier * pdid);      // 128 byte edid


DDCA_Status
ddca_create_usb_display_identifier(
      int                      bus,
      int                      device,
      DDCA_Display_Identifier* pdid);

DDCA_Status
ddca_create_usb_hiddev_display_identifier(
      int                      hiddev_devno,
      DDCA_Display_Identifier* pdid);

DDCA_Status
ddca_free_display_identifier(
      DDCA_Display_Identifier did);

char *
ddca_did_repr(
      DDCA_Display_Identifier did);

// Display Reference

DDCA_Status
ddca_create_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       pdref);

DDCA_Status
ddca_free_display_ref(
      DDCA_Display_Ref      dref);

char *
ddca_dref_repr(
      DDCA_Display_Ref      dref);

void
ddca_dbgrpt_display_ref(
      DDCA_Display_Ref      dref,
      int                   depth);


// Display Handle

DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * p_ddca_dh);

DDCA_Status
ddca_close_display(
      DDCA_Display_Handle   ddca_dh);

char *
ddca_dh_repr(
      DDCA_Display_Handle   ddca_dh);


// MCCS Version

DDCA_Status
ddca_get_mccs_version(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec* pspec);

DDCA_Status
ddca_get_mccs_version_id(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Id*   p_id);

char *
ddca_mccs_version_id_name(
      DDCA_MCCS_Version_Id    version_id);

char *
ddca_mccs_version_id_desc(
      DDCA_MCCS_Version_Id    version_id);


// Monitor Capabilities

DDCA_Status
ddca_get_capabilities_string(
      DDCA_Display_Handle     ddca_dh,
      char**                  p_capabiltities_string);

DDCA_Status
ddca_parse_capabilities_string(
      char *                  capabilities_string,
      DDCA_Capabilities **    p_parsed_capabilities);

void
ddca_free_parsed_capabilities(
      DDCA_Capabilities *     parsed_capabilities);

void
ddca_report_parsed_capabilities(
      DDCA_Capabilities *     parsed_capabilities,
      int                     depth);


// VCP Feature Information, Monitor Independent
//

DDCA_Status
ddca_get_feature_info_by_vcp_version(
      DDCA_Vcp_Feature_Code         feature_code,
   // DDCT_MCCS_Version_Spec        vspec,
      DDCA_MCCS_Version_Id          mccs_version_id,
      DDCA_Version_Feature_Info**   p_info);

char *
ddca_get_feature_name(DDCA_Vcp_Feature_Code feature_code);

DDCA_Status
ddca_get_simple_sl_value_table(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Id        mccs_version_id,
      DDCA_Feature_Value_Table *  p_value_table);   // DDCA_Feature_Value_Entry **

DDCA_Status
ddca_get_simple_nc_feature_value_name(
      DDCA_Display_Handle    ddca_dh,    // needed because value lookup mccs version dependent
      DDCA_Vcp_Feature_Code  feature_code,
      uint8_t                feature_value,
      char**                 p_feature_name);

// Feature Information, Monitor Dependent

DDCA_Status
ddca_get_feature_info_by_display(
      DDCA_Display_Handle           ddca_dh,
      DDCA_Vcp_Feature_Code         feature_code,
      DDCA_Version_Feature_Info **  p_info);

DDCA_Status
ddca_free_feature_info(
      DDCA_Version_Feature_Info * info);

//  Miscellaneous Monitor Specific Functions

DDCA_Status
ddca_get_edid_by_display_ref(
      DDCA_Display_Ref ddca_dref,
      uint8_t **       pbytes);   // pointer into ddcutil data structures, do not free


// Get/Set VCP Feature Values

// void
// ddca_free_table_value_response(
//       DDCA_Table_Value * table_value_response);

// TODO: Choose between ddca_get_nontable_vcp_value()/ddca_get_table_vcp_value() vs ddca_get_vcp_value()

//DDCA_Status
//ddca_get_nontable_vcp_value(
//       DDCA_Display_Handle             ddca_dh,
//       DDCA_Vcp_Feature_Code           feature_code,
//       DDCA_Non_Table_Value_Response * response);
//
//
//DDCA_Status
//ddca_get_table_vcp_value(
//       DDCA_Display_Handle     ddca_dh,
//       DDCA_Vcp_Feature_Code   feature_code,
//       int *                   value_len,
//       uint8_t**               value_bytes);


DDCA_Status
ddca_get_any_vcp_value_using_explicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type_Parm    call_type,
       DDCA_Any_Vcp_Value **       valrec_loc);

DDCA_Status
ddca_get_formatted_vcp_value(
       DDCA_Display_Handle *       ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       char**                      p_formatted_value);


DDCA_Status
ddca_set_continuous_vcp_value(
      DDCA_Display_Handle   ddca_dh,
      DDCA_Vcp_Feature_Code feature_code,
      uint16_t              new_value);


DDCA_Status
ddca_set_simple_nc_vcp_value(
      DDCA_Display_Handle          ddca_dh,
      DDCA_Vcp_Feature_Code        feature_code,
      uint8_t                      new_value);

DDCA_Status
ddca_set_non_table_vcp_value(
      DDCA_Display_Handle    ddca_dh,
      DDCA_Vcp_Feature_Code  feature_code,
      uint8_t                   hi_byte,
      uint8_t                   lo_byte);



// Unimplemented
//DDCA_Status
//ddct_set_table_vcp_value(
//      DDCA_Display_Handle  ddct_dh,
//      DDCA_Vcp_Feature_Code     feature_code,
//      int                  value_len,
//      uint8_t*             value_bytes);

DDCA_Status
ddca_get_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char**               pprofile_values_string);

DDCA_Status
ddca_set_profile_related_values(char *
      profile_values_string);
