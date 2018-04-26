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



#endif /* DDCUTIL_C_TYPES_PRIVATE_H_ */
