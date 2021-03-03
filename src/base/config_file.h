/** config_file.h
 *  Read ddcutil configuration file
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CONFIG_FILE_H_
#define CONFIG_FILE_H_

#include <stdbool.h>
#include "util/error_info.h"

Error_Info * load_configuration_file( bool verbose );
char *       get_config_file_name();
char *       get_config_value(char * segment, char * id);
void         dbgrpt_ini_hash(int depth);

#endif /* CONFIG_FILE_H_ */
