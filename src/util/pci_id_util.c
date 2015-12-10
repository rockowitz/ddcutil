/*  pci_id_util.c
 *
 *  Created on: Dec 9, 2015
 *      Author: rock
 */

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>
#include <limits.h>
#include <sys/stat.h>
// #include <libosinfo-1.0/osinfo/osinfo.h>

#include "util/file_util.h"
#include "util/string_util.h"

// #include "base/linux_errno.h"

// #include "base/util.h"

#include "util/pci_id_util.h"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif


// better: use find command

char * find_pci_ids() {
   char * known_pci_ids_dirs[] = {
         "/usr/share/libosinfo/db",
         "/usr/share",
         "/usr/share/misc",
         "/usr/share/hwdata",
         NULL
   };

   char * result = NULL;
   int ndx;
   for (ndx=0; known_pci_ids_dirs[ndx] != NULL; ndx++) {
      char fnbuf[MAX_PATH];
      snprintf(fnbuf, MAX_PATH, "%s/%s", known_pci_ids_dirs[ndx], "pci.ids");
      // printf("(%s) Looking for %s\n", __func__, fnbuf);
      struct stat stat_buf;
      int rc = stat(fnbuf, &stat_buf);
      if (rc == 0) {
         result = strdup(fnbuf);
         break;
      }
   }

   // printf("(%s) Returning: %s\n", __func__, result);
   return result;
}



// Poor choice of data structures.   Replace with linked list or hash
// and yet, performance not a problem

static GPtrArray * pci_vendors;

// stats 12/2015:
//   lines in pci.ids:  25,339
//   vendors:        2.066
//   total devices: 11,745
//   subsystem:     10,974


GPtrArray * load_pci_ids(){
   bool debug = false;
   int total_vendors = 0;
   int total_devices = 0;
   int total_subsys  = 0;
   GPtrArray * all_vendors = NULL;

   char * pci_id_dir = find_pci_ids();
   if (pci_id_dir) {
      char pci_id_fn[MAX_PATH];
      snprintf(pci_id_fn, MAX_PATH, pci_id_dir, "pci_ids");
      GPtrArray * all_lines = g_ptr_array_sized_new(30000);
      int linect = file_getlines(pci_id_fn, all_lines);
      if (all_lines) {
         all_vendors = g_ptr_array_sized_new(2800);
         Pci_Id_Vendor * cur_vendor = NULL;
         Pci_Id_Device * cur_device = NULL;
         Pci_Id_Subsys * cur_subsys = NULL;

         assert( linect == all_lines->len);
         int ndx;
         char * a_line;
         bool pci_ids_done = false;    // end of PCI id section seen?
         for (ndx=0; ndx<linect && !pci_ids_done; ndx++) {
            a_line = g_ptr_array_index(all_lines, ndx);
            int tabct = 0;
            while (a_line[tabct] == '\t')
               tabct++;
            if (strlen(rtrim_in_place(a_line+tabct)) == 0 || a_line[tabct] == '#')
               continue;

            switch(tabct) {

            case (0):
               {
                  cur_vendor = calloc(1, sizeof(Pci_Id_Vendor));
                  int ct = sscanf(a_line+tabct, "%4hx %m[^\n]",
                                  &cur_vendor->vendor_id,
                                  &cur_vendor->vendor_name);
                  if (ct != 2) {
                     printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                     // hex_dump(a_line+tabct, strlen(a_line+tabct));
                     free(cur_vendor);
                     cur_vendor = NULL;
                  }
                  else {
                     total_vendors++;
                     cur_vendor->vendor_devices = g_ptr_array_sized_new(20);
                     g_ptr_array_add(all_vendors, cur_vendor);
                     if (cur_vendor->vendor_id == 0xffff)
                        pci_ids_done = true;
                  }
                  break;
               }

            case (1):
               {
                  if (cur_vendor) {     // in case of vendor error
                     cur_device = calloc(1, sizeof(Pci_Id_Device));
                     int ct = sscanf(a_line+tabct, "%4hx %m[^\n]", &cur_device->device_id, &cur_device->device_name);
                     if (ct != 2) {
                        printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                        free(cur_device);
                        cur_device = NULL;
                     }
                     else {
                        total_devices++;
                        cur_device->device_subsystems = g_ptr_array_sized_new(5);
                        g_ptr_array_add(cur_vendor->vendor_devices, cur_device);
                     }
                  }
                  break;
               }

            case (2):
               {
                  if (cur_device) {     // in case of error
                     cur_subsys = calloc(1, sizeof(Pci_Id_Subsys));
                     int ct = sscanf(a_line+tabct, "%4hx %4hx %m[^\n]",
                                     &cur_subsys->subvendor_id,
                                     &cur_subsys->subdevice_id,
                                     &cur_subsys->subsystem_name);
                     if (ct != 3) {
                        printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                        free(cur_subsys);
                        cur_subsys = NULL;
                     }
                     else {
                        total_subsys++;
                        g_ptr_array_add(cur_device->device_subsystems, cur_subsys);
                     }
                  }
                  break;
               }

            default:
               printf("Unexpected number of leading tabs in line: %s\n", a_line);
            } // switch
         }    // line loop
      }       // if (all_lines)
      // to do: call

      g_ptr_array_set_free_func(all_lines, free);
      g_ptr_array_free(all_lines, true);
   }          // if (pci_id_dir)
   if (debug) {
      printf("(%s) Total vendors: %d, total devices: %d, total subsystems: %d\n",
             __func__, total_vendors, total_devices, total_subsys);
   }
   return all_vendors;
}




void report_pci_devices() {
   GPtrArray * all_devices = pci_vendors;
   int total_vendors = 0;
   int total_devices = 0;
   int total_subsys  = 0;
   int vctr, dctr, sctr;
   Pci_Id_Vendor * cur_vendor;
   Pci_Id_Device * cur_device;
   Pci_Id_Subsys * cur_subsys;
   for (vctr=0; vctr < all_devices->len; vctr++) {
      total_vendors++;
      cur_vendor = g_ptr_array_index(all_devices, vctr);
      printf("%04x %s\n", cur_vendor->vendor_id, cur_vendor->vendor_name);
      for (dctr=0; dctr<cur_vendor->vendor_devices->len; dctr++) {
         total_devices++;
         cur_device = g_ptr_array_index(cur_vendor->vendor_devices, dctr);
         printf("\t%04x %s\n", cur_device->device_id, cur_device->device_name);
         for (sctr=0; sctr<cur_device->device_subsystems->len; sctr++) {
            total_subsys++;
            cur_subsys = g_ptr_array_index(cur_device->device_subsystems, sctr);
            printf("\t\t%04x %04x %s\n",
                   cur_subsys->subvendor_id, cur_subsys->subdevice_id, cur_subsys->subsystem_name);
         }
      }
   }
   printf("(%s) Total vendors: %d, total devices: %d, total subsystems: %d\n",
          __func__, total_vendors, total_devices, total_subsys);
}


bool init_pci_ids() {
   bool debug = false;
   if (!pci_vendors)
      pci_vendors = load_pci_ids();
   bool ok = (pci_vendors);
   if (ok && debug)
      report_pci_devices();
   return ok;
}



Pci_Id_Vendor * pci_id_find_vendor(ushort vendor_id) {
   int ndx = 0;
   Pci_Id_Vendor * result = NULL;
   for (ndx=0; ndx<pci_vendors->len; ndx++) {
      Pci_Id_Vendor * cur_vendor = g_ptr_array_index(pci_vendors, ndx);
      if (cur_vendor->vendor_id == vendor_id) {
         result = cur_vendor;
         break;
      }
   }
   return result;
}



Pci_Id_Device * pci_id_find_device(Pci_Id_Vendor * cur_vendor, ushort device_id) {
   int ndx = 0;
   Pci_Id_Device * result = NULL;
   for (ndx=0; ndx<cur_vendor->vendor_devices->len; ndx++) {
      Pci_Id_Device * cur_device = g_ptr_array_index(cur_vendor->vendor_devices, ndx);
      if (cur_device->device_id == device_id) {
         result = cur_device;
         break;
      }
   }
   return result;
}


Pci_Id_Subsys * pci_id_find_subsys(Pci_Id_Device * cur_device, ushort subvendor_id, ushort subdevice_id) {
   int ndx = 0;
   Pci_Id_Subsys * result = NULL;
   for (ndx=0; ndx<cur_device->device_subsystems->len; ndx++) {
      Pci_Id_Subsys * cur_subsys = g_ptr_array_index(cur_device->device_subsystems, ndx);
      if (cur_subsys->subvendor_id == subvendor_id && cur_subsys->subdevice_id == subdevice_id) {
         result = cur_subsys;
         break;
      }
   }
   return result;
}



// sadly, both 0000 and ffff are used as ids, so can't use them as special arguments for "not set"

Pci_Id_Names pci_id_get_names(
                ushort vendor_id,
                ushort device_id,
                ushort subvendor_id,
                ushort subdevice_id,
                int argct)
{
   bool debug = false;
   if (debug) {
      printf("(%s) vendor_id = %02x, device_id=%02x, subvendor_id=%02x, subdevice_id=%02x\n",
             __func__,
             vendor_id, device_id, subvendor_id, subdevice_id);
   }
   assert( argct==1 || argct==2 || argct==4);
   Pci_Id_Names names = {NULL, NULL, NULL};
   Pci_Id_Vendor * vendor = pci_id_find_vendor(vendor_id);
   if (vendor) {
      names.vendor_name = vendor->vendor_name;
      if (argct > 1) {
         Pci_Id_Device * device = pci_id_find_device(vendor, device_id);
         if (device) {
            names.device_name = device->device_name;
            if (argct == 4) {
               Pci_Id_Subsys * subsys = pci_id_find_subsys(device, subvendor_id, subdevice_id);
               if (subsys)
                  names.subsys_name = subsys->subsystem_name;
               else {
                  Pci_Id_Vendor * subsys_vendor = pci_id_find_vendor(subvendor_id);
                  if (subsys_vendor)
                     names.subsys_name = subsys_vendor->vendor_name;
               }
            }
         }
      }
   }
   return names;
}


