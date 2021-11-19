/** \f sysfs_filter_functions.h
  */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_FILTER_FUNCTIONS_H_
#define SYSFS_FILTER_FUNCTIONS_H_

#include <stdbool.h>

#include "data_structures.h"


bool
is_sysfs_drm_connector_dir_name(
      const char * dirname,
      const char * simple_fn);



bool
starts_with_card(const char * val);



// for e.g. card0-HDMI-0
bool is_sysfs_drm_connector_dir_name(const char * dirname, const char * simple_fn);


bool
starts_with_card(const char * val);



// Filter Functions

bool predicate_cardN(const char * val);

bool startswith_i2c(const char * value);

bool class_display_device_predicate(char * value);


// Filter Functions

bool drm_filter(const char * name);



//  Filter functions



// for e.g. i2c-3
bool is_i2cN(const char * dirname, const char * val);


bool is_drm_dp_aux_subdir(const char * dirname, const char * val);


// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn);


// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn);

bool is_drm_dir(const char * dirname, const char * simple_fn);


bool is_i2cN_dir(const char * dirname, const char * simple_fn);

bool has_class_display_or_docking_station(const char * dirname, const char * simple_fn);


bool
is_sysfs_drm_connector_dir_name(
      const char * dirname,
      const char * simple_fn);


#ifdef OOPS

bool
starts_with_card(const char * val);



// for e.g. card0-HDMI-0
bool is_sysfs_drm_connector_dir_name(const char * dirname, const char * simple_fn);


bool
starts_with_card(const char * val);



// Filter Functions

bool predicate_cardN(const char * val);

bool startswith_i2c(const char * value);

bool class_display_device_predicate(char * value);


// Filter Functions

bool drm_filter(const char * name);



//  Filter functions



// for e.g. i2c-3
bool is_i2cN(const char * dirname, const char * val);


bool is_drm_dp_aux_subdir(const char * dirname, const char * val);


// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn);


// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn);

bool is_drm_dir(const char * dirname, const char * simple_fn);


bool is_i2cN_dir(const char * dirname, const char * simple_fn);

bool has_class_display_or_docking_station(const char * dirname, const char * simple_fn);


bool
is_sysfs_drm_connector_dir_name(
      const char * dirname,
      const char * simple_fn);



bool
starts_with_card(const char * val);



// for e.g. card0-HDMI-0
bool is_sysfs_drm_connector_dir_name(const char * dirname, const char * simple_fn);




bool
starts_with_card(const char * val);



// Filter Functions

bool predicate_cardN(const char * val);

bool startswith_i2c(const char * value);

bool class_display_device_predicate(char * value);


// Filter Functions

bool drm_filter(const char * name);



//  Filter functions



// for e.g. i2c-3
bool is_i2cN(const char * dirname, const char * val);


bool is_drm_dp_aux_subdir(const char * dirname, const char * val);


// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn);


// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn);

bool is_drm_dir(const char * dirname, const char * simple_fn);


bool is_i2cN_dir(const char * dirname, const char * simple_fn);

bool has_class_display_or_docking_station(const char * dirname, const char * simple_fn);


#endif

#endif /* SYSFS_FILTER_FUNCTIONS_H_ */
