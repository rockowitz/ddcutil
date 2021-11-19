// sysfs_filter_functions.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// #include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "sysfs_util.h"

#include "sysfs_filter_functions.h"


//
// Filename_Filter_Func
//

bool
starts_with_card(const char * value) {
   return str_starts_with(value, "card");
}

bool predicate_cardN(const char * value) {
   return str_starts_with(value, "card");
}

bool drm_filter(const char * name) {
   return str_starts_with(name, "card") && strlen(name) > 5;
}

bool startswith_i2c(const char * value) {
   return str_starts_with(value, "i2c-");
}

bool class_display_device_predicate(const char * value) {
   return str_starts_with(value, "0x03");
}


//
// Dir_Filter_Func
//

#ifdef MAYBE_FUTURE
bool dirname_starts_with(const char * dirname, const char * val) {
   bool debug = false;
   DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, val);
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
#endif

// for e.g. i2c-3
bool is_i2cN(const char * dirname, const char * val) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, val_fn=%s", dirname, val);
   bool result = str_starts_with(dirname, "i2c-");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_drm_dp_aux_subdir(const char * dirname, const char * val) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, val=%s", dirname, val);
   bool result = str_starts_with(dirname, "drm_dp_aux");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

// for e.g. card0-DP-1
bool is_card_connector_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "card");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

// for e.g. card0
bool is_cardN_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "card");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_drm_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = streq(simple_fn, "drm");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}

bool is_i2cN_dir(const char * dirname, const char * simple_fn) {
   // bool debug = false;
   // DBGMSF(debug, "dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = str_starts_with(simple_fn, "i2c-");
   // DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}


// does dirname/simple_fn have attribute class with value display controller or docking station?
bool has_class_display_or_docking_station(
      const char * dirname, const char * simple_fn)
{
   // bool debug = false;
   bool result = false;
   // DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   char * class_val = NULL;
   int    iclass = 0;
   int    top_byte = 0;
   if ( GET_ATTR_TEXT(&class_val, dirname, simple_fn, "class") ) {
      if (str_to_int(class_val, &iclass, 16) ) {
         top_byte = iclass >> 16;
         if (top_byte == 0x03 || top_byte == 0x0a)  // display controller or docking station
            result = true;
      }
   }
   // DBGMSF(debug, "class_val = %s, top_byte = 0x%02x, result=%s",
   //               class_val, top_byte, sbool(result) );
   return result;
}

// for e.g. card0-HDMI-0
bool is_sysfs_drm_connector_dir_name(const char * dirname, const char * simple_fn) {
   bool debug = false;
   if (debug)
      printf("(%s) dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);

   bool result = false;
   if (str_starts_with(simple_fn, "card")) {
      char * hyphen_loc = strchr(simple_fn+4, '-');
      if (hyphen_loc) {
         // todo: test for card number
         result = true;
      }
   }

   if (debug)
      printf("(%s) Returning %s\n", __func__, sbool(result));
   return result;
}




