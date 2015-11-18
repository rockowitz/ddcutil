/*
 * i2c_shim.h
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#ifndef I2C_SHIM_H_
#define I2C_SHIM_H_

#include <util/coredefs.h>
#include <base/status_code_mgt.h>

Global_Status_Code shim_i2c_writer(
      int    fh,
      int    bytect,
      Byte * bytes_to_write,
      int    sleep_millisec);
Global_Status_Code shim_i2c_reader(
       int        fh,
       int        bytect,
       Byte *     readbuf,
       int        sleep_millisec);

#endif /* I2C_SHIM_H_ */
