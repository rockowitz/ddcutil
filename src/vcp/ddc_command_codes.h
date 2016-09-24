/* ddc_command_codes.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#ifndef DDC_COMMAND_CODES_H_
#define DDC_COMMAND_CODES_H_

//
// MCCS Command and Response Codes
//

// Used in 3 ways:
// - to identify commands within ddcutil
// - as identifiers in command request and response packets
// - in capabilities string

#define CMD_VCP_REQUEST          0x01
#define CMD_VCP_RESPONSE         0x02
#define CMD_VCP_SET              0x03
#define CMD_TIMING_REPLY         0x06
#define CMD_TIMING_REQUEST       0x07
#define CMD_VCP_RESET            0x09
#define CMD_SAVE_SETTINGS        0x0c
#define CMD_SELF_TEST_REPLY      0xa1
#define CMD_SELF_TEST_REQUEST    0xb1
#define CMD_ID_REPLY             0xe1
#define CMD_TABLE_READ_REQUST    0xe2
#define CMD_CAPABILITIES_REPLY   0xe3
#define CMD_TABLE_READ_REPLY     0xe4
#define CMD_TABLE_WRITE          0xe7
#define CMD_ID_REQUEST           0xf1
#define CMD_CAPABILITIES_REQUEST 0xf3
#define CMD_ENABLE_APP_REPORT    0xf5

typedef
struct {
   Byte    cmd_code;
   char *  name;
} Cmd_Code_Table_Entry;

extern int ddc_cmd_code_count;    // number of entries in command code table

char * ddc_cmd_code_name(Byte command_id);

Cmd_Code_Table_Entry * get_ddc_cmd_struct_by_id(Byte char_code);

Cmd_Code_Table_Entry * get_ddc_cmd_struct_by_index(int ndx);

// void list_cmd_codes();

#endif /* DDC_COMMAND_CODES_H_ */
