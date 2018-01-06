/* feature_sets.h
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

/** \file
 *  Feature set identifiers
 */

#ifndef FEATURE_SETS_H_
#define FEATURE_SETS_H_

/** \cond */
#include <stdbool.h>

#include "util/coredefs.h"
/** \endcond */


// If this enum is changed, be sure to change the corresponding
// table in feature_sets.c
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

char * feature_subset_name(VCP_Feature_Subset subset_id);
char * feature_subset_names(VCP_Feature_Subset subset_ids);

typedef struct {
   VCP_Feature_Subset  subset;
   Byte                specific_feature;
} Feature_Set_Ref;

void dbgrpt_feature_set_ref(Feature_Set_Ref * fsref, int depth);

#endif /* FEATURE_SETS_H_ */
