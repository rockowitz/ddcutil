/* parse_capabilities.h
 *
 * Parse the capabilities string.
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

#ifndef PARSE_CAPABILITIES_H_
#define PARSE_CAPABILITIES_H_

#include <glib.h>

#include "util/data_structures.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/vcp_version.h"


#define PARSED_CAPABILITIES_MARKER "CAPA"
typedef struct {
   char              marker[4];       // always "CAPA"
   char *            raw_value;
   char *            mccs_ver;
   bool              raw_cmds_segment_seen;
   bool              raw_value_synthesized;
   Byte_Value_Array  commands;        // each stored byte is command id
   GPtrArray *       vcp_features;    // entries are VCP_Feature_Record*
   DDCA_MCCS_Version_Spec      parsed_mccs_version;
} Parsed_Capabilities;


Parsed_Capabilities* parse_capabilities_buffer(Buffer * capabilities);
Parsed_Capabilities* parse_capabilities_string(char * capabilities);
void report_parsed_capabilities(Parsed_Capabilities* pcaps);
void free_parsed_capabilities(Parsed_Capabilities * pcaps);
Byte_Bit_Flags parsed_capabilities_feature_ids(Parsed_Capabilities * pcaps, bool readable_only);


// Tests

void test_segments();
void test_parse_caps();

#endif /* PARSE_CAPABILITIES_H_ */
