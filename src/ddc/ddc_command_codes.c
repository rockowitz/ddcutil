/*
 * ddc_command_codes.c
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <util/string_util.h>

#include <ddc/ddc_command_codes.h>


//
// DDC Command Codes
//

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
int cmd_code_count = sizeof(cmd_code_table)/sizeof(Cmd_Code_Table_Entry);


Cmd_Code_Table_Entry * get_cmd_code_table_entry(int ndx) {
   // printf("(%s) ndx=%d, cmd_code_count=%d  \n", __func__, ndx, cmd_code_count );
   assert( 0 <= ndx && ndx < cmd_code_count);
   return &cmd_code_table[ndx];
}


void list_cmd_codes() {
   printf("DDC command codes:\n");
   int ndx = 0;
   for (;ndx < cmd_code_count; ndx++) {
      Cmd_Code_Table_Entry entry = cmd_code_table[ndx];
      printf("  %02x - %-30s\n", entry.cmd_code, entry.name);
   }
}


Cmd_Code_Table_Entry * find_cmd_entry_by_hexid(Byte id) {
   // printf("(%s) Starting. id=0x%02x \n", __func__, id );
   int ndx = 0;
   Cmd_Code_Table_Entry * result = NULL;
   for (;ndx < cmd_code_count; ndx++) {
      if (id == cmd_code_table[ndx].cmd_code) {
         result = &cmd_code_table[ndx];
         break;
      }
   }
   // printf("(%s) Done.  ndx=%d. returning %p\n", __func__, ndx, result);
   return result;
}


char * get_command_name(Byte command_id) {
   char * result = NULL;
   // in vcp_feature_codes.c:
   Cmd_Code_Table_Entry * cmd_entry = find_cmd_entry_by_hexid(command_id);
   if (cmd_entry)
      result = cmd_entry->name;
   else
      result ="unrecognized command";
   return result;
}

