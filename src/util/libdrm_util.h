/** @file libdrm_util.h
 *  Utilities for use with libdrm
 */

// Copyright (C) 2017-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LIBDRM_UTIL_H_
#define LIBDRM_UTIL_H_

/** \cond */
#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
/** \endcond */

char * connector_type_name( Byte val);
char * connector_type_title(Byte val);
char * connector_status_name( drmModeConnection val);
char * connector_status_title(drmModeConnection val);
char * encoder_type_title(uint32_t encoder_type);

void report_drmModeRes(          drmModeResPtr  res,              int depth);
void report_drmModePropertyBlob( drmModePropertyBlobPtr blob_ptr, int depth);
void report_drmModeConnector(    int fd,  drmModeConnector * p,   int depth);
void report_drm_modeProperty(    drmModePropertyRes * p,          int depth);

void summarize_drm_modeProperty(drmModePropertyRes * p, int depth);
void report_property_value(int fd, drmModePropertyPtr prop_ptr, uint64_t prop_value, int depth) ;

#endif /* LIBDRM_UTIL_H_ */
