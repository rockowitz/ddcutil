/** @file app_dumpload.h
 *
 *  Implement the DUMPVCP and LOADVCP commands
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_DUMPLOAD_H_
#define APP_DUMPLOAD_H_

#include <base/displays.h>
#include <base/status_code_mgt.h>

Status_Errno_DDC
app_loadvcp_by_file(const char * fn, Display_Handle * dh);

Status_Errno_DDC
app_dumpvcp_as_file(Display_Handle * dh, const char * optional_filename);

void
init_app_dumpload();

#endif /* APP_DUMPLOAD_H_ */
