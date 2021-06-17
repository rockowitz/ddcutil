/** \file xdg_util.c
 *  Implement XDG Base Directory Specification
 */

// Copyright (C) 2020-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef XDG_UTIL_H_
#define XDG_UTIL_H_

char * xdg_data_home_dir();     // $XDG_DATA_HOME    or $HOME/.local/share
char * xdg_config_home_dir();   // $XDG_CONFIG_HOME  or $HOME/.config
char * xdg_cache_home_dir();    // $XDG_CACHE_HOME   or $HOME/.cache
char * xdg_state_home_dir();    // $XDG_STATE_HOME   or $HOME/.local/state

char * xdg_data_dirs();         // $XDG_DATA_DIRS    or /usr/local/share:/usr/share
char * xdg_config_dirs();       // $XDG_CONFIG_DIRS  or /etc/xdg

char * xdg_data_path();         // XDG_DATA_HOME directory, followed by XDG_DATA_DIRS
char * xdg_config_path();       // XDG_CONFIG_HOME directory, followed by XDG_CONFIG_DIRS

char * xdg_data_home_file(  const char * application, const char * simple_fn);
char * xdg_config_home_file(const char * application, const char * simple_fn);
char * xdg_cache_home_file( const char * application, const char * simple_fn);
char * xdg_state_home_file( const char * application, const char * simple_fn);

char * find_xdg_data_file(  const char * application, const char * simple_fn);
char * find_xdg_config_file(const char * application, const char * simple_fn);
char * find_xdg_cache_file( const char * application, const char * simple_fn);
char * find_xdg_state_file( const char * application, const char * simple_fn);

void xdg_tests();

#endif /* XDG_UTIL_H_ */
