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

#ifdef UNUSED
Bit_Set_32   get_sysfs_drm_card_numbers();
#endif

const char * drm_bus_type_name(uint8_t bus);
bool         check_drm_supported_using_drm_api(char * busid2);
bool         adapter_supports_drm_using_drm_api(const char * adapter_path);
bool         all_displays_drm_using_drm_api();
bool         check_video_adapters_list_implements_drm(GPtrArray * adapter_devices);
GPtrArray *  get_video_adapter_devices();
bool         check_all_video_adapters_implement_drm();
GPtrArray *  get_dri_device_names_using_filesys();
bool         all_video_adapters_support_drm_using_drm_api(GPtrArray * adapter_paths);

extern Value_Name_Title_Table drm_connector_type_table;
char * drm_connector_type_name(Byte val);
char * drm_connector_type_title(Byte val);

typedef struct {
   int cardno;
   int connector_id;
   int connector_type;
   int connector_type_id;
} Drm_Connector_Identifier;

char * dci_repr(Drm_Connector_Identifier dci);
Drm_Connector_Identifier parse_sys_drm_connector_name(char * drm_connector);

#endif /* DRM_COMMON_H_ */
