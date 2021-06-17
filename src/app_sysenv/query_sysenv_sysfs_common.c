// query_sysenv_sysfs_common.c

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"

#include "query_sysenv_sysfs_common.h"

// Local conversion functions for data coming from sysfs,
// which should always be valid.

ushort h2ushort(char * hval) {
   bool debug = false;
   int ct;
   ushort ival;
   ct = sscanf(hval, "%hx", &ival);
   assert(ct == 1);
   if (debug)
      DBGMSG("hhhh = |%s|, returning 0x%04x", hval, ival);
   return ival;
}

unsigned h2uint(char * hval) {
   bool debug = false;
   int ct;
   unsigned ival;
   ct = sscanf(hval, "%x", &ival);
   assert(ct == 1);
   if (debug)
      DBGMSG("hhhh = |%s|, returning 0x%08x", hval, ival);
   return ival;
}


// does dirname/simple_fn have attribute class with value display controller or docking station?
bool has_class_display_or_docking_station(
      const char * dirname, const char * simple_fn)
{
   bool debug = false;
   bool result = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
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
   DBGMSF(debug, "class_val = %s, top_byte = 0x%02x, result=%s",
                 class_val, top_byte, sbool(result) );
   return result;
}
