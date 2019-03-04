/** \file ddc_watch_displays.c - Watch for monitor addition and removal
 */

// Copyright (C) 2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/error_info.h"
#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
/** \endcond */


#include "ddc/ddc_watch_displays.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;


GPtrArray *  get_sysfs_drm_displays() {
   struct dirent *dent;
   // struct dirent *dent2;
   DIR           *dir1;
   char          *dname;
   char          dnbuf[90];
   const int     cardname_sz = 20;
   char          cardname[cardname_sz];

   int depth = 0;
   int d1    = depth+1;
   // int d2    = depth+2;

   bool debug = false;

   GPtrArray * connected_displays = g_ptr_array_new();
   g_ptr_array_set_free_func(connected_displays, free);

   // rpt_vstring(depth,"Examining /sys/class/drm...");
   dname = "/sys/class/drm";
   dir1 = opendir(dname);
   if (!dir1) {
      rpt_vstring(d1, "drm not defined in sysfs. Unable to open directory %s: %s\n",
                     dname, strerror(errno));
   }
   else {
      closedir(dir1);
      int cardno = 0;
      for (;;cardno++) {
         snprintf(cardname, cardname_sz, "card%d", cardno);
         snprintf(dnbuf, 80, "/sys/class/drm/%s", cardname);
         dir1 = opendir(dnbuf);
         if (!dir1) {
            // rpt_vstring(d1, "Unable to open sysfs directory %s: %s\n", dnbuf, strerror(errno));
            break;
         }
         else {
            while ((dent = readdir(dir1)) != NULL) {
               // DBGMSG("%s", dent->d_name);
               // char cur_fn[100];
               if (str_starts_with(dent->d_name, cardname)) {
                  // rpt_vstring(d1, "Found connector: %s", dent->d_name);
                  char cur_dir_name[PATH_MAX];
                  g_snprintf(cur_dir_name, PATH_MAX, "%s/%s", dnbuf, dent->d_name);
                  char * s_status = read_sysfs_attr(cur_dir_name, "status", false);
                  // rpt_vstring(d2, "%s/status: %s", cur_dir_name, s_status);
                  // edid present iff status == "connected"
                  if (streq(s_status, "connected")) {
                     // GByteArray * gba_edid = read_binary_sysfs_attr(
                     //       cur_dir_name, "edid", 128, /*verbose=*/ false);
                     // hex_dump(gba_edid->data, gba_edid->len);

                     g_ptr_array_add(connected_displays, strdup(dent->d_name));

                    // g_byte_array_free(gba_edid, true);

                 }
               }
            }
            closedir(dir1);
         }
      }
      if (cardno==0)
         rpt_vstring(d1, "No drm class cards found in %s", dname);
   }

   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGMSF(debug, "Connected displays: %s", join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
}


// returns first - second
GPtrArray * displays_minus(GPtrArray * first, GPtrArray *second) {
   assert(first);
   assert(second);
   // to consider: only allocate result if there's actually a difference
   GPtrArray * result = g_ptr_array_new_with_free_func(free);
   int ndx1 = 0;
   int ndx2 = 0;
   while(true) {

      if (ndx1 == first->len)
         break;
      if (ndx2 == second->len) {
         char * cur1 = g_ptr_array_index(first, ndx1);
         g_ptr_array_add(result, strdup(cur1));
         ndx1++;
      }
      else {
         char * cur1 = g_ptr_array_index(first, ndx1);
         char * cur2 = g_ptr_array_index(second, ndx2);
         int comp = strcmp(cur1,cur2);
         if (comp < 0) {
            g_ptr_array_add(result, strdup(cur1));
            ndx1++;
         }
         else if (comp == 0) {
            ndx1++;
            ndx2++;
         }
         else {  // comp > 0
            ndx2++;
         }
      }

   }
   return result;
}


bool displays_eq(GPtrArray * first, GPtrArray * second) {
   assert(first);
   assert(second);
   bool result = false;
   if (first->len == second->len) {
      for (int ndx = 0; ndx < first->len; ndx++) {
         if ( !streq(g_ptr_array_index(first, ndx), g_ptr_array_index(second, ndx)) )
               break;
      }
      result = true;
   }
   return result;
}

GPtrArray * check_displays(GPtrArray * prev_displays, gpointer data) {
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting");

   Watch_Displays_Data * wdd = data;
   assert(memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );

   typedef enum _change_type {Changed_None = 0, Changed_Added = 1, Changed_Removed = 2, Changed_Both = 3 } Change_Type;
   Change_Type change_type = Changed_None;

   GPtrArray * cur_displays = get_sysfs_drm_displays();
   if ( !displays_eq(prev_displays, cur_displays) ) {
      if ( debug || IS_TRACING() ) {
         DBGMSG("Displays changed!");
         DBGMSG("Previous connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", "));
         DBGMSG("Current  connected displays: %s", join_string_g_ptr_array_t(cur_displays,  ", "));
      }
   }

   GPtrArray * removed = displays_minus(prev_displays, cur_displays);
   if (removed->len > 0) {
      DBGTRC(debug, TRACE_GROUP,
             "Removed displays: %s", join_string_g_ptr_array_t(removed, ", ") );
      change_type = Changed_Removed;
   }
   g_ptr_array_free(removed, true);

   GPtrArray * added = displays_minus(cur_displays, prev_displays);
   if (added->len > 0) {
      DBGTRC(debug, TRACE_GROUP,
             "Added displays: %s", join_string_g_ptr_array_t(added, ", ") );
      change_type = (change_type == Changed_None) ? Changed_Added : Changed_Both;
   }
   g_ptr_array_free(added, true);

   g_ptr_array_free(prev_displays, true);

   if (change_type != Changed_None) {
      // assert( change_type != Changed_Both);
      if (wdd && wdd->display_change_handler) {
         wdd->display_change_handler( change_type);
      }
   }


   return cur_displays;
}



gpointer watch_displays(gpointer data) {
   bool debug = false;

   GPtrArray * prev_displays = get_sysfs_drm_displays();

   DBGTRC(debug, TRACE_GROUP,
          "Initial connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", ") );

   while (true) {
      prev_displays = check_displays(prev_displays, data);

      usleep(5000*1000);
      // printf(".");
      fflush(stdout);
   }
}




// #ifdef UNUSED

void show_udev_list_entries(
      struct udev_list_entry * entries,
      char * title)
{
   printf( "   %s: \n", title);
   struct udev_list_entry * cur = NULL;
   udev_list_entry_foreach(cur, entries) {
      const char * name  = udev_list_entry_get_name(cur);
      const char * value = udev_list_entry_get_value(cur);
      printf("      %s  -> %s\n", name, value);

   }
}

void show_sysattr_list_entries(
      struct udev_device *       dev,
      struct udev_list_entry * head)
{
   int d1 = 1;
   int d2 = 2;

   rpt_vstring(d1, "Sysattrs:");
   struct udev_list_entry * cur_entry = NULL;
   udev_list_entry_foreach(cur_entry, head) {
      const char * attr_name   = udev_list_entry_get_name(cur_entry);
      const char * attr_value  = udev_list_entry_get_value(cur_entry);
      const char * attr_value2 = udev_device_get_sysattr_value(dev, attr_name);
      assert(attr_value == NULL);
      // hex_dump( (Byte*) attr_value2, strlen(attr_value2)+1);
      if (attr_value2 && strchr(attr_value2, '\n')) {
      // if (streq(attr_name, "uevent")) {
         // output is annoying to visually scan since it contains newlines
         char * av = strdup(attr_value2);
         char * p = av;
         while (*p) {
            if (*p == 0x0a)
               *p = ',';
            p++;
         }
         rpt_vstring(d2, "%s -> %s", attr_name, av);
         free(av);

      }
      // n. attr_name "descriptors" returns a hex value, not a null-terminated string
      //    should display as hex, but how to determine length?
      // for example of reading, see http://fossies.org/linux/systemd/src/udev/udev-builtin-usb_id.c
      // not worth pursuing
      else {
         rpt_vstring(d2, "%s -> %s", attr_name, attr_value2);
      }
   }
}

void set_fd_blocking(int fd) {
   int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
   assert (flags != -1);
   flags &= ~O_NONBLOCK;
   int rc = fcntl(fd, F_SETFL, flags);
   assert(rc != -1);
}


gpointer watch_devices(gpointer data) {
   bool debug = true;

   Watch_Displays_Data * wdd = data;
   assert(memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );

   struct udev* udev;
   udev = udev_new();
   assert(udev);
   struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // alt "hidraw"
   udev_monitor_enable_receiving(mon);

   // make udev_monitor_receive_device() blocking
   int fd = udev_monitor_get_fd(mon);
   set_fd_blocking(fd);


   GPtrArray * prev_displays = get_sysfs_drm_displays();

   while (true) {
      printf("\nblocking until there is data\n");
      // by default, is non-blocking as of lubudev 171, use fcntl() to make file descriptor blocking
      struct udev_device* dev = udev_monitor_receive_device(mon);
      if (dev) {
         if (debug) {
            printf("Got Device\n");
            // printf("   Node: %s\n", udev_device_get_devnode(dev));         // /dev/dri/card0
            // printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));  // drm
            // printf("   Devtype: %s\n", udev_device_get_devtype(dev));      // drm_minor

            printf("   Action:    %s\n", udev_device_get_action(   dev));     // "change"
            printf("   devpath:   %s\n", udev_device_get_devpath(  dev));
            printf("   subsystem: %s\n", udev_device_get_subsystem(dev));     // drm
            printf("   devtype:   %s\n", udev_device_get_devtype(  dev));     // drm_minor
            printf("   syspath:   %s\n", udev_device_get_syspath(  dev));
            printf("   sysname:   %s\n", udev_device_get_sysname(  dev));
            printf("   sysnum:    %s\n", udev_device_get_sysnum(   dev));
            printf("   devnode:   %s\n", udev_device_get_devnode(  dev));     // /dev/dri/card0


            struct udev_list_entry * entries = NULL;
            entries = udev_device_get_devlinks_list_entry(dev);
            show_udev_list_entries(entries, "devlinks");

            entries = udev_device_get_properties_list_entry(dev);
            show_udev_list_entries(entries, "properties");

            entries = udev_device_get_tags_list_entry(dev);
            show_udev_list_entries(entries, "tags");

            entries = udev_device_get_sysattr_list_entry(dev);
            //show_udev_list_entries(entries, "sysattrs");
            show_sysattr_list_entries(dev,entries);
         }

         const char * hotplug = udev_device_get_property_value(dev, "HOTPLUG");
         if (debug)
            printf("   HOTPLUG:   %s\n", hotplug);

         prev_displays = check_displays(prev_displays, data);

         udev_device_unref(dev);


      }
      else {
         printf("No Device from udev_monitor_receive_device(). An error occurred.\n");
      }

      printf(".");
      fflush(stdout);
   }  // while

    return NULL;
}


void dummy_display_change_handler(Displays_Change_Type changes) {
   DBGMSG("changes=%d", changes);
}


Error_Info *
ddc_start_watch_displays()
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. " );

   Error_Info * ddc_excp = NULL;

   Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
   memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
   data->display_change_handler = dummy_display_change_handler;

   // GThread * th =
   g_thread_new(
         "watch_displays",
         watch_displays,    // watch_display or watch_devices
         data);
   return ddc_excp;
}



