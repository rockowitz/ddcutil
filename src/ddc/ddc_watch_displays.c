/** \file ddc_watch_displays.c - Watch for monitor addition and removal
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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


gpointer watch_displays(gpointer data) {
   bool debug = false;

   GPtrArray * prev_displays = get_sysfs_drm_displays();

   DBGTRC(debug, TRACE_GROUP,
          "Initial connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", ") );

   while (true) {
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
      }
      g_ptr_array_free(removed, true);

      GPtrArray * added = displays_minus(cur_displays, prev_displays);
      if (added->len > 0) {
         DBGTRC(debug, TRACE_GROUP,
                "Added displays: %s", join_string_g_ptr_array_t(added, ", ") );
      }
      g_ptr_array_free(added, true);

      g_ptr_array_free(prev_displays, true);
      prev_displays = cur_displays;

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
      const char * name = udev_list_entry_get_name(cur);
      const char * value = udev_list_entry_get_value(cur);
      printf("      %s  -> %s\n", name, value);

   }
}



gpointer watch_devices(gpointer data) {
   bool blocking = true;

   struct udev* udev;
   udev = udev_new();
   assert(udev);
   struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // alt "hidraw"
   udev_monitor_enable_receiving(mon);
   /* Get the file descriptor (fd) for the monitor.
         This fd will get passed to select() */
   int fd = 0;

      fd = udev_monitor_get_fd(mon);

   if (blocking) {
      int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
      assert (flags != -1);
      flags &= ~O_NONBLOCK;
      int rc = fcntl(fd, F_SETFL, flags);
      assert(rc != -1);
   }

   /* This section will run continuously, calling usleep() at
        the end of each pass. This is to demonstrate how to use
        a udev_monitor in a non-blocking way. */
     while (1) {
        /* Set up the call to select(). In this case, select() will
           only operate on a single file descriptor, the one
           associated with our udev_monitor. Note that the timeval
           object is set to 0, which will cause select() to not
           block. */

        fd_set fds;
        int ret;

        if (!blocking) {

           struct timeval tv;
           FD_ZERO(&fds);
           FD_SET(fd, &fds);
           tv.tv_sec = 0;
           tv.tv_usec = 0;
           ret = select(fd+1, &fds, NULL, NULL, &tv);
        }

        /* Check if our file descriptor has received data. */
        if (blocking || (ret > 0 && FD_ISSET(fd, &fds))) {
           if (!blocking)
              printf("\nselect() says there should be data\n");
           else
              printf("\nblocking until there is data\n");

           /* Make the call to receive the device.
              select() ensured that this will not block. */
           // by default, is non-blocking as of lubudev 171
           struct udev_device* dev = udev_monitor_receive_device(mon);
           if (dev) {
              printf("Got Device\n");
              printf("   Node: %s\n", udev_device_get_devnode(dev));         // /dev/dri/card0
              printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));  // drm
              printf("   Devtype: %s\n", udev_device_get_devtype(dev));      // drm_minor

              printf("   Action: %s\n",udev_device_get_action(dev));         // change


              printf("   devpath:   %s\n", udev_device_get_devpath(  dev));
              printf("   subsystem: %s\n", udev_device_get_subsystem(dev));
              printf("   devtype:   %s\n", udev_device_get_devtype(  dev));
              printf("   syspath:   %s\n", udev_device_get_syspath(  dev));
              printf("   sysname:   %s\n", udev_device_get_sysname(  dev));
              printf("   sysnum:    %s\n", udev_device_get_sysnum(   dev));
              printf("   devnode:   %s\n", udev_device_get_devnode(  dev));


              struct udev_list_entry * entries = NULL;
              entries = udev_device_get_devlinks_list_entry(dev);
              show_udev_list_entries(entries, "devlinks");

              entries = udev_device_get_properties_list_entry(dev);
              show_udev_list_entries(entries, "properties");

              entries = udev_device_get_tags_list_entry(dev);
              show_udev_list_entries(entries, "tags");

              entries = udev_device_get_sysattr_list_entry(dev);
              show_udev_list_entries(entries, "sysattrs");

              const char * hotplug = udev_device_get_property_value(dev, "HOTPLUG");
              printf("   HOTPLUG:   %s\n", hotplug);

              udev_device_unref(dev);
           }
           else {
              printf("No Device from receive_device(). An error occurred.\n");
           }
        }
        if (!blocking)
           usleep(250*1000);
        printf(".");
        fflush(stdout);
     }


    return NULL;

}
// #endif

#ifdef UNUSED
Error_Info *
start_watch_devices()
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. " );

   Error_Info * ddc_excp = NULL;

   // GThread * th =
   g_thread_new(
         "watch_devices",
         watch_devices,
         NULL);
   return ddc_excp;
}
#endif

Error_Info *
ddc_start_watch_displays()
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. " );

   Error_Info * ddc_excp = NULL;


   // GThread * th =
   g_thread_new(
         "watch_displays",
         // watch_displays,
         watch_devices,
         NULL);
   return ddc_excp;
}



