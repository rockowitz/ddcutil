/** @file drm_common.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DRM_COMMON_H_
#define DRM_COMMON_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>

#include "data_structures.h"
/** \endcond */

const char * drm_bus_type_name(uint8_t bus);
bool         check_drm_supported_using_drm_api(char * busid2);
bool         adapter_supports_drm_using_drm_api(const char * adapter_path);
bool         all_displays_drm_using_drm_api();
bool         check_video_adapters_list_implements_drm(GPtrArray * adapter_devices);
bool         check_all_video_adapters_implement_drm();
GPtrArray *  get_dri_device_names_using_filesys();
bool         all_video_adapters_support_drm_using_drm_api(GPtrArray * adapter_paths);

#endif /* DRM_COMMON_H_ */
