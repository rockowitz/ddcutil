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
#include "base/ddc_base.h"
#include "base/displays.h"


// TODO: replace GArray with GPtrArray for vcp_features

#define PARSED_CAPABILITIES_MARKER "CAPA"
typedef struct {
   char              marker[4];       // always "CAPA"
   char *            raw_value;
   char *            mccs_ver;
   Byte_Value_Array  commands;        // each stored byte is command id
   GArray *          vcp_features;    // entries are VCP_Feature_Record
   Version_Spec      parsed_mccs_version;
} Parsed_Capabilities;


Parsed_Capabilities* parse_capabilities_buffer(Buffer * capabilities);
Parsed_Capabilities* parse_capabilities_string(char * capabilities);
void report_parsed_capabilities(Parsed_Capabilities* pcaps, MCCS_IO_Mode io_mode);
void free_parsed_capabilities(Parsed_Capabilities * pcaps);


// Tests

void test_segments();
void test_parse_caps();

#endif /* PARSE_CAPABILITIES_H_ */
