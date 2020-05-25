/** \file parsed_capabilitied_feature.h
 * Parses the description of a VCP feature extracted from a capabilities string.
 */

// Copyright (C) 2015-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PARSED_CAPABILITIES_FEATURE_H
#define PARSED_CAPABILITIES_FEATURE_H

#include <glib-2.0/glib.h>

#include "util/data_structures.h"

#include "base/core.h"
#include "base/feature_sets.h"

// define both for testing
#define CFR_BVA     // Use Byte_Value_Array for values
#undef  CFR_BBF     // Use Byte_Bit_Flags for values

#define CAPABILITIES_FEATURE_MARKER "VCPF"
/** Parsed description of a VCP Feature in a capabilities string. */
typedef struct {
     char              marker[4];     ///<  always "VCPF"
     Byte              feature_id;    ///<  VCP feature code
     Byte_Value_Array  values;        ///<  need unsorted values for feature x72 gamma
#ifdef CFR_BBF
     Byte_Bit_Flags    bbflags;       //    alternative, but sorts values, screws up x72 gamma
#endif
     char *            value_string;  ///<  value substring from capabilities string
     bool              valid_values;  ///<  string is valid
} Capabilities_Feature_Record;

Capabilities_Feature_Record * parse_capabilities_feature(
      Byte   feature_id,
      char * value_string_start,
      int    value_string_len,
      GPtrArray* error_messages);

void free_capabilities_feature_record(
      Capabilities_Feature_Record * vfr);

void dbgrpt_capabilities_feature_record(
      Capabilities_Feature_Record * vfr, int depth);

#endif /* PARSED_CAPABILITIES_FEATURE_H */
