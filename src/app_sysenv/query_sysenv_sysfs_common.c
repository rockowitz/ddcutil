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

   ushort ival;
#ifdef NDEBUG
   sscanf(hval, "%hx", &ival);
#else
   assert( sscanf(hval, "%hx", &ival) == 1);
#endif

   DBGMSF(debug, "hhhh = |%s|, returning 0x%04x", hval, ival);
   return ival;
}

unsigned h2uint(char * hval) {
   bool debug = false;

   unsigned ival;
#ifdef NDEBUG
   sscanf(hval, "%x", &ival);
#else
   assert( sscanf(hval, "%x", &ival) == 1);
#endif

   DBGMSF(debug, "hhhh = |%s|, returning 0x%08x", hval, ival);
   return ival;
}

