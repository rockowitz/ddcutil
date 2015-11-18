/*
 * i2c_base_io.h
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#ifndef I2C_BASE_IO_H_
#define I2C_BASE_IO_H_

#include <stdbool.h>

#include <base/ddc_base_defs.h>
#include <base/call_stats.h>
#include <base/common.h>
#include <base/msg_control.h>
#include <base/status_code_mgt.h>
#include <base/util.h>


void init_i2c_io_stats(I2C_Call_Stats* pStats);

extern I2C_Call_Stats * timing_stats;

typedef Base_Status_Errno_DDC (*I2C_Writer)(int fh, int bytect, Byte * bytes_to_write);
typedef Base_Status_Errno_DDC (*I2C_Reader)(int fh, int bytect, Byte * readbuf);


Base_Status_Errno_DDC write_writer(int fh, int bytect, Byte * pbytes);
Base_Status_Errno_DDC read_reader (int fh, int bytect, Byte * readbuf);
Base_Status_Errno_DDC ioctl_writer(int fh, int bytect, Byte * pbytes);
Base_Status_Errno_DDC ioctl_reader(int fh, int bytect, Byte * readbuf);

// Don't work:
Base_Status_Errno_DDC i2c_smbus_write_i2c_block_data_writer(int fh, int bytect, Byte * bytes_to_write);
Base_Status_Errno_DDC i2c_smbus_read_i2c_block_data_reader(int fh, int bytect, Byte * readbuf);


// void init_i2c_io();


#endif /* I2C_BASE_IO_H_ */
