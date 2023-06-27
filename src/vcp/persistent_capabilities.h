/** \f persistent_capabilities.h */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PERSISTENT_CAPABILITIES_H_
#define PERSISTENT_CAPABILITIES_H_

#include "util/error_info.h"

#include "base/monitor_model_key.h"

bool   enable_capabilities_cache(bool onoff);
char * capabilities_cache_file_name();
void   delete_capabilities_file();
char * get_persistent_capabilities(Monitor_Model_Key* mmk);
void   set_persistent_capabilites(Monitor_Model_Key* mmk, const char * capabilities);
void   dbgrpt_capabilities_hash(int depth, const char * msg);
void   init_persistent_capabilities();
void   terminate_persistent_capabilities();

#endif /* PERSISTENT_CAPABILITIES_H_ */
