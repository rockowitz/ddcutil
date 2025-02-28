// drm_card_connector_util.h

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DRM_CARD_CONNECTOR_UTIL_H_
#define DRM_CARD_CONNECTOR_UTIL_H_

#include <stdbool.h>

typedef struct {
   int cardno;
   int connector_id;
   int connector_type;
   int connector_type_id;
} Drm_Connector_Identifier;

char * dci_repr(Drm_Connector_Identifier dci);
char * dci_repr_t(Drm_Connector_Identifier dci);
bool   dci_eq(Drm_Connector_Identifier dci1, Drm_Connector_Identifier dci2);
int    dci_cmp(Drm_Connector_Identifier dci1, Drm_Connector_Identifier dci2);
int    sys_drm_connector_name_cmp0(const char * s1, const char * s2);
int    sys_drm_connector_name_cmp(gconstpointer connector_name1, gconstpointer connector_name2);
Drm_Connector_Identifier parse_sys_drm_connector_name(const char * drm_connector);

#ifdef UNUSED
Bit_Set_32   get_sysfs_drm_card_numbers();
#endif

#endif /* DRM_CARD_CONNECTOR_UTIL_H_ */
