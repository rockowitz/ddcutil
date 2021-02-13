/** \f query_sysenv_modules.h
 *
 *  Module checks
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef QUERY_SYSENV_MODULES_H_
#define QUERY_SYSENV_MODULES_H_

#include <stdbool.h>

#include "query_sysenv_base.h"

void check_i2c_dev_module(Env_Accumulator * accum, int depth);
void probe_modules_d(int depth);

#endif /* QUERY_SYSENV_MODULES_H_ */
