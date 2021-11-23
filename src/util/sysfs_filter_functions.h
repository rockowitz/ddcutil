/** \f sysfs_filter_functions.h
  */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_FILTER_FUNCTIONS_H_
#define SYSFS_FILTER_FUNCTIONS_H_

#include <stdbool.h>

#include "data_structures.h"

void free_pcre_hash_table();

// Filename_Filter_Func

bool predicate_cardN(               const char * value);
bool predicate_cardN_connector(     const char * value);
bool startswith_i2c(                const char * value);

// Dir_Filter_Func

// for e.g. dirname i2c-3
bool is_i2cN(const char * dirname, const char * val);
bool is_drm_dp_aux_subdir(const char * dirname, const char * val);

// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn);

// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn);

bool is_i2cN_dir(const char * dirname, const char * simple_fn);

bool has_class_display_or_docking_station(const char * dirname, const char * simple_fn);

#endif /* SYSFS_FILTER_FUNCTIONS_H_ */
