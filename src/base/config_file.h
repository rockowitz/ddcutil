/** config_file.h
 *  Read ddcutil configuration file
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CONFIG_FILE_H_
#define CONFIG_FILE_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>
#include "util/error_info.h"

int          load_configuration_file(char *        config_fn,
                                     GHashTable**  hash_table_loc,
                                     GPtrArray*    errmsgs,
                                     bool          verbose );

char *       get_config_value(GHashTable * config_hash, char * segment, char * id);
void         dbgrpt_ini_hash(GHashTable * config_hash, int depth);

#endif /* CONFIG_FILE_H_ */
