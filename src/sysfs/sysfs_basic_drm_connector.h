/** \file sysfs_basic_drm_connector.h
 *  A reduced view of a DRM card connector: only what is needed to set the
 *  connector fields of an #I2C_Bus_Info.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_BASIC_DRM_CONNECTOR_H_
#define SYSFS_BASIC_DRM_CONNECTOR_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "util/coredefs_base.h"    // Byte

#include "public/ddcutil_types.h"

/** What a connector contributes to an #I2C_Bus_Info, and nothing else.
 *
 *  Three of the five fields are the answer -- connector_name and connector_id
 *  are copied into the bus record, and which of the remaining two matched
 *  decides drm_connector_found_by.  The other two are the keys the match is
 *  made on: i2c_busno where the driver publishes the bus/connector mapping,
 *  the EDID where it does not.
 *
 *  #Sys_Drm_Connector carries ten further fields -- connector_path, name,
 *  ddc_dir_path, is_aux_channel, base_busno, base_name, base_dev, enabled,
 *  status, i2c_busno_from_driver -- which nothing on this path reads.  Reading
 *  them costs sysfs attributes per connector, so a scan that skips them is
 *  cheaper than one that does not, and a record that omits them cannot be
 *  mistaken for a general-purpose connector description.
 */
typedef struct {
   char *  connector_name;    // e.g. card1-DP-1, copied to businfo->drm_connector_name
   int     connector_id;      // copied to businfo->drm_connector_id, -1 if not published
   int     i2c_busno;         // match key, -1 if the driver does not publish it
   Byte *  edid_bytes;        // match key, NULL if no display is attached
   gsize   edid_size;
} Sys_Basic_Drm_Connector;

GPtrArray * scan_basic_drm_connectors(int depth);

void free_basic_drm_connectors(GPtrArray * connectors);

void free_basic_drm_connector(void * connector);

Sys_Basic_Drm_Connector * find_basic_drm_connector_by_busno(
      GPtrArray * connectors, int busno);

Sys_Basic_Drm_Connector * find_basic_drm_connector_by_edid(
      GPtrArray * connectors, Byte * edid_bytes);

void dbgrpt_basic_drm_connector(Sys_Basic_Drm_Connector * connector, int depth);
void dbgrpt_basic_drm_connectors(GPtrArray * connectors, int depth);

void init_sysfs_basic_drm_connector();

#endif /* SYSFS_BASIC_DRM_CONNECTOR_H_ */
