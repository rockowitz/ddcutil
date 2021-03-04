// file_util_base.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef FILE_UTIL_BASE_H_
#define FILE_UTIL_BASE_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>

#ifdef __cplusplus
extern "C" {
#endif

int
file_getlines(
      const char * fn,
      GPtrArray*   line_array,
      bool         verbose)
;
#ifdef __cplusplus
}    // extern "C"
#endif



#endif /* FILE_UTIL_BASE_H_ */
