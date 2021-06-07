/** \file linux_util.h
 *  Miscellaneous Linux utiliites
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LINUX_UTIL_H_
#define LINUX_UTIL_H_

#include <stdbool.h>

int get_kernel_config_parm(const char * parm_name, char * buffer, int bufsz);

#define KERNEL_MODULE_NOT_FOUND     0     // not found
#define KERNEL_MODULE_BUILTIN       1     // module is built into kernel
#define KERNEL_MODULE_LOADABLE_FILE 2     // module is a loadable file
int module_status_using_libkmod(const char * module_alias);

int is_module_loaded_using_libkmod(const char * module_name);

#endif /* LINUX_UTIL_H_ */
