/** \file app_experimental.h */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_EXPERIMENTAL_H_
#define APP_EXPERIMENTAL_H_

#include "cmdline/parsed_cmd.h"

bool init_experimental_options(Parsed_Cmd* parsed_cmd);
void report_experimental_options(Parsed_Cmd * parsed_cmd, int depth);

void test_display_detection_variants();

#endif /* APP_EXPERIMENTAL_H_ */
