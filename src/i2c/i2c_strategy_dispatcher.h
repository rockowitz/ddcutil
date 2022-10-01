/** \file i2c_strategy_dispatcher.h
 *
 *  Vestigial code for testing alternative mechanisms to read from and write to
 *  the IC2 bus.
 */
// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_STRATEGY_DISPATCHER_H_
#define I2C_STRATEGY_DISPATCHER_H_

#include "util/coredefs.h"

#include "base/execution_stats.h"
#include "base/status_code_mgt.h"

#include "i2c_execute.h"


/** I2C IO strategy ids  */
typedef enum {
   I2C_IO_STRATEGY_NOT_SET,
   I2C_IO_STRATEGY_FILEIO,    ///< use file write() and read()
   I2C_IO_STRATEGY_IOCTL}     ///< use ioctl(I2C_RDWR)
I2C_IO_Strategy_Id;

char *
i2c_io_strategy_id_name(I2C_IO_Strategy_Id id);

/** Describes one I2C IO strategy */
typedef struct {
   I2C_IO_Strategy_Id strategy_id;       ///< id of strategy
   char *             strategy_name;     ///< name of strategy
   I2C_Writer         i2c_writer;        ///< writer function
   I2C_Reader         i2c_reader;        ///< read function
   char *             i2c_writer_name;   ///< write function name
   char *             i2c_reader_name;   ///< read function name
} I2C_IO_Strategy;

bool
is_nvidia_einval_bug(I2C_IO_Strategy_Id strategy_id, int busno, int rc);

void
i2c_set_io_strategy_by_id(I2C_IO_Strategy_Id strategy_id);

I2C_IO_Strategy_Id
i2c_get_io_strategy_id();

Status_Errno_DDC
invoke_i2c_writer(
      int    fd,
      Byte   slave_address,
      int    bytect,
      Byte * bytes_to_write);

Status_Errno_DDC
invoke_i2c_reader(
       int        fd,
       Byte       slave_address,
       bool       read_bytewise,
       int        bytect,
       Byte *     readbuf);

void init_i2c_strategy_dispatcher();

#endif /* I2C_STRATEGY_DISPATCHER_H_ */
