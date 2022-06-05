/** @file app_setvcp.h
 *
 *  Implement the SETVCP command
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_SETVCP_H_
#define APP_SETVCP_H_

#include "cmdline/parsed_cmd.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

Status_Errno_DDC
app_setvcp(
      Parsed_Cmd *      parsed_cmd,
      Display_Handle *  dh);

void init_app_setvcp();

#endif /* APP_SETVCP_H_ */
