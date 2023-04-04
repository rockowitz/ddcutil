/** @file query_sysenv.h
 *
 *  Primary file for the ENVIRONMENT command
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_H_
#define QUERY_SYSENV_H_

#include <stdbool.h>

void init_query_sysenv();
void query_sysenv(bool quickenv);

#endif /* QUERY_SYSENV_H_ */
