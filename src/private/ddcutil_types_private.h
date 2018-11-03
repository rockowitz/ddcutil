/* ddcutil_types_private.h
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

#ifndef DDCUTIL_C_TYPES_PRIVATE_H_
#define DDCUTIL_C_TYPES_PRIVATE_H_


#include "public/ddcutil_types.h"     // for #defines


#ifdef UNUSED
/** #DDCA_Vcp_Value_Type_Parm extends #DDCA_Vcp_Value_Type to allow for its use as a
    function call parameter where the type is unknown */
typedef enum {S
   DDCA_UNSET_VCP_VALUE_TYPE_PARM = 0,   /**< Unspecified */
   DDCA_NON_TABLE_VCP_VALUE_PARM  = 1,   /**< Continuous (C) or Non-Continuous (NC) value */
   DDCA_TABLE_VCP_VALUE_PARM      = 2,   /**< Table (T) value */
} DDCA_Vcp_Value_Type_Parm;
#endif


#ifdef FUTURE
// Possible future declarations for exposing formatting functions, to be passed
// in DDCA_Version_Feature_Info.
// Issue: Won't expose well in Python API

char * (*DDCA_Func_Format_Non_Table_Value) (
          DDCA_Vcp_Feature_Code     feature_code,
          DDCA_MCCS_Version_Spec    vspec,
          DDCA_Non_Table_Vcp_Value  valrec);     // or pointer?

char * (*DDCA_Func_Format_Table_Value) (
          DDCA_Vcp_Feature_Code     feature_code,
          DDCA_MCCS_Version_Spec    vspec,
          DDCA_Table_Vcp_Value      valrec);     // or pointer?

char * (*DDCA_Func_Format_Any_Value) (
          DDCA_Vcp_Feature_Code     feature_code,
          DDCA_MCCS_Version_Spec    vspec,
          DDCA_Any_Vcp_Value        valrec);     // or pointer?
#endif




// #define MONITOR_MODEL_KEY_MARKER "MMID"
/** Identifies a monitor model */
typedef struct {
// char                marker[4];
   char                mfg_id[DDCA_EDID_MFG_ID_FIELD_SIZE];
   char                model_name[DDCA_EDID_MODEL_NAME_FIELD_SIZE];
   uint16_t            product_code;
   bool                defined;
} DDCA_Monitor_Model_Key;


// Experimental async access - used in Python API

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





#endif /* DDCUTIL_C_TYPES_PRIVATE_H_ */
