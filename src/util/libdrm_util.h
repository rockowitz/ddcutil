/* libdrm_util.h
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#ifndef LIBDRM_UTIL_H_
#define LIBDRM_UTIL_H_

#include <xf86drm.h>
#include <xf86drmMode.h>

char * connector_type_name(Byte val);
  char * connector_status_name(drmModeConnection val);
  char * encoder_type_title(uint32_t encoder_type);

void report_drmModeRes(          drmModeResPtr  res,              int depth);
void report_drmModePropertyBlob( drmModePropertyBlobPtr blob_ptr, int depth);
void report_drmModeConnector(int fd,    drmModeConnector * p,            int depth);
void report_drm_modeProperty(    drmModePropertyRes * p,          int depth);

void summarize_drm_modeProperty(drmModePropertyRes * p, int depth);
void report_property_value(int fd, drmModePropertyPtr prop_ptr, uint64_t prop_value, int depth) ;

#endif /* LIBDRM_UTIL_H_ */
