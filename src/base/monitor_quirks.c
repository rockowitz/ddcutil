/** @file monitor_quirks.c */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/coredefs_base.h"

#include "base/core.h"
#include "base/monitor_model_key.h"

#include "base/monitor_quirks.h"

typedef struct {
   Monitor_Model_Key    mmk;
   Monitor_Quirk_Data   data;
} Monitor_Quirk_Table_Entry;

static Monitor_Quirk_Table_Entry quirk_table[] = {
   {{ "XMI", "Mi Monitor", 13380, true}, { MQ_NO_SETTING,   NULL}},
// {{ "DEL", "DELL U3011", 16485, true}, { MQ_NO_MFG_RANGE, "msg 1"}},    // for testing
// {{ "NEC", "P241W",      26715, true}, { MQ_NO_SETTING,   "msg 2"}},    // for testing
};
int quirk_table_size = ARRAY_SIZE(quirk_table);


Monitor_Quirk_Data *
get_monitor_quirks(Monitor_Model_Key * mmk) {
   bool debug = false;
   DBGMSF(debug, "quirk_table_size=%d, mmk=%s", quirk_table_size, mmk_repr(*mmk));
   Monitor_Quirk_Data * result = NULL;

   for (int ndx = 0; ndx < quirk_table_size; ndx++) {
      DBGMSF(debug,  "ndx=%d, mmk=%s", ndx, mmk_repr(quirk_table[ndx].mmk));
      if (monitor_model_key_eq(*mmk, quirk_table[ndx].mmk)) {
         result = &quirk_table[ndx].data;
         break;
      }
   }
   return result;
}
