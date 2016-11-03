/* app_getvcp.h
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

#ifndef APP_GETVCP_H_
#define APP_GETVCP_H_

#include <stdbool.h>

#include "base/displays.h"
#include "base/feature_sets.h"
#include "base/status_code_mgt.h"


Global_Status_Code
app_show_single_vcp_value_by_feature_table_entry(
      Display_Handle *   dh,
      VCP_Feature_Table_Entry * entry);

Global_Status_Code
app_show_single_vcp_value_by_feature_id(
      Display_Handle *   dh,
      Byte               feature_id,
      bool               force);

Global_Status_Code
app_show_vcp_subset_values_by_display_handle(
      Display_Handle *   dh,
      VCP_Feature_Subset subset,
      bool               show_unsupported);

#ifdef OLD
Global_Status_Code
app_show_vcp_subset_values_by_display_ref(
      Display_Ref *      dref,
      VCP_Feature_Subset subset,
      bool               show_unsupported);
#endif

Global_Status_Code
app_show_feature_set_values_by_display_handle(
      Display_Handle *   dh,
      Feature_Set_Ref *  fsref,
      bool               show_unsupported,
      bool               force);


void
app_read_changes(Display_Handle * dh);

void
app_read_changes_forever(Display_Handle * dh);

#endif /* APP_GETVCP_H_ */
