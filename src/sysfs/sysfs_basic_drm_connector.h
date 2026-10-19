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


/** A reduced DRM connector record used while associating I2C buses.
 *
 *  Connector name and ID are the result.  The I2C bus number, base bus
 *  number, EDID, and adapter name are match keys.  MST adapters do not publish
 *  a connector bus number; when their DPMST adapter name identifies exactly
 *  one connected connector with an EDID, that association can be used instead.
 *
 *  #Sys_Drm_Connector carries additional fields for diagnostics and hotplug.
 *  This smaller record avoids reading those attributes during bus lookup.
 */
typedef struct {
   char *  connector_name;    // e.g. card1-DP-1, copied to businfo->drm_connector_name
   int     connector_id;      // copied to businfo->drm_connector_id, -1 if not published
   int     i2c_busno;         // match key, -1 if the driver does not publish it
   int     base_busno;
   char *  adapter_name;      // connector AUX/I2C adapter name, e.g. "DPMST"
   bool    connected;         // set when a DPMST connector reports connected
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

Sys_Basic_Drm_Connector * find_unique_basic_drm_connector_by_adapter_name(
      GPtrArray * connectors, const char * adapter_name);

void dbgrpt_basic_drm_connector(Sys_Basic_Drm_Connector * connector, int depth);
void dbgrpt_basic_drm_connectors(GPtrArray * connectors, int depth);

void init_sysfs_basic_drm_connector();

#endif /* SYSFS_BASIC_DRM_CONNECTOR_H_ */

