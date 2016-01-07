/* vcp_base.h
 *
 * Created on: Jan 7, 2016
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef SRC_BASE_VCP_FEATURE_BASE_H_
#define SRC_BASE_VCP_FEATURE_BASE_H_

#include "util/coredefs.h"

// Standard printf format strings for reporting feature codes values.
extern const char* FMT_CODE_NAME_DETAIL_W_NL;
extern const char* FMT_CODE_NAME_DETAIL_WO_NL;


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
   VCP_SUBSET_SINGLE_FEATURE  = 0x0001,
   VCP_SUBSET_NONE            = 0x0000,
} VCP_Feature_Subset;

char * feature_subset_name(VCP_Feature_Subset subset_id);


typedef struct {
   VCP_Feature_Subset  subset;
   Byte                specific_feature;
} Feature_Set_Ref;

void report_feature_set_ref(Feature_Set_Ref * fsref, int depth);


#endif /* SRC_BASE_VCP_FEATURE_BASE_H_ */
