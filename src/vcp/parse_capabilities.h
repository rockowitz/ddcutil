/** @file parse_capabilities.h
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PARSE_CAPABILITIES_H_
#define PARSE_CAPABILITIES_H_

#include <glib.h>

#include "util/data_structures.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/vcp_version.h"


#define PARSED_CAPABILITIES_MARKER "CAPA"
/** Contains parsed capabilities information */
typedef struct {
   char                    marker[4];       // always "CAPA"
   char *                  raw_value;
   char *                  mccs_version_string;
   bool                    raw_cmds_segment_seen;
   bool                    raw_vcp_features_seen;
   bool                    raw_value_synthesized;
   Byte_Value_Array        commands;        // each stored byte is command id
   GPtrArray *             vcp_features;    // entries are Capabilities_Feature_Record *
   DDCA_MCCS_Version_Spec  parsed_mccs_version;
} Parsed_Capabilities;


Parsed_Capabilities* parse_capabilities_buffer(Buffer * capabilities);
Parsed_Capabilities* parse_capabilities_string(char * capabilities);

void                 free_parsed_capabilities(Parsed_Capabilities * pcaps);
Byte_Bit_Flags       parsed_capabilities_feature_ids(Parsed_Capabilities * pcaps, bool readable_only);

bool                 parsed_capabilities_may_support_table_commands(Parsed_Capabilities * pcaps);


// Tests

void test_segments();
void test_parse_caps();

#endif /* PARSE_CAPABILITIES_H_ */
