/** @file query_sysenv.h
 *
 *  Primary file for the ENVIRONMENT command
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_H_
#define QUERY_SYSENV_H_

#include <stdbool.h>

#include "cmdline/parsed_cmd.h"

void force_envcmd_settings(Parsed_Cmd * parsed_cmd);
void query_sysenv(bool quickenv);

void init_query_sysenv();

#endif /* QUERY_SYSENV_H_ */
