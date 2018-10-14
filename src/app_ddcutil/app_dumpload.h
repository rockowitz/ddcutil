/** @file app_dumpload.h
 *
 *  Implement the DUMPVCP and LOADVCP commands
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_DUMPLOAD_H_
#define APP_DUMPLOAD_H_

#include <base/displays.h>
#include <base/status_code_mgt.h>

bool loadvcp_by_file(const char * fn, Display_Handle * dh);

Public_Status_Code dumpvcp_as_file(Display_Handle * dh, char * optional_filename);

#endif /* APP_DUMPLOAD_H_ */
