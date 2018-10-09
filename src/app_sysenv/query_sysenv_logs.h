/** @file query_sysenv_logs.h
 *  Query configuration files, logs, and output of logging commands.
 */

// Copyright (C) 2017-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_LOGS_H_
#define QUERY_SYSENV_LOGS_H_

#include "query_sysenv_base.h"

void probe_logs(Env_Accumulator * accum);
void probe_config_files(Env_Accumulator * accum);

#endif /* QUERY_SYSENV_LOGS_H_ */
