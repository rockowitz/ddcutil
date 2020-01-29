/** @file i2c_bus_selector.c
 *
 * Generalized Bus_Info search
 *
 * Overkill for current use.
 * Was coded at the time when selecting display by criteria occurred at the i2c/adn/usb
 * level rather than in ddc_displays.c.
 * Still used by USB layer as a fallback to find the EDID by model etc.
 * if the EDID can't be gotten from USB services.
 */
// Copyright (C) 2018-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>

#include <util/report_util.h>
#include <util/string_util.h>

#include "i2c_bus_selector.h"


typedef struct {
   int           busno;
   const char *  mfg_id;
   const char *  model_name;
   const char *  serial_ascii;
   const Byte *  edidbytes;
   Byte          options;
} I2C_Bus_Selector;


static
void report_i2c_bus_selector(I2C_Bus_Selector * sel, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("I2C_Bus_Selector", sel, depth);
   rpt_int("busno",        NULL, sel->busno, d1);
   rpt_str("mfg_id",       NULL, sel->mfg_id, d1);
   rpt_str("model_name",   NULL, sel->model_name, d1);
   rpt_str("serial_ascii", NULL, sel->serial_ascii, d1);
   rpt_structure_loc("edidbytes", sel->edidbytes, d1);
   if (sel->edidbytes)
      rpt_hex_dump(sel->edidbytes, 128, d2);
}

static
void init_i2c_bus_selector(I2C_Bus_Selector* sel) {
   assert(sel);
   memset(sel, 0, sizeof(I2C_Bus_Selector));
   sel->busno = -1;
}


// Note: No need for free_i2c_bus_selector() function since strings and memory
// pointed to by selector are always in other data structures


/* Tests if a bus_info table entry matches the criteria of a selector.
 *
 * Arguments:
 *   bus_info    pointer to Bus_Info instance to test
 *   sel         selection criteria
 *
 * Returns:      true/false
 */
static
bool bus_info_matches_selector(I2C_Bus_Info * bus_info, I2C_Bus_Selector * sel) {
   bool debug = false;
   if (debug) {
      DBGMSG("Starting");
      i2c_dbgrpt_bus_info(bus_info, 1);
   }

   assert( bus_info && sel);
   assert( sel->busno >= 0   ||
           sel->mfg_id       ||
           sel->model_name   ||
           sel->serial_ascii ||
           sel->edidbytes);

   bool result = false;
   // does the bus represent a valid display?
   // 8/2018: This function is called only (indirectly) from get_fallback_hidev_edid()
   // in usb_edid.c to get the EDID for an EIZO display communicated with using USB.
   // DISPSEL_VALID_ONLY is not set in that case.
   if (sel->options & DISPSEL_VALID_ONLY) {
#ifdef DETECT_SLAVE_ADDRS
      if (!(bus_info->flags & I2C_BUS_ADDR_0X37))
         goto bye;
#endif
   }
   bool some_test_passed = false;

   if (sel->busno >= 0) {
      DBGMSF(debug, "bus_info->busno = %d", bus_info->busno);
      if (sel->busno != bus_info->busno)  {
         result = false;
         goto bye;
      }
      DBGMSF(debug, "busno test passed");
      some_test_passed = true;
   }

   Parsed_Edid * edid = bus_info->edid;  // will be NULL for I2C bus with no monitor

   if (sel->mfg_id && strlen(sel->mfg_id) > 0) {
      if ((!edid) || strlen(edid->mfg_id) == 0 || !streq(sel->mfg_id, edid->mfg_id) ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (sel->model_name && strlen(sel->model_name) > 0) {
      if ((!edid) || strlen(edid->model_name) == 0 || !streq(sel->model_name, edid->model_name) ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (sel->serial_ascii && strlen(sel->serial_ascii) > 0) {
      if ((!edid) || strlen(edid->serial_ascii) == 0 || !streq(sel->serial_ascii, edid->serial_ascii) ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (sel->edidbytes) {
      if ((!edid) || !memcmp(sel->edidbytes, edid->bytes, 128) != 0  ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (some_test_passed)
      result = true;

bye:
   DBGMSF(debug, "Returning: %s", bool_repr(result));
   return result;
}


/* Finds the first Bus_Info instance that matches a selector
 *
 * Arguments:
 *   sel       pointer to selection criteria
 *
 * Returns:    pointer to Bus_Info instance if found, NULL if not
 */
static
I2C_Bus_Info * find_bus_info_by_selector(I2C_Bus_Selector * sel) {
   assert(sel);
   bool debug = false;
   if (debug) {
      DBGMSG("Starting.");
      report_i2c_bus_selector(sel, 1);
   }

   I2C_Bus_Info * bus_info = NULL;
   assert(i2c_buses);
   int busct = i2c_buses->len;

   for (int ndx = 0; ndx < busct; ndx++) {
      I2C_Bus_Info * cur_info = g_ptr_array_index(i2c_buses, ndx);
      if (bus_info_matches_selector(cur_info, sel)) {
         bus_info = cur_info;
         break;
      }
   }

    DBGMSF(debug, "returning %p", bus_info );
    if (debug && bus_info) {
       i2c_dbgrpt_bus_info(bus_info, 1);
    }
    return bus_info;
 }


// Finally, functions that use the generalized bus selection mechanism

/** Retrieves bus information by some combination of the monitor's
 * mfg id, model name and/or serial number.
 *
 *  @param  mfg_id  3 character manufacturer id
 *  @param  model     monitor model (as listed in the EDID)
 *  @param  sn        monitor ascii serial number (as listed in the EDID)
 *  @param  findopts  selector options
 *
 * @return pointer to Bus_Info struct for the bus,\n
 *         NULL if not found
 *
 * @remark used by get_fallback_hiddev_edid() in usb_edid.c
 */
I2C_Bus_Info *
i2c_find_bus_info_by_mfg_model_sn(
      const char * mfg_id,
      const char * model,
      const char * sn,
      Byte findopts)
{
   bool debug = false;
   DBGMSF(debug, "Starting. mfg_id=|%s|, model=|%s|, sn=|%s|", mfg_id, model, sn );
   assert(mfg_id || model || sn);    // loosen the requirements

   I2C_Bus_Selector sel;
   init_i2c_bus_selector(&sel);
   sel.mfg_id       = mfg_id;
   sel.model_name   = model;
   sel.serial_ascii = sn;
   sel.options      = findopts;
   I2C_Bus_Info * result = find_bus_info_by_selector(&sel);

   DBGMSF(debug, "Returning: %p", result );
   return result;
}
