/** @file common_init.h
  * Initialization that must be performed very early by both ddcutil and libddcutil
  */

// Copyright (C) 2021-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_COMMON_INIT_H_
#define DDC_COMMON_INIT_H_

#include <glib-2.0/glib.h>

#include "util/error_info.h"
#include "cmdline/parsed_cmd.h"

void         i2c_discard_caches(Cache_Types caches);
Error_Info * init_tracing(Parsed_Cmd * parsed_cmd);
bool         submaster_initializer(Parsed_Cmd * parsed_cmd);
void         init_ddc_common_init();

#endif /* DDC_COMMON_INIT_H_ */
