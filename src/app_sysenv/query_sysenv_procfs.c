/** @file query_sysenv_procfs.c
 *
 *  Query environment using /proc file system
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
/** \endcore */

#include "query_sysenv_base.h"

#include "query_sysenv_procfs.h"


/** Scans /proc/modules for information on loaded drivers of interest
 */
int query_proc_modules_for_video() {
   bool debug = false;
   DBGMSF0(debug, "Starting.");

   int d1 = 1;
   int rc = 0;

   GPtrArray * garray = g_ptr_array_sized_new(300);
   g_ptr_array_set_free_func(garray, g_free);

   rpt_vstring(0,"Scanning /proc/modules for driver environment...");
   int ct = file_getlines("/proc/modules", garray, true);
   if (ct < 0)
      rc = ct;
   else {
      int ndx = 0;
      for (ndx=0; ndx<garray->len; ndx++) {
         char * curline = g_ptr_array_index(garray, ndx);
         char mod_name[32];
         int  mod_size;
         int  mod_instance_ct;
         char mod_dependencies[500];
         char mod_load_state[10];     // one of: Live Loading Unloading
         char mod_addr[30];
         int piece_ct = sscanf(curline, "%s %d %d %s %s %s",
                               mod_name,
                               &mod_size,
                               &mod_instance_ct,
                               mod_dependencies,
                               mod_load_state,
                               mod_addr);
         if (piece_ct != 6) {
            DBGMSG("Unexpected error parsing /proc/modules.  sscanf returned %d", piece_ct);
         }
         if (streq(mod_name, "drm") ) {
            rpt_vstring(d1,"Loaded drm module depends on:   %s", mod_dependencies);
         }
         else if (streq(mod_name, "video") ) {
            rpt_vstring(d1,"Loaded video module depends on: %s", mod_dependencies);
         }
         else if (exactly_matches_any(mod_name, (const char **) get_known_video_driver_module_names()) >= 0 ) {
            rpt_vstring(d1,"Found video driver module:      %s", mod_name);
         }
         else if ( starts_with_any(mod_name, (const char **) get_prefix_match_names()) >= 0 ) {
            rpt_vstring(d1,"Found other loaded module:      %s", mod_name);
         }
      }
   }
   g_ptr_array_free(garray, true);
   DBGMSF0(debug, "Done.");
   return rc;
}


/** Reports nvidia proprietary driver information by examining
 *  /proc/driver/nvidia.
 */
bool query_proc_driver_nvidia() {
   bool debug = false;
   bool result = false;
   char * dn = "/proc/driver/nvidia/";
   if ( directory_exists(dn) ) {
      rpt_vstring(0,"Examining /proc/driver/nvidia:");
      result = true;
      sysenv_show_one_file(dn, "version",  debug, 1);
      sysenv_show_one_file(dn, "registry", debug, 1);
      sysenv_show_one_file(dn, "params",   debug, 1);
      char * dn_gpus = "/proc/driver/nvidia/gpus/";
      if (directory_exists(dn_gpus)) {
         DIR * dp = opendir(dn_gpus);
         struct dirent * ep;

         while ( (ep = readdir(dp)) ) {
            if ( !streq(ep->d_name,".") && !streq(ep->d_name, "..") ) {
               rpt_vstring(1, "PCI bus id: %s", ep->d_name);
               char dirbuf[400];
               strcpy(dirbuf, dn_gpus);
               strcat(dirbuf, ep->d_name);
               // printf("Reading directory: %s\n", dirbuf);
               // DIR * dp2 = opendir(dirbuf);
               // if (dp2) {
               //    struct dirent * ep2;
               //    printf("GPU: %s\n", ep->d_name);
               //    while ( (ep2 = readdir(dp2)) ) {
               //       if ( !streq(ep2->d_name,".") && !streq(ep2->d_name, "..") ) {
               //          puts(ep2->d_name);
               //       }
               //    }
               //    closedir(dp2);
               // }
               if ( directory_exists(dirbuf)) {
                  sysenv_show_one_file(dirbuf, "information", debug, 1);
                  sysenv_show_one_file(dirbuf, "registry",    debug, 1);
               }
            }
         }
         closedir(dp);
      }
   }
   else {
       DBGMSF(debug, "Nvidia driver directory %s not found\n", dn);
   }
   return result;
}
