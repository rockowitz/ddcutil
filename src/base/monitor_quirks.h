/** @file monitor_quirks.h */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MONITOR_QUIRKS_H_
#define MONITOR_QUIRKS_H_

#include "base/monitor_model_key.h"

typedef enum {
   MQ_NONE         = 0,
   MQ_NO_SETTING   = 1,
   MQ_NO_MFG_RANGE = 2,
   MQ_OTHER        = 4,
} Monitor_Quirk_Type;

typedef struct {
   Monitor_Quirk_Type quirk_type;
   char *             quirk_msg;
} Monitor_Quirk_Data;

Monitor_Quirk_Data *
get_monitor_quirks(Monitor_Model_Key * mmk);

#endif /* MONITOR_QUIRKS_H_ */
