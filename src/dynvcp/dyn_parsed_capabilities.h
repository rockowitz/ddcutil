/** @file dyn_parsed_capabilities.h
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYN_PARSED_CAPABILITIES_H_
#define DYN_PARSED_CAPABILITIES_H_

#include <glib.h>

// #include "ddcutil_types.h"

// #include "private/ddcutil_types_private.h"

// #include "util/data_structures.h"

// #include "base/core.h"
// #include "base/displays.h"
// #include "base/dynamic_features.h"
// #include "base/vcp_version.h"

// #include "vcp/parsed_capabilities_feature.h"

#include "base/displays.h"

#include "vcp/parse_capabilities.h"


#ifdef UNNEEDED
void report_capabilities_feature(
      Capabilities_Feature_Record * vfr,
      DDCA_MCCS_Version_Spec        vcp_version,
      int                           depth);
#endif

void dyn_report_parsed_capabilities(
         Parsed_Capabilities*    pcaps,
         Display_Ref *           dref,
         int                     depth);

#endif /* DYN_PARSED_CAPABILITIES_H_ */
