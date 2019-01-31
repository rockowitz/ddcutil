/** \file parsed_capabilitie_feature.h
 * Contains the parsed description of a VCP feature extracted from a capabilities string.
 */

// Copyright (C) 2015-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
// #ifdef OLD_BVA
     Byte_Value_Array  values;        // need unsorted values for gamma
// #endif
     Byte_Bit_Flags    bbflags;       // alternative, but sorts values, screws up gamma
     char *            value_string;  ///< value substring from capabilities string
} Capabilities_Feature_Record;

Capabilities_Feature_Record * new_capabilities_feature(
      Byte   feature_id,
      char * value_string_start,
      int    value_string_len);

void free_capabilities_feature(
      Capabilities_Feature_Record * vfr);


#endif /* PARSED_CAPABILITIES_H */
