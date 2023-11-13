/** \file ddc_command_codes.c
 *  DDC/CI command codes
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ddc_command_codes.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/string_util.h"

//
// DDC Command and Response Codes
//

typedef
struct {
   Byte    cmd_code;
   char *  name;
} Cmd_Code_Table_Entry;

Cmd_Code_Table_Entry cmd_code_table[] = {
      {CMD_VCP_REQUEST          , "VCP Request" },
      {CMD_VCP_RESPONSE         , "VCP Response" },
      {CMD_VCP_SET              , "VCP Set"      },
      {CMD_TIMING_REPLY         , "Timing Reply "},
      {CMD_TIMING_REQUEST       , "Timing Request" },
      {CMD_VCP_RESET            , "VCP Reset" },
      {CMD_SAVE_SETTINGS        , "Save Settings" },
      {CMD_SELF_TEST_REPLY      , "Self Test Reply" },
      {CMD_SELF_TEST_REQUEST    , "Self Test Request" },
      {CMD_ID_REPLY             , "Identification Reply"},
      {CMD_TABLE_READ_REQUST    , "Table Read Request" },
      {CMD_CAPABILITIES_REPLY   , "Capabilities Reply" },
      {CMD_TABLE_READ_REPLY     , "Table Read Reply" },
      {CMD_TABLE_WRITE          , "Table Write" },
      {CMD_ID_REQUEST           , "Identification Request" },
      {CMD_CAPABILITIES_REQUEST , "Capabilities Request" },
      {CMD_ENABLE_APP_REPORT    , "Enable Application Report" }
};
static
int ddc_cmd_code_count = sizeof(cmd_code_table)/sizeof(Cmd_Code_Table_Entry);


static
Cmd_Code_Table_Entry * get_ddc_cmd_struct_by_id(Byte cmd_id) {
   // DBGMSG("Starting. id=0x%02x ", id );
   int ndx = 0;
   Cmd_Code_Table_Entry * result = NULL;
   for (;ndx < ddc_cmd_code_count; ndx++) {
      if (cmd_id == cmd_code_table[ndx].cmd_code) {
         result = &cmd_code_table[ndx];
         break;
      }
   }
   // DBGMSG("Done.  ndx=%d. returning %p", ndx, result);
   return result;
}


char * ddc_cmd_code_name(Byte command_id) {
   char * result = NULL;
   Cmd_Code_Table_Entry * cmd_entry = get_ddc_cmd_struct_by_id(command_id);
   if (cmd_entry)
      result = cmd_entry->name;
   else
      result ="Unrecognized operation code";
   return result;
}
