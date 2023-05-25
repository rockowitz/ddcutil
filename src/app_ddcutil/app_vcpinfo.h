/** @file app_vcpinfo.h
 *
 *  Implement VCPINFO and (deprecated) LISTVCP commands
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_VCPINFO_H_
#define APP_VCPINFO_H_

#include <stdbool.h>
#include <stdio.h>

#include "cmdline/parsed_cmd.h"

void
app_listvcp(FILE * fh);

bool
app_vcpinfo(Parsed_Cmd * parsed_cmd);

void
init_app_vcpinfo();

#endif /* APP_VCPINFO_H_ */
