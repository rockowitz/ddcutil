/** @file stats.h
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef STATS_H_
#define STATS_H_


#include <stdint.h>

//! I2C retry types
typedef enum{
   WRITE_ONLY_TRIES_OP,       /**< write-only operation tries */
   WRITE_READ_TRIES_OP,       /**< read-write operation tries */
   MULTI_PART_READ_OP,        /**< multi-part read operation tries */
   MULTI_PART_WRITE_OP        /**< multi-part write operation tries */
} Retry_Operation;
#define RETRY_OP_COUNT 4
typedef uint16_t Retry_Op_Value;

//
// Retry management
//

const char * retry_type_name(Retry_Operation stat_id);
const char * retry_type_description(Retry_Operation retry_class);

#endif /* STATS_H_ */
