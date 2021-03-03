// ddcutil_config_file.h

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_CONFIG_FILE_H_
#define DDCUTIL_CONFIG_FILE_H_

int    read_ddcutil_config_file(char * application, char *** tokens_loc, char** default_options_loc);
char * get_config_file_name();

#endif /* DDCUTIL_CONFIG_FILE_H_ */
