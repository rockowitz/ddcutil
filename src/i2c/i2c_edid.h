/** \file i2c_edid.h
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_EDID_H_
#define I2C_EDID_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/edid.h"
#include "util/data_structures.h"

#include "base/core.h"
#include "base/status_code_mgt.h"

extern bool EDID_Read_Uses_I2C_Layer;
extern bool EDID_Read_Bytewise;
extern bool EDID_Write_Before_Read;
extern int  EDID_Read_Size;
#ifdef TEST_EDID_SMBUS
extern bool EDID_Read_Uses_Smbus;
#endif

Status_Errno_DDC i2c_get_raw_edid_by_fd(int fd, Buffer * rawedid);
Status_Errno_DDC i2c_get_parsed_edid_by_fd(int fd, Parsed_Edid ** edid_ptr_loc);

void init_i2c_edid();

#endif /* I2C_EDID_H_ */
