/** \file demo_watch_displays.c
 *
 * Sample program that watches for changes to attached displays.
 */

// Copyright (C) 2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"


int main(int argc, char** argv) {
   ddca_add_traced_function("watch_displays");

   while (true) {
      usleep(60 * 1000*1000);  // some long interval, just to keep alive
      printf(".");
      fflush(stdout);
   }
}
