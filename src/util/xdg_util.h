/** \file xdg_util.c
 *  Implement XDG Base Directory Specification
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef XDG_UTIL_H_
#define XDG_UTIL_H_

char *
find_xdg_data_file(
      const char * application,
      const char * simple_fn);

#endif /* XDG_UTIL_H_ */
