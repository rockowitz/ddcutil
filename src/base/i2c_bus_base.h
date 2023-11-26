/** @file i2c_bus_base.h
 *
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_BASE_H_
#define I2C_BUS_BASE_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>

#include "util/data_structures.h"
#include "util/edid.h"


// Retrieve and inspect bus information

// Keep in sync with i2c_bus_flags_table
#define I2C_BUS_EXISTS                  0x08
#define I2C_BUS_ACCESSIBLE              0x04
#define I2C_BUS_ADDR_0X50               0x02      ///< detected I2C bus address 0x50
#define I2C_BUS_ADDR_0X37               0x01      ///< detected I2C bus address 0x37
#define I2C_BUS_ADDR_0X30               0x80      ///< detected write-only addr to specify EDID block number
// #define I2C_BUS_EDP                     0x40      ///< bus associated with eDP display
// #define I2C_BUS_LVDS                    0x20      ///< bus associated with LVDS display
// #define I2C_BUS_LAPTOP                  (I2C_BUS_EDP|I2C_BUS_LVDS) ///< bus associated with laptop display
#define I2C_BUS_PROBED                  0x10      ///< has bus been checked?
#define I2C_BUS_VALID_NAME_CHECKED    0x0800
#define I2C_BUS_HAS_VALID_NAME        0x0400
#define I2C_BUS_BUSY                  0x0200      ///< for possible future use
#define I2C_BUS_SYSFS_EDID            0x0100
#define I2C_BUS_DRM_CONNECTOR_CHECKED 0x8000
#define I2C_BUS_LVDS_OR_EDP           0x4000
#define I2C_BUS_APPARENT_LAPTOP       0x2000
#define I2C_BUS_LAPTOP                (I2C_BUS_LVDS_OR_EDP | I2C_BUS_APPARENT_LAPTOP)

typedef enum {
   DRM_CONNECTOR_NOT_CHECKED    = 0,    // ??? needed?
   DRM_CONNECTOR_NOT_FOUND      = 1,
   DRM_CONNECTOR_FOUND_BY_BUSNO = 2,
   DRM_CONNECTOR_FOUND_BY_EDID  = 3
} Drm_Connector_Found_By;

const char * drm_connector_found_by_name(Drm_Connector_Found_By found_by);

#define I2C_BUS_INFO_MARKER "BINF"
/** Information about one I2C bus */
typedef
struct {
   char             marker[4];          ///< always "BINF"
   int              busno;              ///< I2C device number, i.e. N for /dev/i2c-N
   unsigned long    functionality;      ///< i2c bus functionality flags
   Parsed_Edid *    edid;               ///< parsed EDID, if slave address x50 active
   uint16_t         flags;              ///< I2C_BUS_* flags
   char *           driver;             ///< driver name
   int              open_errno;         ///< errno if open fails (!I2C_BUS_ACCESSIBLE)
   char *           drm_connector_name; ///< from /sys
   Drm_Connector_Found_By
                    drm_connector_found_by;
   bool             last_checked_dpms_asleep;
} I2C_Bus_Info;

char *           interpret_i2c_bus_flags(uint16_t flags);
char *           interpret_i2c_bus_flags_t(uint16_t flags);

// Accessors
char * i2c_get_drm_connector_attribute(const I2C_Bus_Info * businfo, const char * attribute);

#define I2C_GET_DRM_DPMS(_businfo) \
   i2c_get_drm_connector_attribute(_businfo, "dpms")

#define I2C_GET_DRM_STATUS(_businfo) \
   i2c_get_drm_connector_attribute(_businfo, "status")

#define I2C_GET_DRM_ENABLED(_businfo) \
   i2c_get_drm_connector_attribute(_businfo, "enabled")

void             i2c_reset_bus_info(I2C_Bus_Info * bus_info);
char *           i2c_get_drm_connector_name(I2C_Bus_Info * bus_info);


// Lifecycle
I2C_Bus_Info *   i2c_new_bus_info(int busno);
void             i2c_free_bus_info(I2C_Bus_Info * businfo);
void             i2c_gdestroy_bus_info(void * data);

void             i2c_update_bus_info(I2C_Bus_Info * existing, I2C_Bus_Info* new_info);

// Generalized Bus_Info retrieval
I2C_Bus_Info *   i2c_find_bus_info_in_gptrarray_by_busno(GPtrArray * buses, int busno);
int              i2c_find_bus_info_index_in_gptrarray_by_busno(GPtrArray * buses, int busno);

void             i2c_dbgrpt_bus_info(I2C_Bus_Info * businfo, int depth);


//
// Detected Buses
//

// shared with i2c_bus_selector.c
extern GPtrArray * i2c_buses;
extern Bit_Set_256 connected_buses;

GPtrArray *      i2c_get_all_buses();
I2C_Bus_Info *   i2c_get_bus_info_by_index(guint busndx);
I2C_Bus_Info *   i2c_find_bus_info_by_busno(int busno);
int              i2c_dbgrpt_buses(bool report_all, int depth);  // Reports all detected i2c buses

void init_i2c_bus_base();

#endif /* I2C_BUS_BASE_H_ */
