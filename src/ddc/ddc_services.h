/* ddc_services.h
 *
 * Created on: Nov 15, 2015
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

#ifndef DDC_SERVICES_H_
#define DDC_SERVICES_H_

#include <glib.h>
#include <stdio.h>
#include <time.h>

#include "base/common.h"
#include "base/ddc_base_defs.h"     // for Version_Spec
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "ddc/vcp_feature_codes.h"
#include "ddc/vcp_feature_set.h"


void init_ddc_services();



Global_Status_Code
check_valid_operation_by_feature_rec_and_version(
      VCP_Feature_Table_Entry * feature_rec,
      Version_Spec              vcp_version,
      Version_Feature_Flags     operation_flags);


Global_Status_Code
check_valid_operation_by_feature_id_and_dh(
      Byte                  feature_id,
      Display_Handle *      dh,
      Version_Feature_Flags operation_flags);



void show_vcp_values_by_display_ref(
        Display_Ref *      dref,
        VCP_Feature_Subset subset,
        GPtrArray *        collector);

void show_single_vcp_value_by_display_ref(
        Display_Ref * dref,
        char *        feature,
        bool          force);

Global_Status_Code set_vcp_value_top(
        Display_Ref* dref,
        char *       feature,
        char *       new_value);

Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh);
Version_Spec get_vcp_version_by_display_ref(   Display_Ref *    dref);


char * format_timestamp(time_t time_millis, char * buf, int bufsz);

GPtrArray * get_profile_related_values_by_display_handle(Display_Handle * dh);
GPtrArray * get_profile_related_values_by_display_ref(   Display_Ref *    dref);

void ddc_show_max_tries();

#endif /* DDC_SERVICES_H_ */
