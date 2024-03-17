// drm_connector_state.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef DRM_CONNECTOR_STATE_H_
#define DRM_CONNECTOR_STATE_H_


/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>

#include <xf86drmMode.h>
#include "edid.h"
/** \endcond */

#ifdef REF
typedef struct _drmModeConnector {
   uint32_t connector_id;
   uint32_t encoder_id; /**< Encoder currently connected to */
   uint32_t connector_type;
   uint32_t connector_type_id;
   drmModeConnection connection;
   uint32_t mmWidth, mmHeight; /**< HxW in millimeters */
   drmModeSubPixel subpixel;

   int count_modes;
   drmModeModeInfoPtr modes;

   int count_props;
   uint32_t *props; /**< List of property ids */
   uint64_t *prop_values; /**< List of property values */

   int count_encoders;
   uint32_t *encoders; /**< List of encoder ids */
} drmModeConnector, *drmModeConnectorPtr;
#endif


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

#endif /* DRM_CONNECTOR_STATE_H_ */
