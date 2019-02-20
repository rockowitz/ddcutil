/** @file dyn_parsed_capabilities.h
 *
 *  Report parsed capabilities, taking into account dynamic feature definitions.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DYN_PARSED_CAPABILITIES_H_
#define DYN_PARSED_CAPABILITIES_H_

#include "base/displays.h"
#include "vcp/parse_capabilities.h"

#ifdef UNNEEDED
#include "base/vcp_version.h"
#include "vcp/parsed_capabilities_feature.h"

void report_capabilities_feature(
      Capabilities_Feature_Record * vfr,
      DDCA_MCCS_Version_Spec        vcp_version,
      int                           depth);
#endif

void dyn_report_parsed_capabilities(
         Parsed_Capabilities*    pcaps,
         Display_Handle *        dh,
         Display_Ref *           dref,
         int                     depth);

#endif /* DYN_PARSED_CAPABILITIES_H_ */
