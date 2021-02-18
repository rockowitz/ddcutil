/** \file app_capabilities.h
 *
 *  Implement the CAPABILITIES command
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_CAPABILITIES_H_
#define APP_CAPABILITIES_H_

/** \cond */
#include "base/displays.h"
/** \endcond */
#include "vcp/parse_capabilities.h"

extern bool persistent_capabilities_enabled;

DDCA_Status
app_get_capabilities_string(
      Display_Handle * dh,
      char ** capabilities_string_loc);

void
app_show_parsed_capabilities(
      Display_Handle * dh,
      Parsed_Capabilities * pcap);

DDCA_Status
app_capabilities(              // implements the CAPABILITIES command
      Display_Handle * dh);

void init_app_capabilities();

#endif /* APP_CAPABILITIES_H_ */
