/* vcp_feature_record.h
 *
 * Created on: Nov 1, 2015
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

#ifndef VCP_FEATURE_RECORD_H_
#define VCP_FEATURE_RECORD_H_

#include "util/data_structures.h"

#include "base/ddc_base_defs.h"
#include "base/util.h"


// Used when parsing capabilities string

#define CAPABILITIES_FEATURE_MARKER "VCPF"
typedef struct {
     char              marker[4];     // always "VCPF"
     Byte              feature_id;
     Byte_Value_Array  values;
     Byte_Bit_Flags    bbflags;       // alternative
     char *            value_string;
} Capabilities_Feature_Record;

Capabilities_Feature_Record * new_Capabilities_Feature(
                        Byte   feature_id,
                        char * value_string_start,
                        int    value_string_len);
void free_capabilities_feature(Capabilities_Feature_Record * pfeat);
void report_capabilities_feature(Capabilities_Feature_Record * vfr, Version_Spec vcp_version);

#endif /* VCP_FEATURE_RECORD_H_ */
