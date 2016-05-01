/* pci_id_util.c
 *
 * Created on: Dec 9, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
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
#include "util/report_util.h"

// #include "base/linux_errno.h"

// #include "base/util.h"

#include "util/pci_id_util.h"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif



char * simple_device_fn[] = {
      "pci.ids",
      "usb.ids"
};



/* Returns fully qualified file name of device id file.
 *
 * Arguments:
 *    id_type     ID_TYPE_PCI or ID_TYPE_USB
 *
 * Returns:   fully qualified file name of device id file,
 *            NULL if not found
 *            It is the responsibility of the caller to free
 *            this value
 */
char * find_id_file(Device_Id_Type id_type) {
   bool debug = false;

   char * known_pci_ids_dirs[] = {
         "/usr/share/libosinfo/db",
         "/usr/share",
         "/usr/share/misc",
         "/usr/share/hwdata",
         NULL
   };

   // better: use find command ??

   char * id_fn = simple_device_fn[id_type];
   if (debug)
      printf("(%s) id_type=%d, id_fn = |%s|\n", __func__, id_type, id_fn);

   char * result = NULL;
   int ndx;
   for (ndx=0; known_pci_ids_dirs[ndx] != NULL; ndx++) {
      char fnbuf[MAX_PATH];
      snprintf(fnbuf, MAX_PATH, "%s/%s", known_pci_ids_dirs[ndx], id_fn);
      if (debug)
         printf("(%s) Looking for |%s|\n", __func__, fnbuf);
      struct stat stat_buf;
      int rc = stat(fnbuf, &stat_buf);
      if (rc == 0) {
         result = strdup(fnbuf);
         break;
      }
   }

   if (debug)
      printf("(%s) id_type=%d, Returning: %s\n", __func__, id_type, result);
   return result;
}



// Poor choice of data structures.   Replace with linked list or hash
// and yet, performance not a problem

static GPtrArray * pci_vendors;
static GPtrArray * usb_vendors;
static GPtrArray * hid_descriptor_types;       // tag HID
static GPtrArray * hid_descriptor_item_types;  // tag R
static GPtrArray * hid_country_codes;          // tag HCC - for keyboards



static void
load_simple_ids(
      GPtrArray * simple_table,
      GPtrArray * all_lines,
      char * segment_tag,
      int cur_pos,
      int * end_pos)
{
   bool debug = false;
   assert(simple_table);
   if (debug) {
      printf("(%s) Starting. curpos=%d, -> |%s|\n",
             __func__, cur_pos, (char *) g_ptr_array_index(all_lines,cur_pos));
   }
   // char * cur_tag = '\0';
   bool more = true;
   int linect = all_lines->len;
   while (more && cur_pos < linect) {
      char * a_line = g_ptr_array_index(all_lines, cur_pos++);
      rtrim_in_place(a_line);
      // printf("(%s) curpos=%d, a_line=|%s|\n", __func__, cur_pos-1, a_line);
      if ( strlen(a_line) == 0 || a_line[0] == '#') {
         // printf("(%s) Comment line\n", __func__);
         continue;
      }
      // split into tag hexvalue  name
      char   atag[40];
      ushort acode;
      char*   aname;
      /* int ct = */ sscanf(a_line, "%s %hx %m[^\n]",
                          atag,
                          &acode,
                          &aname);
      // printf("(%s) ct = %d, atag=|%s|, acode=0x%08x, aname=|%s|\n",
      //        __func__, ct, atag, acode, aname);
      if (!streq(atag, segment_tag)) {
         // printf("(%s) Tag doesn't match\n", __func__);
         break;
      }

      Id_Simple_Table_Entry * cur_entry = calloc(1,sizeof(Id_Simple_Table_Entry));
      cur_entry->id   = acode;
      cur_entry->name = strdup(aname);
      // printf("(%s) Created new entry for id=0x%08x, name=|%s|\n",
      //       __func__, cur_entry->id, cur_entry->name);
      g_ptr_array_add(simple_table, cur_entry);
   }

   if (cur_pos <= linect)
      cur_pos--;
   *end_pos = cur_pos;
   if (debug)
      printf("(%s) Set end_pos = %d\n", __func__, cur_pos);
}


void report_simple_ids(GPtrArray * simple_table, int depth) {
   rpt_structure_loc("Simple ids table", simple_table, depth);
   for (int ndx = 0; ndx < simple_table->len; ndx++) {
      Id_Simple_Table_Entry * cur_entry = g_ptr_array_index(simple_table, ndx);
      rpt_vstring(depth+1, "0x%04x -> |%s|", cur_entry->id, cur_entry->name);
   }
}


typedef struct {
   ushort   code;
   char *   name;
   GPtrArray * children;
} Node_Entry;

#ifdef OLD
typedef struct {
   ushort   code;
   char *   name;
} Leaf_Entry;
#endif

typedef struct {
   char *      name;
   int         initial_size;
   int         total_entries;
   Node_Entry * cur_entry;
} MLT_Level;


typedef struct {
   char*       table_name;
   char*       segment_tag;
   int         levels;
   GPtrArray * root;
   MLT_Level   level_detail[];
} Multi_Level_Table;


Multi_Level_Table hid_usages_table = {
      .table_name = "HID usages",
      .segment_tag = "HUT",
      .levels = 2,
      .level_detail = {
            {"usage_page", 20, 0},
            {"usage_id", 40, 0}
      }
};


void mlt_cur_entries(Multi_Level_Table * mlt) {
   int d1 = 1;
   rpt_vstring(0, "Multi_Level_Table.  levels=%d", mlt->levels);
   for (int ndx=0; ndx < mlt->levels; ndx++) {
      rpt_vstring(d1, "  mlt->level_detail[%d].cur_entry=%p, addr of entry=%p",
                      ndx, mlt->level_detail[ndx].cur_entry,  &mlt->level_detail[ndx].cur_entry);
   }
}

static /* GPtrArray */  Multi_Level_Table * load_multi_level_table(
      Multi_Level_Table * header,
      GPtrArray *       all_lines,
      int*              curpos)
{
   bool debug = false;
   int linendx = *curpos;
   if (debug)
      printf("(%s) Starting. linendx=%d, -> |%s|\n",
             __func__, linendx, (char *) g_ptr_array_index(all_lines,linendx));

   for (int ndx = 0; ndx < header->levels; ndx++) {
      header->level_detail[ndx].total_entries = 0;
      header->level_detail[ndx].cur_entry = NULL;
   }
   header->root = g_ptr_array_sized_new(header->level_detail[0].initial_size);

   bool more = true;
   while (more && linendx < all_lines->len) {
      char * a_line = g_ptr_array_index(all_lines, linendx++);
      int tabct = 0;
      while (a_line[tabct] == '\t')
         tabct++;
      if (strlen(rtrim_in_place(a_line+tabct)) == 0 || a_line[tabct] == '#')
         continue;

      // MLT_Level * lvl_detail = &header->level_detail[tabct];
      header->level_detail[tabct].cur_entry = calloc(1, sizeof(Node_Entry));
      // printf("Created new Node_Entry.  Set header->level_detail[tabct].cur_entry (addr %p) to %p\n",
      //        &(header->level_detail[tabct].cur_entry), header->level_detail[tabct].cur_entry);

      if (tabct == 0) {
         char cur_tag[40];
         int ct = sscanf(a_line+tabct, "%s %4hx %m[^\n]",
                              cur_tag,
                              &header->level_detail[tabct].cur_entry->code,
                              &header->level_detail[tabct].cur_entry->name );
         if (!streq(cur_tag, header->segment_tag)) {
            free(header->level_detail[tabct].cur_entry);
            // segment_done = true;
            // more = false;   // needed for continue
            break;     // or continue?
         }
         if (ct != 3) {
            printf("(%s) Error processing line %d: \"%s\"\n", __func__, linendx, a_line);
            printf("(%s) Lines has %d fields, not 3.  Ignoring\n", __func__, ct);
            // hex_dump(a_line+tabct, strlen(a_line+tabct));
            free(header->level_detail[tabct].cur_entry);
            header->level_detail[tabct].cur_entry = NULL;
         }
         else {
            header->level_detail[tabct].total_entries++;
            if (tabct < header->levels-1)  // if not a leaf
               header->level_detail[tabct].cur_entry->children = g_ptr_array_sized_new(20);
            g_ptr_array_add(header->root, header->level_detail[tabct].cur_entry);
            for (int lvl = tabct+1; lvl < header->levels; lvl++)
                header->level_detail[lvl].cur_entry = NULL;
            // printf("Successful level 0 node added. set header->level_detail[%d].cur_entry (addr %p) to %p\n",
            //        tabct, &(header->level_detail[tabct].cur_entry), header->level_detail[tabct].cur_entry   );
            // mlt_cur_entries(header);
        }
     }

     else  {    // intermediate or leaf node
        if (!header->level_detail[tabct-1].cur_entry) {   // bad data, issue warning and ignore
           printf("Error processing line %d: \"%s\"\n", linendx-1, a_line);
           printf("No enclosing level %d node\n", tabct-1);
           // printf("(A) tabct=%d\n", tabct);
           // mlt_cur_entries(header);
           free(header->level_detail[tabct].cur_entry);
           header->level_detail[tabct].cur_entry = NULL;

        }
        else {
           int ct = sscanf(a_line+tabct, "%4hx  %m[^\n]",
                            &header->level_detail[tabct].cur_entry->code,
                            &header->level_detail[tabct].cur_entry->name);
            if (ct != 2) {
               printf("(%s) Error reading line %d: %s\n",
                      __func__, linendx-1, a_line);
               free(header->level_detail[tabct].cur_entry);
               header->level_detail[tabct].cur_entry = NULL;
            }
            else {
               header->level_detail[tabct].total_entries++;
               if (tabct < header->levels-1)  // if not a leaf
                  header->level_detail[tabct].cur_entry->children = g_ptr_array_sized_new(20);
               g_ptr_array_add(header->level_detail[tabct-1].cur_entry->children, header->level_detail[tabct].cur_entry);
            }
        }
     }     // intermediate or leaf node
   }       // while loop over lines

   if (debug) {
      for (int lvlndx=0; lvlndx<header->levels; lvlndx++) {
         printf("(%s) Table %s (tag %s): total level %d (%s) nodes: %d\n",
             __func__,
             header->table_name,
             header->segment_tag,
             lvlndx,
             header->level_detail[lvlndx].name,
             header->level_detail[lvlndx].total_entries);
      }
   }

   *curpos = linendx;
   return header;
}


void report_multi_level_table_node(Multi_Level_Table * header, int level, Node_Entry * entry, int depth) {
   // MLT_Level level_detail = header->level_detail[level];
   rpt_vstring(depth, "%04x  %s", entry->code, entry->name);
   if (entry->children) {
      for (int ndx=0; ndx<entry->children->len; ndx++) {
         report_multi_level_table_node(
               header,
               level+1,
               g_ptr_array_index(entry->children, ndx),
               depth+1);
      }
   }
}




void report_multi_level_table(Multi_Level_Table * header, int depth) {
      // bool debug = true;
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Multi_Level_Table", header, depth);
   rpt_vstring(d1, "%-20s:  %s", "Table",       header->table_name);
   rpt_vstring(d1, "%-20s:  %s", "Segment tag", header->segment_tag);

   for (int ndx=0; ndx < header->root->len; ndx++) {
      report_multi_level_table_node(
            header,
            0,
            g_ptr_array_index(header->root, ndx),
            d2);
   }
}


// stats 12/2015:
//   lines in pci.ids:  25,339
//   vendors:            2,066
//   total devices:     11,745
//   subsystem:         10,974


int find_next_segment_start(GPtrArray* lines, int cur_ndx, char* segment_tag) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting cur_ndx=%d, segment_tag=|%s|\n", __func__, cur_ndx, segment_tag);

   for(; cur_ndx < lines->len; cur_ndx++) {
      char * a_line = g_ptr_array_index(lines, cur_ndx);
      int tabct = 0;
      while (a_line[tabct] == '\t')
         tabct++;
      rtrim_in_place(a_line+tabct);
      if (strlen(a_line+tabct) == 0 || a_line[tabct] == '#')
         continue;    // always skip comment and blank lines
      if (segment_tag) {  // skipping the current segment
         if (tabct == 0) {
            char   atag[40];
            char*  rest;
            /* int ct = */ sscanf(a_line, "%s %m[^\n]",
                             atag,
                             &rest);
            // printf("(%s) ct = %d\n", __func__, ct);
            if (!streq(atag,segment_tag)) {
               strcpy(segment_tag, atag);
               free(rest);
               break;
            }
         }
      }
   }
   return cur_ndx;
}




static void load_device_ids(Device_Id_Type id_type){
   bool debug = false;
   int total_vendors = 0;
   int total_devices = 0;
   int total_subsys  = 0;
   GPtrArray * all_vendors = NULL;

   if (debug)
      printf("(%s) id_type=%d\n", __func__, id_type);

   // char * id_fn = simple_device_fn[id_type];
   char * device_id_fqfn = find_id_file(id_type);
   if (device_id_fqfn) {
      // char device_id_fqfn[MAX_PATH];
      // snprintf(device_id_fqfn, MAX_PATH, id_fqfn, id_fn);  // ???
      if (debug)
         printf("(%s) device_id_fqfn = %s\n", __func__, device_id_fqfn);

      GPtrArray * all_lines = g_ptr_array_sized_new(30000);
      int linect = file_getlines(device_id_fqfn, all_lines, true);
      if (linect > 0) {
         all_vendors = g_ptr_array_sized_new(2800);
         Pci_Id_Vendor * cur_vendor = NULL;
         Pci_Id_Device * cur_device = NULL;
         Pci_Id_Subsys * cur_subsys = NULL;

         assert( linect == all_lines->len);
         int linendx;
         char * a_line;
         bool pci_ids_done = false;    // end of PCI id section seen?
         for (linendx=0; linendx<linect && !pci_ids_done; linendx++) {
            a_line = g_ptr_array_index(all_lines, linendx);
            int tabct = 0;
            while (a_line[tabct] == '\t')
               tabct++;
            if (strlen(rtrim_in_place(a_line+tabct)) == 0 || a_line[tabct] == '#')
               continue;
            if (id_type == ID_TYPE_USB) {
               // hacky test for end of id section
               if (memcmp(a_line+tabct, "C", 1) == 0) {
                  pci_ids_done = true;
                  break;
               }
            }

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
                     // usb.ids has no final ffff field, test works only for pci.ids
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
                  if (cur_device) {
                     if (id_type == ID_TYPE_PCI) {
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
                     }  // ID_TYPE_PCI
                     else {     // ID_TYPE_USB
                        cur_subsys = calloc(1, sizeof(Pci_Id_Subsys));
                        int ct = sscanf(a_line+tabct, "%4hx  %m[^\n]",
                                        &cur_subsys->subvendor_id,
                                        &cur_subsys->subsystem_name);
                        if (ct != 2) {
                           printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                           free(cur_subsys);
                           cur_subsys = NULL;
                        }
                        else {
                           total_subsys++;
                           g_ptr_array_add(cur_device->device_subsystems, cur_subsys);
                        }
                     }  //ID_TYPE_USB
                  }  // if (cur_device)
                  break;
               }

            default:
               printf("Unexpected number of leading tabs in line: %s\n", a_line);
            } // switch
         }    // line loop

         if (id_type == ID_TYPE_PCI)
            pci_vendors = all_vendors;
         else
            usb_vendors = all_vendors;


         // if usb.ids, look for additional segments
         if (id_type == ID_TYPE_USB) {
            a_line = g_ptr_array_index(all_lines, linendx);
            // printf("(%s) First line of next segment: |%s|, linendx=%d\n", __func__, a_line, linendx);
            linendx--; //  start looking on comment line before segment
            a_line = g_ptr_array_index(all_lines, linendx);
            // printf("(%s) First line of next segment: |%s|, linendx=%d\n", __func__, a_line, linendx);
#define MAX_TAG_SIZE 40
            char tagbuf[MAX_TAG_SIZE];
            tagbuf[0] = '\0';

            while (linendx < all_lines->len) {
               //printf("(%s) Before find_next_segment_start(), linendx=%d: |%s|\n",
               //       __func__, linendx, (char *) g_ptr_array_index(all_lines, linendx));
               linendx = find_next_segment_start(all_lines, linendx, tagbuf);
               // printf("(%s) Next segment starts at line %d: |%s|\n",
               //        __func__, linendx, (char *) g_ptr_array_index(all_lines, linendx));
               if (linendx >= all_lines->len)
                  break;

               if ( streq(tagbuf,"HID") ) {
                  hid_descriptor_types = g_ptr_array_new();
                  load_simple_ids(hid_descriptor_types, all_lines, tagbuf, linendx, &linendx);
                  // printf("(%s) After HID, linendx=%d\n", __func__, linendx);
                  // rpt_title("hid_descriptor_types: ", 0);
                  // report_simple_ids(hid_descriptor_types, 1);
               }
               else if ( streq(tagbuf,"R") ) {
                   hid_descriptor_item_types = g_ptr_array_new();
                   load_simple_ids(hid_descriptor_item_types, all_lines, tagbuf, linendx, &linendx);
                   // printf("(%s) After R, linendx=%d\n", __func__, linendx);
                   // rpt_title("hid_descriptor_item_types: ", 0);
                   // report_simple_ids(hid_descriptor_item_types, 1);
                }
               else if ( streq(tagbuf,"HCC") ) {
                   hid_country_codes = g_ptr_array_new();
                   load_simple_ids(hid_country_codes, all_lines, tagbuf, linendx, &linendx);
                   // printf("(%s) After HCC, linendx=%d\n", __func__, linendx);
                   // rpt_title("hid_country_codes: ", 0);
                   // report_simple_ids(hid_country_codes, 1);
                }

                else if ( streq(tagbuf,"HUT") ) {
                   load_multi_level_table(&hid_usages_table, all_lines, &linendx);
                   // printf("(%s) After HUT, linendx=%d\n", __func__, linendx);
                   // rpt_title("usages table: ", 0);
                   // report_multi_level_table(&hid_usages_table, 1);
                }
            }
         }

      }       // if (all_lines)
      // to do: call

      g_ptr_array_set_free_func(all_lines, free);
      g_ptr_array_free(all_lines, true);
   }          // if pci.ids or usb.ids was found
   if (debug) {
      char * level3_name = (id_type == ID_TYPE_PCI) ? "subsystems" : "interfaces";
      printf("(%s) Total vendors: %d, total devices: %d, total %s: %d\n",
             __func__, total_vendors, total_devices, level3_name, total_subsys);
   }
   return;
}




void report_device_ids(Device_Id_Type id_type) {
   // bool debug = true;

   GPtrArray * all_devices = (id_type == ID_TYPE_PCI) ? pci_vendors : usb_vendors;
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
            if (id_type == ID_TYPE_PCI)
               printf("\t\t%04x %04x %s\n",
                      cur_subsys->subvendor_id, cur_subsys->subdevice_id, cur_subsys->subsystem_name);
            else
               printf("\t\t%04x %s\n",
                      cur_subsys->subvendor_id, cur_subsys->subsystem_name);
         }
      }
   }
   char * level3_name = (id_type == ID_TYPE_PCI) ? "subsystems" : "interfaces";
   printf("(%s) Total vendors: %d, total devices: %d, total %s: %d\n",
          __func__, total_vendors, total_devices, level3_name, total_subsys);
}


bool pciusb_id_ensure_initialized() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__);
   if (!pci_vendors) {
      load_device_ids(ID_TYPE_PCI);
      load_device_ids(ID_TYPE_USB);
   }
   bool ok = (pci_vendors);
   if (ok && debug) {
      // report_device_ids(ID_TYPE_PCI);
      // report_device_ids(ID_TYPE_USB);
   }
   return ok;
}



Pci_Id_Vendor * pciusb_id_find_vendor(ushort vendor_id, Device_Id_Type id_type) {
   pciusb_id_ensure_initialized();
   bool debug = false;
   int ndx = 0;
   GPtrArray * all_vendors = (id_type == ID_TYPE_PCI) ? pci_vendors : usb_vendors;
   Pci_Id_Vendor * result = NULL;
   for (ndx=0; ndx<all_vendors->len; ndx++) {
      Pci_Id_Vendor * cur_vendor = g_ptr_array_index(all_vendors, ndx);
      // printf("(%s) Comparing cur_vendor=0x%04x\n", __func__, cur_vendor->vendor_id);
      if (cur_vendor->vendor_id == vendor_id) {
         result = cur_vendor;
         break;
      }
   }
   if (debug) {
   printf("(%s) id_type=%d, vendor_id=0x%02x, returning %p\n",
          __func__,
          id_type,
          vendor_id,
          result);
   }
   return result;
}


Pci_Id_Vendor * pci_id_find_vendor(ushort vendor_id) {
   return pciusb_id_find_vendor(vendor_id, ID_TYPE_PCI);
}

Pci_Id_Vendor * usb_id_find_vendor(ushort vendor_id) {
   return pciusb_id_find_vendor(vendor_id, ID_TYPE_USB);
}



Pci_Id_Device * pci_id_find_device(Pci_Id_Vendor * cur_vendor, ushort device_id) {
   pciusb_id_ensure_initialized();
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

Pci_Id_Device * usb_id_find_device(Pci_Id_Vendor * cur_vendor, ushort device_id) {
   return pci_id_find_device(cur_vendor, device_id);
}


Pci_Id_Subsys * pci_id_find_subsys(Pci_Id_Device * cur_device, ushort subvendor_id, ushort subdevice_id) {
   pciusb_id_ensure_initialized();
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

Pci_Id_Subsys * usb_id_find_interface(Pci_Id_Device * cur_device, ushort interface_id) {
   pciusb_id_ensure_initialized();
   int ndx = 0;
   Pci_Id_Subsys * result = NULL;
   for (ndx=0; ndx<cur_device->device_subsystems->len; ndx++) {
      Pci_Id_Subsys * cur_subsys = g_ptr_array_index(cur_device->device_subsystems, ndx);
      if (cur_subsys->subvendor_id == interface_id) {
         result = cur_subsys;
         break;
      }
   }
   return result;
}




// sadly, both 0000 and ffff are used as ids, so can't use them as special arguments for "not set"

Pci_Usb_Id_Names pci_id_get_names(
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
   pciusb_id_ensure_initialized();
   Pci_Usb_Id_Names names = {NULL, NULL, NULL};
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
                  names.subsys_or_interface_name = subsys->subsystem_name;
               else {
                  Pci_Id_Vendor * subsys_vendor = pci_id_find_vendor(subvendor_id);
                  if (subsys_vendor)
                     names.subsys_or_interface_name = subsys_vendor->vendor_name;
               }
            }
         }
      }
   }
   return names;
}


Pci_Usb_Id_Names usb_id_get_names(
                ushort vendor_id,
                ushort device_id,
                ushort interface_id,
                int argct)
{
   bool debug = false;
   if (debug) {
      printf("(%s) vendor_id = %02x, device_id=%02x, interface_id=%02x\n",
             __func__,
             vendor_id, device_id, interface_id);
   }
   assert( argct==1 || argct==2 || argct==3);
   pciusb_id_ensure_initialized();
   Pci_Usb_Id_Names names = {NULL, NULL, NULL};
   Pci_Id_Vendor * vendor = usb_id_find_vendor(vendor_id);
   if (vendor) {
      names.vendor_name = vendor->vendor_name;
      if (argct > 1) {
         Pci_Id_Device * device = pci_id_find_device(vendor, device_id);
         if (device) {
            names.device_name = device->device_name;
            if (argct == 3) {
               Pci_Id_Subsys * subsys = usb_id_find_interface(device, interface_id);
               if (subsys)
                  names.subsys_or_interface_name = subsys->subsystem_name;
            }
         }
      }
   }
   if (debug) {
      printf("(%s) Returning: vendor_name=%s, device_name=%s, subsys_or_interface_name=%s\n",
            __func__,

            names.vendor_name, names.device_name, names.subsys_or_interface_name);
   }
   return names;
}


Node_Entry * mlt_find_child(GPtrArray * nodelist, ushort id) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting, id=0x%04x\n", __func__, id);
   Node_Entry * result = NULL;

   for (int ndx = 0; ndx < nodelist->len; ndx++) {
      Node_Entry * cur_entry = g_ptr_array_index(nodelist, ndx);
      if (debug) printf("(%s) Comparing code=0x%04x, name=%s\n",
                 __func__, cur_entry->code, cur_entry->name);
      if (cur_entry->code == id) {
         result = cur_entry;
         break;
      }
   }

   if (debug)
      printf("(%s) Returning %p\n", __func__, result);
   return result;
}

static void report_multi_level_names(Multi_Level_Names * mln, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Multi_Level_Names", mln, depth);
   rpt_int("levels", NULL, mln->levels, d1);
   for (int ndx = 0; ndx < mln->levels; ndx++) {
      rpt_str("names", NULL, mln->names[ndx], d1);
   }
}

// Implement using varargs
// variable arguments are of type ushort
Multi_Level_Names mlt_get_names(Multi_Level_Table * table, int argct, ...) {
  bool debug = false;
  assert(argct >= 1 && argct <= MLT_MAX_LEVELS);
  pciusb_id_ensure_initialized();

  Multi_Level_Names result = {0};

  ushort args[MLT_MAX_LEVELS];

  va_list ap;
  int ndx;
  va_start(ap, argct);
  for (ndx=0; ndx<argct; ndx++) {
     args[ndx] = (ushort) va_arg(ap,int);
     // printf("(%s) args[%d] = 0x%04x\n", __func__, ndx, args[ndx]);
  }
  va_end(ap);

  int argndx = 0;
  GPtrArray * children = table->root;
  while (argndx < argct) {
     assert(children);
     Node_Entry * level_entry = mlt_find_child(children, args[argndx]);
     if (!level_entry) {
        result.levels = 0;   // indicates not found
        break;
     }
     result.levels = argndx+1;
     result.names[argndx] = level_entry->name;
     children = level_entry->children;
     argndx++;
  }

  if (debug) {
     printf("(%s) Returning: \n", __func__);
     report_multi_level_names(&result, 1);
  }
  return result;
}

char * usage_code_page_name(ushort usage_page_code) {
   char * result = NULL;
   // ushort * args = {usage_page_code};
   Multi_Level_Names names_found = mlt_get_names(&hid_usages_table, 1, usage_page_code);
   if (names_found.levels == 1)
      result = names_found.names[0];
   return result;
}

char * usage_code_value_name(ushort usage_page_code, ushort usage_simple_id) {
   // printf("(%s) usage_page_code=0x%04x, usage_simple_id=0x%04x\n", __func__, usage_page_code, usage_simple_id);
   char * result = NULL;
   // ushort * args = {usage_page_code, usage_simple_id};
   Multi_Level_Names names_found = mlt_get_names(&hid_usages_table, 2, usage_page_code, usage_simple_id);
   if (names_found.levels == 2)
      result = names_found.names[1];
   return result;
}





