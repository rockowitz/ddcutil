/** \file linux_util.h
 *  Miscellaneous Linux utiliites
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_UTIL_H_
#define LINUX_UTIL_H_

#include <stdbool.h>

int get_kernel_config_parm(const char * parm_name, char * buffer, int bufsz);

int is_module_builtin(char * module_name);
bool is_module_loadable(char * module_name);

#endif /* LINUX_UTIL_H_ */
