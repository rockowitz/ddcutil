/** \f persistent_capabilities.h */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PERSISTENT_CAPABILITIES_H_
#define PERSISTENT_CAPABILITIES_H_

#include "private/ddcutil_types_private.h"
#include "util/error_info.h"

bool   enable_capabilities_cache(bool onoff);
char * get_capabilities_cache_file_name();
char * get_persistent_capabilities(DDCA_Monitor_Model_Key* mmk);
void   set_persistent_capabilites(DDCA_Monitor_Model_Key* mmk, const char * capabilities);
void   dbgrpt_capabilities_hash(int depth, const char * msg);
void   init_persistent_capabilities();

#endif /* PERSISTENT_CAPABILITIES_H_ */
