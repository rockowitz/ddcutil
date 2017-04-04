/* parsed_capabilities_feature.h
 *
 * <copyright>
 * Copyright (C) 2015-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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
 * Contains the parsed description of a VCP feature extracted from a capabilities string.
 */

#ifndef PARSED_CAPABILITIES_H
#define PARSED_CAPABILITIES_H

#include "util/data_structures.h"

#include "base/core.h"
#include "base/feature_sets.h"


// Used when parsing capabilities string

#define CAPABILITIES_FEATURE_MARKER "VCPF"
/** Parsed description of a VCP Feature in a capabilities string. */
typedef struct {
     char              marker[4];     ///<  always "VCPF"
     Byte              feature_id;    ///<  VCP feature code
#ifdef OLD_BVA
     Byte_Value_Array  values;
#endif
     Byte_Bit_Flags    bbflags;       // alternative
     char *            value_string;  ///< value substring from capabilities string
} Capabilities_Feature_Record;

Capabilities_Feature_Record *
new_capabilities_feature(
      Byte   feature_id,
      char * value_string_start,
      int    value_string_len);

void
free_capabilities_feature(
      Capabilities_Feature_Record * vfr);
void
show_capabilities_feature(
      Capabilities_Feature_Record * vfr,
      DDCA_MCCS_Version_Spec        vcp_version);

#endif /* PARSED_CAPABILITIES_H */
