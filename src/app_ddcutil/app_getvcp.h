/* app_getvcp.h
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

/** \file app_getvcp.h
 */

#ifndef APP_GETVCP_H_
#define APP_GETVCP_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "base/displays.h"
#include "base/feature_sets.h"
#include "base/status_code_mgt.h"


Public_Status_Code
app_show_single_vcp_value_by_feature_table_entry(
      Display_Handle *          dh,
      VCP_Feature_Table_Entry * entry);

#ifdef PRE_UDF
Public_Status_Code
app_show_single_vcp_value_by_feature_id(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force);
#endif

Public_Status_Code
app_show_single_vcp_value_by_feature_id_new(
      Display_Handle *      dh,
      DDCA_Vcp_Feature_Code feature_id,
      bool                  force);

Public_Status_Code
app_show_vcp_subset_values_by_display_handle(
      Display_Handle *      dh,
      VCP_Feature_Subset    subset,
      Feature_Set_Flags     flags,
      Byte_Bit_Flags        features_seen);

Public_Status_Code
app_show_feature_set_values_by_display_handle(
      Display_Handle *      dh,
      Feature_Set_Ref *     fsref,
      Feature_Set_Flags     flags);


void
app_read_changes(Display_Handle * dh);

void
app_read_changes_forever(Display_Handle * dh);

#endif /* APP_GETVCP_H_ */
