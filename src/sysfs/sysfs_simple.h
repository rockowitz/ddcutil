/** @file sysfs_simple.h
 *
 *  Answers simple questions about an I2C bus from sysfs, given only its bus
 *  number: its name, its driver, its class, and whether it can be dismissed
 *  without opening it.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_SIMPLE_H_
#define SYSFS_SIMPLE_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>

#include "base/displays.h"

typedef struct {
   int    i2c_busno;
   int    base_busno;
   int    connector_id;
   char * name;
} Connector_Bus_Numbers;

void        dbgrpt_connector_bus_numbers(Connector_Bus_Numbers * cbn, int depth);
void        free_connector_bus_numbers(Connector_Bus_Numbers * cbn);
void        get_connector_bus_numbers(
               const char *            dirname,    // <device>/drm/cardN
               const char *            fn,         // card0-HDMI-1 etc
               Connector_Bus_Numbers * cbn);

char *      find_adapter_and_get_driver(char * path, int depth);
char *      get_driver_for_adapter(char * adapter_path, int depth);
// char *   find_adapter(char * path, int depth); // MOVED
char *      sysfs_find_adapter(char * path);
char *      get_driver_for_busno(int busno);
// char *   get_i2c_sysfs_driver_by_busno(int busno);  //duplicative
char *      get_i2c_device_sysfs_name(int busno);
uint32_t    get_i2c_device_sysfs_class(int busno);
bool        sysfs_is_soc_system();
bool        sysfs_is_ignorable_i2c_device(int busno);


// Must be executed by root.  For future use in case libddcutil is incorporated
// into a system service.

extern bool enable_write_detect_to_status;

void possibly_write_detect_to_status(const char * driver, const char * connector);
void possibly_write_detect_to_status_by_connector_name(const char * connector);
void possibly_write_detect_to_status_by_businfo(I2C_Bus_Info * businfo);
void possibly_write_detect_to_status_by_dref(Display_Ref * dref);
void possibly_write_detect_to_status_by_connector_path(const char * path);

#define WRITE_DETECT_TO_STATUS 1
#ifdef WRITE_DETECT_TO_STATUS
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_PATH possibly_write_detect_to_status_by_connector_path
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_NAME possibly_write_detect_to_status_by_connector_name
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_BUSINFO        possibly_write_detect_to_status_by_businfo
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_DREF           possibly_write_detect_to_status_by_dref
#else
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_PATH
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_NAME
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_BUSINFO
#define POSSIBLY_WRITE_DETECT_TO_STATUS_BY_DREF
#endif


void        init_sysfs_simple();

#endif /* SYSFS_SIMPLE_H_ */
