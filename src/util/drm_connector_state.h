/** @file drm_connector_state.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DRM_CONNECTOR_STATE_H_
#define DRM_CONNECTOR_STATE_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "drm_common.h"
#include "edid.h"
/** \endcond */


typedef struct {
   int               cardno;
   int               connector_id;
   uint32_t          connector_type;
   uint32_t          connector_type_id;
   drmModeConnection connection;
   Parsed_Edid  *    edid;
   uint64_t          link_status;
   uint64_t          dpms;
   uint64_t          subconnector;
} Drm_Connector_State;

int get_drm_connector_states_by_devname(const char * devname, bool verbose, GPtrArray * collector);
Drm_Connector_State * get_drm_connector_state_by_devname(const char * devname, int connector_id);
GPtrArray* drm_get_all_connector_states();
void redetect_drm_connector_states();
void report_drm_connector_states(int depth);

Drm_Connector_State * find_drm_connector_state(Drm_Connector_Identifier cid);

#endif /* DRM_CONNECTOR_STATE_H_ */
