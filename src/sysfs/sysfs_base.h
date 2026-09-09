/** @file sysfs_base.h */

// Copyright (C) 2020-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_BASE_H_
#define SYSFS_BASE_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdint.h>

#include "base/displays.h"
#include "base/i2c_bus_base.h"


// Sysfs_Connector_Names functions

typedef struct {
   GPtrArray *  all_connectors;
   GPtrArray *  connectors_having_edid;
} Sysfs_Connector_Names;

Sysfs_Connector_Names
            get_sysfs_drm_connector_names();
bool        sysfs_connector_names_equal(Sysfs_Connector_Names cn1, Sysfs_Connector_Names cn2);
void        free_sysfs_connector_names_contents(Sysfs_Connector_Names names_struct);
void        dbgrpt_sysfs_connector_names(Sysfs_Connector_Names connector_names, int depth);
Sysfs_Connector_Names
            copy_sysfs_connector_names_struct(Sysfs_Connector_Names original);

// Misc
char *      find_sysfs_drm_connector_name_by_edid(
                 GPTRARRAY(char*) * connector_names, Byte * edid);
bool        sysfs_connector_directories_exist();

// sysfs reliability

extern bool force_sysfs_unreliable;
extern bool force_sysfs_reliable;

bool        is_driver_reliable(const char * driver_name);
bool        is_connector_reliable(const char * connector_name);

bool        is_sysfs_reliable_for_driver(const char * driver);
bool        is_sysfs_reliable_for_busno(int busno);
bool        is_sysfs_reliable();


// predicate functions
// typedef Dir_Filter_Func
bool        is_n_nnnn(const char * dirname, const char * simple_fn);

void        dbgrpt_sysfs_basic_connector_attributes(int depth);
char *      get_sys_drm_connector_name_by_connector_id(int connector_id);
char *      get_sys_drm_connector_name_by_busno(int busno);
bool        all_sys_drm_connectors_have_connector_id_direct();

char *      get_i2c_sysfs_driver_by_device_name(char * device_name);

#ifdef UNUSED
char *
get_i2c_sysfs_driver_by_fd(
      int fd);
#endif

int search_all_businfo_records_by_connector_name(char *connector_name);

/** What #i2c_check_bus() needs from sysfs about an I2C bus, and nothing else.
 *
 *  **driver** becomes I2C_Bus_Info.driver, and is what every later reliability
 *  decision branches on.  **adapter_class** is tested for a display
 *  controller; a bus whose adapter is not one is rejected outright.
 *
 *  #Sysfs_I2C_Info carries five further fields -- busno, name, adapter_path,
 *  driver_version, conflicting_driver_names -- that this path never reads.
 *  Two of them cost a sysfs attribute read apiece, and collecting
 *  conflicting_driver_names costs two directory walks per bus, so a collector
 *  filling only this struct is cheaper than one filling the full record.
 */
typedef struct {
   char * driver;
   char * adapter_class;
} Sysfs_Basic_I2C_Info;

Sysfs_Basic_I2C_Info get_basic_i2c_info(int busno);
void                 free_sysfs_basic_i2c_info_contents(Sysfs_Basic_I2C_Info info);

void init_i2c_sysfs_base();


#ifdef FOR_FUTURE_USE
typedef struct {
   char * connector;
   int    busno;
   Display_Ref * dref;    // currently
}   Connector_Busno_Dref;
extern GPtrArray * cbd_table;
typedef GPtrArray Connector_Busno_Dref_Table;

Connector_Busno_Dref_Table * create_connector_busnfo_dref_table();
Connector_Busno_Dref * new_cbd0(int busno);
Connector_Busno_Dref * new_cbd(const char * connector, int busno);
Connector_Busno_Dref * get_cbd_by_connector(const char * connector);
Connector_Busno_Dref * get_cbd_by_busno(int busno);
// if dref != NULL, replaces, if NULL, just erases
void                   set_cbd_connector(Connector_Busno_Dref * cbd, Display_Ref * dref);
void dbgrpt_cbd_table(Connector_Busno_Dref_Table * cbd_table, int depth);
#endif

#endif /* SYSFS_BASE_H_ */
