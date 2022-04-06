/** #file common_init.h
  *  Initialization that must be performed very early by both ddcutil and libddcutil
  */

// Copyright (C) 2021-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_COMMON_INIT_H_
#define DDC_COMMON_INIT_H_

#include "cmdline/parsed_cmd.h"

void init_tracing(Parsed_Cmd * parsed_cmd);
bool submaster_initializer(Parsed_Cmd * parsed_cmd);

#endif /* DDC_COMMON_INIT_H_ */
