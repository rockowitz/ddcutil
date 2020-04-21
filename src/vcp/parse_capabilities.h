/** @file parse_capabilities.h
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PARSE_CAPABILITIES_H_
#define PARSE_CAPABILITIES_H_

/** \cond */
#include <glib.h>

#include "util/data_structures.h"
/** \endcond */

#include "base/vcp_version.h"

typedef enum {
   CAPABILITIES_VALID,
   CAPABILITIES_USABLE,
   CAPABILITIES_INVALID
} Parsed_Capabilities_Validity;


#define PARSED_CAPABILITIES_MARKER "CAPA"
/** Contains parsed capabilities information */
typedef struct {
   char                    marker[4];             // always "CAPA"
   char *                  raw_value;
   bool                    raw_value_synthesized;
   char *                  model;
   char *                  mccs_version_string;
   DDCA_MCCS_Version_Spec  parsed_mccs_version;  // parsed mccs_version_string, DDCA_VSPEC_UNKNOWN if parsing fails
   bool                    raw_cmds_segment_seen;
   bool                    raw_cmds_segment_valid;
   Byte_Value_Array        commands;             // each stored byte is command id
   bool                    raw_vcp_features_seen;
   GPtrArray *             vcp_features;         // entries are Capabilities_Feature_Record *
   Parsed_Capabilities_Validity caps_validity;
   GPtrArray *             messages;
} Parsed_Capabilities;


Parsed_Capabilities* parse_capabilities_string(char * capabilities);
void                 free_parsed_capabilities(Parsed_Capabilities * pcaps);
Byte_Bit_Flags       get_parsed_capabilities_feature_ids(Parsed_Capabilities * pcaps, bool readable_only);
bool                 parsed_capabilities_supports_table_commands(Parsed_Capabilities * pcaps);
char *               parsed_capabilities_validity_name(Parsed_Capabilities_Validity validity);
void                 dbgrpt_parsed_capabilities(Parsed_Capabilities * pcaps, int depth);


// Tests
void test_segments();
void test_parse_caps();

#endif /* PARSE_CAPABILITIES_H_ */
