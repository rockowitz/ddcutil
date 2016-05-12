/* ddc_base.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDC_BASE_H_
#define DDC_BASE_H_

#include <stdbool.h>

#include "util/coredefs.h"

// Standard printf format strings for reporting feature codes values.
extern const char* FMT_CODE_NAME_DETAIL_W_NL;
extern const char* FMT_CODE_NAME_DETAIL_WO_NL;


typedef struct {
    Byte  major;
    Byte  minor;
} Version_Spec;

extern const Version_Spec VCP_SPEC_V20;
extern const Version_Spec VCP_SPEC_V21;
extern const Version_Spec VCP_SPEC_V30;
extern const Version_Spec VCP_SPEC_V22;

bool vcp_version_le(Version_Spec val, Version_Spec max);
bool vcp_version_gt(Version_Spec val, Version_Spec min);


// typedef enum {I2C_IO_STRATEGY_FILEIO, I2C_IO_STRATEGY_IOCTL} I2C_IO_Strategy_Id;



// If this enum is changed, be sure to change the corresponding
// table in vcp_feature_base.c
typedef enum {
   VCP_SUBSET_PROFILE         = 0x8000,
   VCP_SUBSET_COLOR           = 0x4000,
   VCP_SUBSET_LUT             = 0x2000,
   VCP_SUBSET_CRT             = 0x1000,
   VCP_SUBSET_TV              = 0x0800,
   VCP_SUBSET_AUDIO           = 0x0400,
   VCP_SUBSET_WINDOW          = 0x0200,
   VCP_SUBSET_DPVL            = 0x0100,

   // subsets used only on commands processing,
   // not in feature descriptor table
   VCP_SUBSET_SCAN            = 0x0080,
   VCP_SUBSET_ALL             = 0x0040,
   VCP_SUBSET_SUPPORTED       = 0x0020,
   VCP_SUBSET_KNOWN           = 0x0010,
   VCP_SUBSET_PRESET          = 0x0008,    // uses VCP_SPEC_PRESET
   VCP_SUBSET_MFG             = 0x0004,    // mfg specific codes
   VCP_SUBSET_TABLE           = 0x0002,    // is a table feature
   VCP_SUBSET_SINGLE_FEATURE  = 0x0001,
   VCP_SUBSET_NONE            = 0x0000,
} VCP_Feature_Subset;


typedef
struct _Vcp_Subset_Desc {
   VCP_Feature_Subset     subset_id;
   char *                 subset_id_name;
   char *                 public_name;
} Vcp_Subset_Desc;


extern struct _Vcp_Subset_Desc vcp_subset_desc[];
const int vcp_subset_count;

char * feature_subset_name(VCP_Feature_Subset subset_id);


typedef struct {
   VCP_Feature_Subset  subset;
   Byte                specific_feature;
} Feature_Set_Ref;

void report_feature_set_ref(Feature_Set_Ref * fsref, int depth);


#endif /* DDC_BASE_H_ */
