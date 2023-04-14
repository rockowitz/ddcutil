// libkmod_util.h

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LIBKMOD_UTIL_H_
#define LIBKMOD_UTIL_H_

#include "linux_util.h"

int module_status_using_libkmod(const char * module_alias);
int is_module_loaded_using_libkmod(const char * module_name);

#endif /* LIBKMOD_UTIL_H_ */
