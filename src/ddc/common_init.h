/** \file common_init.h */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef COMMON_INIT_H_
#define COMMON_INIT_H_

#include "cmdline/parsed_cmd.h"

void
init_tracing(Parsed_Cmd * parsed_cmd);
bool init_failsim(Parsed_Cmd * parsed_cmd);
void init_max_tries(Parsed_Cmd * parsed_cmd);
void init_performance_options(Parsed_Cmd * parsed_cmd);
bool submaster_initializer(Parsed_Cmd * parsed_cmd);


#endif /* COMMON_INIT_H_ */
