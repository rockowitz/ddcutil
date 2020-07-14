/** \file app_capabilities.h
 *
 *  Capabilities functions factored out from main.c
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_CAPABILITIES_H_
#define APP_CAPABILITIES_H_

#include "base/displays.h"
#include "vcp/parse_capabilities.h"


DDCA_Status app_get_capabilities_string(Display_Handle * dh, char ** capabilities_string_loc);
void app_show_parsed_capabilities2(Display_Handle * dh, Parsed_Capabilities * pcap);
DDCA_Status app_capabilities(Display_Handle * dh);

Parsed_Capabilities *
app_get_capabilities_by_display_handle(
      Display_Handle * dh);

void
app_show_parsed_capabilities(
      char *                capabilities_string,
      Display_Handle *      dh,
      Parsed_Capabilities * pcap);

#endif /* APP_CAPABILITIES_H_ */
