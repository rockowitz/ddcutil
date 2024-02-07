// drm_common.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef DRM_COMMON_H_
#define DRM_COMMON_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
/** \endcond */

 bool adapter_supports_drm(const char * adapter_path);

 bool libdrm_extracted_check(char * busid2);
 bool all_displays_drm2();



 #ifdef UNUSED
 Bit_Set_32
 get_sysfs_drm_card_numbers();

 GPtrArray *
 get_video_adapter_devices();
 #endif

 #ifdef WRONG
 bool
 check_video_adapters_list_implements_drm(GPtrArray * adapter_devices);

 bool
 check_all_video_adapters_implement_drm();
 #endif


 bool check_all_video_adapters_implement_drm();


 bool all_video_adapters_support_drm(GPtrArray * adapter_paths);



#endif /* DRM_COMMON_H_ */
