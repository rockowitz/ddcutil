/** @file udev_i2c_util.h
  *  I2C specific udev utilities
  */

// Copyright (C) 2016-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef UDEV_I2C_UTIL_H_
#define UDEV_I2C_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
/** \endcond */

#include "data_structures.h"
#include "udev_util.h"

GPtrArray *                     // array of Udev_Device_Summary
get_i2c_devices_using_udev();

int udev_i2c_device_summary_busno(Udev_Device_Summary * summary);

void
report_i2c_udev_device_summaries(
      GPtrArray * summaries,    // array of Udev_Device_Summary
      char *      title,
      int         depth) ;


Byte_Value_Array                // one byte for each I2C bus number
get_i2c_device_numbers_using_udev(bool include_ignorable_devices);

/** Signature of function that tests sys attribute name */
typedef bool (*Sysattr_Name_Filter)(const char * sysattr_name);

Byte_Value_Array
get_i2c_device_numbers_using_udev_w_sysattr_name_filter(Sysattr_Name_Filter keep_func);

#endif /* UDEV_I2C_UTIL_H_ */
