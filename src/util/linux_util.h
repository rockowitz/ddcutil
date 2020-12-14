/** \file linux_util.h
 *  Miscellaneous Linux utiliites
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_UTIL_H_
#define LINUX_UTIL_H_

#include <stdbool.h>

bool is_module_builtin(char * module_name);

#endif /* LINUX_UTIL_H_ */
