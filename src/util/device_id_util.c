/* device_id_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <limits.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "util/file_util.h"
#include "util/multi_level_map.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "util/device_id_util.h"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif


typedef enum {
   ID_TYPE_PCI,
   ID_TYPE_USB
} Device_Id_Type;

// keep in order with enum Device_Id_Type
static char * simple_device_fn[] = {
      "pci.ids",
      "usb.ids"
};


/* Finds the pci.ids or usb.ids file.
 *
 * Arguments:
 *    id_type     ID_TYPE_PCI or ID_TYPE_USB
 *
 * Returns:   fully qualified file name of device id file,
 *            NULL if not found
 *            It is the responsibility of the caller to free
 *            this value
 */
char * devid_find_file(Device_Id_Type id_type) {
   bool debug = false;

   char * known_pci_ids_dirs[] = {
         "/usr/share/libosinfo/db",
         "/usr/share",
         "/usr/share/misc",
         "/usr/share/hwdata",
         NULL
   };

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


//
// Simple_Id_Table
//
// A simple data structure for the simple case where there is only a single
// level of lookup

typedef struct {
   ushort  id;
   char *  name;
} Simple_Id_Table_Entry;


typedef GPtrArray  Simple_Id_Table;    // array of Simple_Id_Table_Entry
/* Creates a new Simple_Id_Table
 *
 * Arguments:
 *    initial_size     if > 0, size for initial allocation
 *
 * Returns:            pointer to newly allocated table
 */
static Simple_Id_Table *
create_simple_id_table(int initial_size) {
   Simple_Id_Table * new_table = NULL;
   if (initial_size > 0)
      new_table = g_ptr_array_sized_new(initial_size);
   else
      new_table = g_ptr_array_new();
   return new_table;
}


/* Adds an entry to a Simple_Id_Table
 *
 * Arguments:
 *   simple_table  pointer to Simple_Id_Table
 *   id            key of entry
 *   name          value of entry
 */
static Simple_Id_Table_Entry *
sit_add(Simple_Id_Table * simple_table, ushort id, char * name) {
   Simple_Id_Table_Entry * new_entry = calloc(1, sizeof(Simple_Id_Table_Entry));
   new_entry->id = id;
   new_entry->name = strdup(name);
   g_ptr_array_add(simple_table, new_entry);
   return new_entry;
}


void
report_simple_id_table(Simple_Id_Table * simple_table, int depth) {
   rpt_structure_loc("Simple ids table", simple_table, depth);
   for (int ndx = 0; ndx < simple_table->len; ndx++) {
      Simple_Id_Table_Entry * cur_entry = g_ptr_array_index(simple_table, ndx);
      rpt_vstring(depth+1, "0x%04x -> |%s|", cur_entry->id, cur_entry->name);
   }
}

char * get_simple_id_name(Simple_Id_Table * simple_table, ushort id) {
   char * result = NULL;
   for (int ndx = 0; ndx < simple_table->len; ndx++) {
      Simple_Id_Table_Entry * cur_entry = g_ptr_array_index(simple_table, ndx);
      if (cur_entry->id == id) {
         result = cur_entry->name;
         break;
      }
   }
   return result;
}



//
// *** Global Variables ***
//

// stats 12/2015:
//   lines in pci.ids:  25,339
//   vendors:            2,066
//   total devices:     11,745
//   subsystem:         10,974

static Multi_Level_Map * pci_vendors_mlm = {0};
static Multi_Level_Map * usb_vendors_mlm = {0};

static Simple_Id_Table * hid_descriptor_types;       // tag HID
static Simple_Id_Table * hid_descriptor_item_types;  // tag R
static Simple_Id_Table * hid_country_codes;          // tag HCC - for keyboards
static Multi_Level_Map * hid_usages_table;           // tag HUT


//
// *** Input File Parsing ***
//

/* Parses a subrange of an array of text lines into an empty Simple_Id_Table
 *
 * Arguments:
 *   simple_table
 *   all_lines       array of pointer to text lines to parse
 *   segment_tag     first token in lines, when this changes it indicates segment exhausted
 *   cur_pos         first line of all_lines to parse
 *   end_pos         updated with line number after last line of segment
 */
static void
load_simple_id_segment(
      Simple_Id_Table * simple_table,      // empty GPtrArray, will be filled in w Simple_Id_Table_Entry *
      GPtrArray *       all_lines,         // array of pointers to lines
      char *            segment_tag,
      int               cur_pos,
      int *             end_pos)
{
   bool debug = false;
   assert(simple_table);
   if (debug) {
      printf("(%s) Starting. curpos=%d, -> |%s|\n",
             __func__, cur_pos, (char *) g_ptr_array_index(all_lines,cur_pos));
   }
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

      sit_add(simple_table, acode, aname);
   }

   if (cur_pos <= linect)
      cur_pos--;
   *end_pos = cur_pos;
   if (debug)
      printf("(%s) Set end_pos = %d\n", __func__, cur_pos);
}



static  Multi_Level_Map * load_multi_level_segment(
      Multi_Level_Map * header,
      char *            segment_tag,
      GPtrArray *       all_lines,
      int*              curpos)
{
   bool debug = false;
   int linendx = *curpos;
   if (debug)
      printf("(%s) Starting. linendx=%d, -> |%s|\n",
             __func__, linendx, (char *) g_ptr_array_index(all_lines,linendx));

   MLM_Node * cur_nodes[8] = {NULL};     // todo: deal properly w max levels

   for (int ndx = 0; ndx < header->levels; ndx++) {
      header->level_detail[ndx].total_entries = 0;
      header->level_detail[ndx].cur_entry = NULL;
   }

   ushort cur_code;
   char * cur_name;

   bool more = true;
   while (more && linendx < all_lines->len) {
      char * a_line = g_ptr_array_index(all_lines, linendx++);
      int tabct = 0;
      while (a_line[tabct] == '\t')
         tabct++;
      if (strlen(rtrim_in_place(a_line+tabct)) == 0 || a_line[tabct] == '#')
         continue;

      if (tabct == 0) {
         char cur_tag[40];
         int ct = sscanf(a_line+tabct, "%s %4hx %m[^\n]",
                              cur_tag,
                              &cur_code,    // &header->level_detail[tabct].cur_entry->code,
                              &cur_name);   // &header->level_detail[tabct].cur_entry->name );
         if (!streq(cur_tag, segment_tag)) {
            // more = false;   // needed for continue
            break;     // or continue?
         }
         if (ct != 3) {
            printf("(%s) Error processing line %d: \"%s\"\n", __func__, linendx, a_line);
            printf("(%s) Lines has %d fields, not 3.  Ignoring\n", __func__, ct);
            // hex_dump(a_line+tabct, strlen(a_line+tabct));
         }
         else {
            header->level_detail[tabct].total_entries++;
            cur_nodes[tabct] = mlm_add_node(header, NULL, cur_code, cur_name);
            for (int lvl = tabct+1; lvl < header->levels; lvl++) {
                header->level_detail[lvl].cur_entry = NULL;
                cur_nodes[lvl] = NULL;
            }
            // printf("Successful level 0 node added. set header->level_detail[%d].cur_entry (addr %p) to %p\n",
            //        tabct, &(header->level_detail[tabct].cur_entry), header->level_detail[tabct].cur_entry   );
            // mlt_cur_entries(header);
        }
     }

     else  {    // intermediate or leaf node
        if (!cur_nodes[tabct-1]) {
           printf("Error processing line %d: \"%s\"\n", linendx-1, a_line);
           printf("No enclosing level %d node\n", tabct-1);
           // printf("(A) tabct=%d\n", tabct);
           // mlt_cur_entries(header);
           // A free(header->level_detail[tabct].cur_entry);
           header->level_detail[tabct].cur_entry = NULL;

        }
        else {
           int ct = sscanf(a_line+tabct, "%4hx  %m[^\n]",
                            &cur_code,   // &header->level_detail[tabct].cur_entry->code,
                            &cur_name);  // &header->level_detail[tabct].cur_entry->name);
            if (ct != 2) {
               printf("(%s) Error reading line %d: %s\n",
                      __func__, linendx-1, a_line);
            }
            else {
               header->level_detail[tabct].total_entries++;
               // A if (tabct < header->levels-1)  // if not a leaf
               // A    header->level_detail[tabct].cur_entry->children = g_ptr_array_sized_new(20);
               // A g_ptr_array_add(header->level_detail[tabct-1].cur_entry->children, header->level_detail[tabct].cur_entry);
               cur_nodes[tabct] = mlm_add_node(header, cur_nodes[tabct-1],  cur_code, cur_name);
               for (int lvl = tabct+1; lvl < header->levels; lvl++) {
                   header->level_detail[lvl].cur_entry = NULL;
                   cur_nodes[lvl] = NULL;
               }
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


/* Find the start of the next segment in a line array,
 * i.e. a non-comment line that begins with a different tag
 *
 * Arguments:
 *    lines         array of pointers to lines
 *    cur_ndx       current position in line array
 *    segment_tag   first token of lines in current segment
 *
 * Returns:         line number of start of next segment
 */
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


int load_device_ids(Device_Id_Type id_type, GPtrArray * all_lines) {
   bool debug = false;
    int total_vendors = 0;
    int total_devices = 0;
    int total_subsys  = 0;

    MLM_Level usb_id_levels[] = {
          {"vendor", 5000, 0},
          {"product",  20, 0},
          {"interface", 10, 0}
    };

    MLM_Level pci_id_levels[] = {
          {"vendor",   10000, 0},
          {"device",      20, 0},
          {"subsystem",    5, 0}
    };

#define MAX_LEVELS 5
     Multi_Level_Map * mlm = NULL;
     int levelct = 3;

     if (id_type == ID_TYPE_PCI) {
        mlm = mlm_create("PCI Devices", 3, pci_id_levels);
     }
     else
        mlm = mlm_create("USB Devices", 3, usb_id_levels);

    MLM_Node * cur_node[MAX_LEVELS] = {NULL};

    int linect = all_lines->len;
    int linendx;
    char * a_line;
    bool device_ids_done = false;    // end of PCI id section seen?
    for (linendx=0; linendx<linect && !device_ids_done; linendx++) {
       a_line = g_ptr_array_index(all_lines, linendx);
       int tabct = 0;
       while (a_line[tabct] == '\t')
          tabct++;
       if (strlen(rtrim_in_place(a_line+tabct)) == 0 || a_line[tabct] == '#')
          continue;
       if (id_type == ID_TYPE_USB) {
          // hacky test for end of id section
          if (memcmp(a_line+tabct, "C", 1) == 0) {
             device_ids_done = true;
             continue;
          }
       }

       switch(tabct) {

       case (0):
          {
             ushort cur_id = 0;
             char * cur_name = NULL;
             int ct = sscanf(a_line+tabct, "%4hx %m[^\n]",
                             &cur_id,
                             &cur_name);

             if (ct != 2) {
                printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                // hex_dump(a_line+tabct, strlen(a_line+tabct));
                for (int ndx = tabct; ndx < levelct; ndx++) {
                   cur_node[ndx] = NULL;
                }
             }
             else {
                total_vendors++;
                // usb.ids has no final ffff field, test works only for pci.ids
                if (cur_id == 0xffff)
                   device_ids_done = true;
                cur_node[tabct] = mlm_add_node(mlm, NULL, cur_id, cur_name);
                for (int ndx = tabct+1; ndx < levelct; ndx++) {
                   cur_node[ndx] = NULL;
                }
             }
             break;
          }

       case (1):
          {
             if (cur_node[tabct-1])  {
                ushort cur_id;
                char * cur_name;
                int ct = sscanf(a_line+tabct, "%4hx %m[^\n]", &cur_id, &cur_name);

                if (ct != 2) {
                   printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                }
                else {
                   total_devices++;
                   cur_node[tabct] = mlm_add_node(mlm, cur_node[tabct-1], cur_id, cur_name);
                   for (int ndx = tabct+1; ndx < levelct; ndx++) {
                      cur_node[ndx] = NULL;
                   }
                }

             }
             else {
                // TODO: handle bad data case
             }
             break;
          }

       case (2):
          {
             if (cur_node[tabct-1]) {
                if (id_type == ID_TYPE_PCI) {
                   ushort this_subvendor_id = 0;
                   ushort this_subdevice_id = 0;
                   char * this_name = NULL;
                   int ct = sscanf(a_line+tabct, "%4hx %4hx %m[^\n]",
                                   &this_subvendor_id,
                                   &this_subdevice_id,
                                   &this_name);
                   // transitional:

                   uint   this_id = this_subvendor_id << 16 | this_subdevice_id;

                   if (ct != 3) {
                      printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                   }
                   else {
                      total_subsys++;
                      cur_node[tabct] = mlm_add_node(mlm, cur_node[tabct-1], this_id, this_name);
                   }
                }  // ID_TYPE_PCI
                else {     // ID_TYPE_USB
                   ushort this_id = 0;
                   char * this_name = NULL;
                   int ct = sscanf(a_line+tabct, "%4hx  %m[^\n]",
                                   &this_id,
                                   &this_name);

                   if (ct != 2) {
                      printf("(%s) Error reading line: %s\n", __func__, a_line+tabct);
                   }
                   else {
                      total_subsys++;
                      // new
                      cur_node[tabct] = mlm_add_node(mlm, cur_node[tabct-1], this_id, this_name);
                   }
                }  //ID_TYPE_USB
             }  // if (cur_device)
             break;
          }

       default:
          printf("Unexpected number of leading tabs in line: %s\n", a_line);
       } // switch
    }    // line loop

    if (id_type == ID_TYPE_PCI) {
       pci_vendors_mlm = mlm;
    }
    else {
       usb_vendors_mlm = mlm;
    }

    if (debug) {
       char * level3_name = (id_type == ID_TYPE_PCI) ? "subsystems" : "interfaces";
       printf("(%s) Total vendors: %d, total devices: %d, total %s: %d\n",
              __func__, total_vendors, total_devices, level3_name, total_subsys);
    }

   return linendx;
}


static void load_file_lines(Device_Id_Type id_type, GPtrArray * all_lines) {
   // bool debug = false;
   int linendx;
   // char * a_line;

   linendx = load_device_ids(id_type, all_lines);

   // if usb.ids, look for additional segments
   if (id_type == ID_TYPE_USB) {
         // a_line = g_ptr_array_index(all_lines, linendx);
         // printf("(%s) First line of next segment: |%s|, linendx=%d\n", __func__, a_line, linendx);
      linendx--; //  start looking on comment line before segment
         // a_line = g_ptr_array_index(all_lines, linendx);
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
            hid_descriptor_types = create_simple_id_table(0);
            load_simple_id_segment(hid_descriptor_types, all_lines, tagbuf, linendx, &linendx);
               // printf("(%s) After HID, linendx=%d\n", __func__, linendx);
               // rpt_title("hid_descriptor_types: ", 0);
               // report_simple_ids(hid_descriptor_types, 1);
         }
         else if ( streq(tagbuf,"R") ) {
             hid_descriptor_item_types = create_simple_id_table(0);
             load_simple_id_segment(hid_descriptor_item_types, all_lines, tagbuf, linendx, &linendx);
                // printf("(%s) After R, linendx=%d\n", __func__, linendx);
                // rpt_title("hid_descriptor_item_types: ", 0);
                // report_simple_ids(hid_descriptor_item_types, 1);
          }
         else if ( streq(tagbuf,"HCC") ) {
             hid_country_codes = create_simple_id_table(0);
             load_simple_id_segment(hid_country_codes, all_lines, tagbuf, linendx, &linendx);
                // printf("(%s) After HCC, linendx=%d\n", __func__, linendx);
                // rpt_title("hid_country_codes: ", 0);
                // report_simple_ids(hid_country_codes, 1);
          }

          else if ( streq(tagbuf,"HUT") ) {
#ifdef OLD
             hid_usages_table = mlm_create(
                                                    hid_usages_table0.table_name,
                                                    hid_usages_table0.levels,
                                                    hid_usages_table0.level_detail);
#endif
             MLM_Level hut_level_desc[] = {
                   {"usage page", 20, 0},
                   {"usage_id",   20, 0}
             };

             hid_usages_table = mlm_create(
                                  "HUT",
                                  2,
                                  hut_level_desc);


             load_multi_level_segment(hid_usages_table, tagbuf, all_lines, &linendx);
                // printf("(%s) After HUT, linendx=%d\n", __func__, linendx);
                // rpt_title("usages table: ", 0);
                // report_multi_level_table(hid_usages_table, 1);
          }
      }
   }
}


/* Locates a pci.ids or usb.ids file and loads its contents into internal tables.
 *
 * Arguments:
 *    id_type
 *
 * Returns:    nothing
 */
static void load_id_file(Device_Id_Type id_type){
   bool debug = false;

   if (debug)
      printf("(%s) id_type=%d\n", __func__, id_type);

   // char * id_fn = simple_device_fn[id_type];
   char * device_id_fqfn = devid_find_file(id_type);
   if (device_id_fqfn) {
      // char device_id_fqfn[MAX_PATH];
      // snprintf(device_id_fqfn, MAX_PATH, id_fqfn, id_fn);  // ???
      if (debug)
         printf("(%s) device_id_fqfn = %s\n", __func__, device_id_fqfn);

      GPtrArray * all_lines = g_ptr_array_sized_new(30000);
      int linect = file_getlines(device_id_fqfn, all_lines, true);
      if (linect > 0) {
         load_file_lines(id_type, all_lines);
      }       // if (all_lines)
      // to do: call

      g_ptr_array_set_free_func(all_lines, free);
      g_ptr_array_free(all_lines, true);
   }          // if pci.ids or usb.ids was found

   return;
}


//
// Internal Report Functions
//

/* Reports a device id table.
 *
 * Arguments:
 *    id_type
 *
 * Returns:    nothing
 */
void report_device_ids_mlm(Device_Id_Type id_type) {
   // bool debug = true;

   Multi_Level_Map * all_devices = (id_type == ID_TYPE_PCI) ? pci_vendors_mlm : usb_vendors_mlm;
   GPtrArray * top_level_nodes = all_devices->root;
   int total_vendors = 0;
   int total_devices = 0;
   int total_subsys  = 0;
   int vctr, dctr, sctr;
   MLM_Node * cur_vendor;
   MLM_Node * cur_device;
   MLM_Node * cur_subsys;
   for (vctr=0; vctr < top_level_nodes->len; vctr++) {
      total_vendors++;
      cur_vendor = g_ptr_array_index(top_level_nodes, vctr);
      printf("%04x %s\n", cur_vendor->code, cur_vendor->name);
      if (cur_vendor->children) {
         for (dctr=0; dctr<cur_vendor->children->len; dctr++) {
            total_devices++;
            cur_device = g_ptr_array_index(cur_vendor->children, dctr);
            printf("\t%04x %s\n", cur_device->code, cur_device->name);
            if (cur_device->children) {
               for (sctr=0; sctr<cur_device->children->len; sctr++) {
                  total_subsys++;
                  cur_subsys = g_ptr_array_index(cur_device->children, sctr);
                  if (id_type == ID_TYPE_PCI)
                     printf("\t\t%04x %04x %s\n",
                            cur_subsys->code>>16, cur_subsys->code&0xffff, cur_subsys->name);
                  else
                     printf("\t\t%04x %s\n",
                            cur_subsys->code, cur_subsys->name);
               }
            }
         }
      }
   }
   char * level3_name = (id_type == ID_TYPE_PCI) ? "subsystems" : "interfaces";
   printf("(%s) Total vendors: %d, total devices: %d, total %s: %d\n",
          __func__, total_vendors, total_devices, level3_name, total_subsys);
}


//
// *** Name Lookup ***
//

// sadly, both 0000 and ffff are used as ids, so can't use them as special arguments for "not set"

Pci_Usb_Id_Names devid_get_pci_names(
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
   devid_ensure_initialized();
   uint ids[3] = {vendor_id, device_id, subvendor_id << 16 | subdevice_id};   // only diff from usb_id_get_names
   int levelct = (argct == 4) ? 3 : argct;              // also this
   Multi_Level_Names mlm_names =  mlm_get_names2(pci_vendors_mlm, levelct, ids);  // and this
   Pci_Usb_Id_Names names2;
   names2.vendor_name = mlm_names.names[0];
   names2.device_name = mlm_names.names[1];
   names2.subsys_or_interface_name = mlm_names.names[2];
   if (levelct == 3 && mlm_names.levels == 2) {
      // couldn't find the subsystem, see if at least we can look up the subsystem vendor
      uint ids[1] = {subvendor_id};
      Multi_Level_Names mlm_names3 = mlm_get_names2(pci_vendors_mlm, 1, ids);
      if (mlm_names3.levels == 1) {
         names2.subsys_or_interface_name = mlm_names3.names[0];
      }
   }

   if (debug) {
      printf("(%s) names2: vendor_name=%s, device_name=%s, subsys_or_interface_name=%s\n",
            __func__,
            names2.vendor_name, names2.device_name, names2.subsys_or_interface_name);
   }

   return names2;
}


Pci_Usb_Id_Names devid_get_usb_names(
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
   devid_ensure_initialized();
   uint ids[3] = {vendor_id, device_id, interface_id};
   Multi_Level_Names mlm_names =  mlm_get_names2(usb_vendors_mlm, argct, ids);
   Pci_Usb_Id_Names names2;
   names2.vendor_name = mlm_names.names[0];
   names2.device_name = mlm_names.names[1];
   names2.subsys_or_interface_name = mlm_names.names[2];

   if (debug) {
      printf("(%s) names2: vendor_name=%s, device_name=%s, subsys_or_interface_name=%s\n",
            __func__,
            names2.vendor_name, names2.device_name, names2.subsys_or_interface_name);
   }

   return names2;
}


/* Gets the page name for a usage page code
 *
 * Corresponds to names_huts() in names.c
 * Is top level field in HUT entry of usb.ids
*/
char * devid_usage_code_page_name(ushort usage_page_code) {
   devid_ensure_initialized();
   // Per USB HID Usage Tables spec v1.12, section 3.0,
   // Usage page ID xff00..xffff are vendor defined
   //               x0092..xfeff are reserved
   // We regard any value < xff00 for which lookup fails as reserved.
   // This allows for additional usage pages beyond x0092 to be specified
   // in the usb.ids file.   However, usb.ids includes the line:
   //     HUT  ff  Vendor specific
   // This is incorrect.  It is treating use page code as 1 byte instead of 2.
   // xff is in the reserved range.  It is not a vendor defined page.
   char * result = "Reserved";
   if (usage_page_code > 0xff00)
      result = "Vendor-defined";
   else {
      // ushort * args = {usage_page_code};
      Multi_Level_Names names_found = mlm_get_names(hid_usages_table, /*argct=*/ 1, usage_page_code);
      if (names_found.levels == 1)
         result = names_found.names[0];
   }
   return result;
}


// corresponds to names_hutus() in names
// first and second fields of HUT entry in usb.ids
char * devid_usage_code_id_name(ushort usage_page_code, ushort usage_simple_id) {
   static char resultbuf[12] = {0};
   bool debug = false;
   if (debug) {
      printf("(%s) usage_page_code=0x%04x, usage_simple_id=0x%04x\n",
             __func__, usage_page_code, usage_simple_id);
   }
   devid_ensure_initialized();
   char * result = NULL;
   if (usage_page_code == 0x81) {
      snprintf(resultbuf, 10, "ENUM_%d", usage_simple_id);
      result = resultbuf;
   }
   else {
      // ushort * args = {usage_page_code, usage_simple_id};
      Multi_Level_Names names_found = mlm_get_names(hid_usages_table, 2, usage_page_code, usage_simple_id);
      if (names_found.levels == 2)
         result = names_found.names[1];
   }
   return result;
}


char * devid_usage_code_name_by_extended_id(uint32_t extended_usage) {
   return devid_usage_code_id_name( extended_usage >> 16, extended_usage & 0xffff );
}


// HID documentation refers to this as item tag
// usb.ids file refers to this as item type
// is actually 1 byte
// corresponds to names.c function names_reporttag()
char * devid_hid_descriptor_item_type(ushort id) {
   devid_ensure_initialized();
   char * result = NULL;
   result = get_simple_id_name(hid_descriptor_item_types, id);
   return result;
}


//
// *** Initialization ***
//

bool devid_ensure_initialized() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__);
   bool ok = (pci_vendors_mlm && usb_vendors_mlm);

   if (!ok) {
      load_id_file(ID_TYPE_PCI);
      load_id_file(ID_TYPE_USB);
      ok = true;
      if (ok && debug) {
         // report_device_ids(ID_TYPE_PCI);
         // report_device_ids_mlm(ID_TYPE_PCI);
         // report_device_ids(ID_TYPE_USB);
         // report_device_ids_mlm(ID_TYPE_USB);
      }
   }
   if (debug)
      printf("(%s) Returning: %s\n", __func__, bool_repr(ok));
   return ok;
}


