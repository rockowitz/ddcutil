/** \file ddc_command_codes.h
 *  DDC/CI command codes
 */

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_COMMAND_CODES_H_
#define DDC_COMMAND_CODES_H_

//
// DDC/CI Command and Response Codes
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

char * ddc_cmd_code_name(Byte command_id);

#endif /* DDC_COMMAND_CODES_H_ */
