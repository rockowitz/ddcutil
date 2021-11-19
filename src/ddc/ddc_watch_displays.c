/** \file ddc_watch_displays.c - Watch for monitor addition and removal
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#ifdef ENABLE_UDEV
#include <libudev.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
// #include <unistd.h>

#include "util/data_structures.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "ddc/ddc_watch_displays.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;

static bool terminate_watch_thread = false;
static GThread * watch_thread = NULL;
static GMutex    watch_thread_mutex;

#define WATCH_DISPLAYS_DATA_MARKER "WDDM"
typedef struct {
   char                   marker[4];
   Display_Change_Handler display_change_handler;
   pid_t                  main_process_id;
   pid_t                  main_thread_id;
   Byte_Bit_Flags         drm_card_numbers;
} Watch_Displays_Data;


static
void free_watch_displays_data(Watch_Displays_Data * wdd) {
   if (wdd) {
      assert( memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );
      wdd->marker[3] = 'x';
      free(wdd);
   }
}


const char * displays_change_type_name(Displays_Change_Type change_type) {
   char * result = NULL;
   switch(change_type)
   {
   case Changed_None:    result = "Changed_None";    break;
   case Changed_Added:   result = "Changed_Added";   break;
   case Changed_Removed: result = "Changed_Removed"; break;
   case Changed_Both:    result = "Changed_Both";    break;
   }
   return result;
}


bool check_thread_or_process(pid_t id) {
   struct stat buf;
   char procfn[20];
   snprintf(procfn, 20, "/proc/%d", id);
   int rc = stat(procfn, &buf);
   bool result = (rc == 0);
   DBGMSG("File: %s, returning %s", procfn, sbool(result));
   if (!result)
      DBGMSG("!!! Returning: %s", sbool(result));
   return result;
}





/** Gets a list of all displays known to DRM.
 *
 *  \param sysfs_drm_cards
 *  \bool  verbose
 *  \return GPtrArray of connector names for DRM displays
 *
 *  \remark
 *  The caller is responsible for freeing the returned #GPtrArray.
 */
#ifdef OLD
GPtrArray * get_sysfs_drm_displays_old(Byte_Bit_Flags sysfs_drm_cards, bool verbose)
{
   bool debug = false;
   int  depth = 0;
   int  d1    = depth+1;
   int  d2    = depth+2;

   struct dirent *dent;
   DIR           *dir1;
   char          *dname;
   char          dnbuf[90];
   const int     cardname_sz = 20;
   char          cardname[cardname_sz];

   GPtrArray * connected_displays = g_ptr_array_new();
   g_ptr_array_set_free_func(connected_displays, g_free);

#ifdef TARGET_BSD
   dname = "/compat/linux/sys/class/drm";
#else
   dname = "/sys/class/drm";
#endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s...", dname);
   Byte_Bit_Flags iter = bbf_iter_new(sysfs_drm_cards);
   int cardno = -1;
   while ( (cardno = bbf_iter_next(iter)) >= 0) {
      snprintf(cardname, cardname_sz, "card%d", cardno);
      snprintf(dnbuf, 80, "%s/%s", dname, cardname);
      dir1 = opendir(dnbuf);
      DBGMSF(debug, "dnbuf=%s", dnbuf);
      if (!dir1) {
         // rpt_vstring(d1, "Unable to open sysfs directory %s: %s\n", dnbuf, strerror(errno));
         break;
      }
      else {
         while ((dent = readdir(dir1)) != NULL) {
            // DBGMSG("%s", dent->d_name);
            // char cur_fn[100];
            if (str_starts_with(dent->d_name, cardname)) {
               if (verbose)
                  rpt_vstring(d1, "Found connector: %s", dent->d_name);
               char cur_dir_name[PATH_MAX];
               g_snprintf(cur_dir_name, PATH_MAX, "%s/%s", dnbuf, dent->d_name);
               char * s_status = read_sysfs_attr(cur_dir_name, "status", false);
               // rpt_vstring(d2, "%s/status: %s", cur_dir_name, s_status);
               if (verbose)
                  rpt_vstring(d2, "Display: %s, status=%s", dent->d_name, s_status);
               // edid present iff status == "connected"
               if (streq(s_status, "connected")) {
                  if (verbose) {
                     GByteArray * gba_edid = read_binary_sysfs_attr(
                           cur_dir_name, "edid", 128, /*verbose=*/ true);
                     if (gba_edid) {
                        rpt_vstring(d2, "%s/edid:", cur_dir_name);
                        rpt_hex_dump(gba_edid->data, gba_edid->len, d2);
                        g_byte_array_free(gba_edid, true);
                     }
                     else {
                        rpt_vstring(d2, "Reading %s/edid failed.", cur_dir_name);
                     }
                  }

                  g_ptr_array_add(connected_displays, strdup(dent->d_name));
               }
               free(s_status);
               if (verbose)
                  rpt_nl();
            }
         }
         closedir(dir1);
      }
   }
   bbf_iter_free(iter);
   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGTRC_DONE(debug, TRACE_GROUP, "Connected displays: %s",
                              join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
}
#endif


// Move get_sysfs_drm_examine_one_connector(), get_sysfs_drm_displays()
// to sysfs_i2c_util.c?

static
void get_sysfs_drm_examine_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   GPtrArray * connected_displays = (GPtrArray *) data;

   char * status = NULL;
   bool found_status = RPT_ATTR_TEXT(-1, &status, dirname, simple_fn, "status");
   if (found_status && streq(status,"connected")) {
         g_ptr_array_add(connected_displays, strdup(simple_fn));
      }
   g_free(status);

   DBGMSF(debug, "Added connector %s", simple_fn);
}


static
GPtrArray * get_sysfs_drm_displays() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   DBGTRC_STARTING(debug, TRACE_GROUP, "Examining %s", dname);
   GPtrArray * connected_displays = g_ptr_array_new_with_free_func(g_free);
   dir_filtered_ordered_foreach(
                 dname,
                 is_sysfs_drm_connector_dir_name,      // filter function
                 NULL,                    // ordering function
                 get_sysfs_drm_examine_one_connector,
                 connected_displays,      // accumulator
                 0);
   g_ptr_array_sort(connected_displays, gaux_ptr_scomp);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning Connected displays: %s",
                              join_string_g_ptr_array_t(connected_displays, ", "));
   return connected_displays;
 }


static GPtrArray * check_displays(GPtrArray * prev_displays, gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "prev_displays=%s",
                 join_string_g_ptr_array_t(prev_displays, ", "));

   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );

   // typedef enum _change_type {Changed_None = 0, Changed_Added = 1, Changed_Removed = 2, Changed_Both = 3 } Change_Type;
   Displays_Change_Type change_type = Changed_None;

   GPtrArray * cur_displays = get_sysfs_drm_displays(wdd->drm_card_numbers, false);
   if ( !gaux_string_ptr_arrays_equal(prev_displays, cur_displays) ) {
      if ( debug || IS_TRACING() ) {
         DBGMSG("Displays changed!");
         DBGMSG("Previous connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", "));
         DBGMSG("Current  connected displays: %s", join_string_g_ptr_array_t(cur_displays,  ", "));
      }

      GPtrArray * removed = gaux_string_ptr_arrays_minus(prev_displays, cur_displays);
      if (removed->len > 0) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                "Removed displays: %s", join_string_g_ptr_array_t(removed, ", ") );
         change_type = Changed_Removed;
      }

      GPtrArray * added = gaux_string_ptr_arrays_minus(cur_displays, prev_displays);
      if (added->len > 0) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                "Added displays: %s", join_string_g_ptr_array_t(added, ", ") );
         change_type = (change_type == Changed_None) ? Changed_Added : Changed_Both;
      }

   //    if (change_type != Changed_None) {
      // assert( change_type != Changed_Both);
      // DBGMSG("wdd->display_change_handler = %p (%s)",
      //         wdd->display_change_handler,
      //         rtti_get_func_name_by_addr(wdd->display_change_handler) );
      if (wdd->display_change_handler) {
         wdd->display_change_handler( change_type, removed, added);
      }
      // }
      g_ptr_array_free(removed,       true);
      g_ptr_array_free(added,         true);
   }

   g_ptr_array_free(prev_displays, true);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s",
                              join_string_g_ptr_array_t(cur_displays, ", "));
   return cur_displays;
}


// How to detect main thread crash?

gpointer watch_displays_using_poll(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0);

   GPtrArray * prev_displays = get_sysfs_drm_displays(wdd->drm_card_numbers, false);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
          "Initial connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", ") );

   while (!terminate_watch_thread) {
     // else    // logically meaningless, since if() case exits, but avoids clang use after free warning
     prev_displays = check_displays(prev_displays, data);

      usleep(3000*1000);
      // printf(".");
      // fflush(stdout);
   }
   DBGTRC_DONE(true, TRACE_GROUP, "Terminating");
   free_watch_displays_data(wdd);
   g_thread_exit(0);
   return NULL;    // satisfy compiler check that value returned
}


#ifdef ENABLE_UDEV
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
#endif


void set_fd_blocking(int fd) {
   int flags = fcntl(fd, F_GETFL, /* ignored for F_GETFL */ 0);
   assert (flags != -1);
   flags &= ~O_NONBLOCK;
   int rc = fcntl(fd, F_SETFL, flags);
   assert(rc != -1);
}


#ifdef ENABLE_UDEV
gpointer watch_displays_using_udev(gpointer data) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   Watch_Displays_Data * wdd = data;
   assert(wdd && memcmp(wdd->marker, WATCH_DISPLAYS_DATA_MARKER, 4) == 0 );

   // DBGMSG("Caller process id: %d, caller thread id: %d", wdd->main_process_id, wdd->main_thread_id);
   // pid_t cur_pid = getpid();
   // pid_t cur_tid = get_thread_id();
   // DBGMSG("Our process id: %d, our thread id: %d", cur_pid, cur_tid);

   struct udev* udev;
   udev = udev_new();
   assert(udev);
   struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
   udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);   // alt "hidraw"
   udev_monitor_enable_receiving(mon);

   // make udev_monitor_receive_device() blocking
   int fd = udev_monitor_get_fd(mon);
   set_fd_blocking(fd);

   GPtrArray * prev_displays = get_sysfs_drm_displays(wdd->drm_card_numbers, false);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
          "Initial connected displays: %s", join_string_g_ptr_array_t(prev_displays, ", ") );

   while (true) {

      // Doesn't work to kill thread, udev_monitor_receive_device() is blocking
      // leave in so that code checkers are fooled into thinking that
      // free_watch_displays_data() is called at program termination
      if (terminate_watch_thread) {
         DBGTRC_DONE(true, TRACE_GROUP, "Terminating thread");
         free_watch_displays_data(wdd);
         g_thread_exit(0);
         assert(false);    // avoid clang warning re wdd use after free
      }

      // Doesn't work to detect client crash, main thread and process remains for some time.
      // 11/2020: is this even needed since terminate_watch_thread check added?
#ifdef DOESNT_WORK
      bool pid_found = check_thread_or_process(cur_pid);
      if (!pid_found) {
         DBGMSG("Process %d not found", cur_pid);
      }
      bool tid_found = check_thread_or_process(cur_tid);
      if (!pid_found || !tid_found) {
         DBGMSG("Thread %d not found", cur_tid);
         g_thread_exit(GINT_TO_POINTER(-1));
         break;
      }
#endif

      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Blocking until there is data");
      // by default, is non-blocking as of libudev 171, use fcntl() to make file descriptor blocking
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
            printf("   initialized: %d\n", udev_device_get_is_initialized(  dev));
            printf("   driver:    %s\n", udev_device_get_driver(  dev));

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
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,"HOTPLUG: %s", hotplug);     // "1"

         prev_displays = check_displays(prev_displays, data);

         udev_device_unref(dev);
      }
      else {
         // Failure indicates main thread has died.  Kill this one too.
         int errsv=errno;
         DBGTRC_DONE(debug, TRACE_GROUP,
                            "No Device from udev_monitor_receive_device()."
                            " An error occurred. errno=%d=%s. Terminating thread.",
                            errsv, linux_errno_name(errsv));
         g_thread_exit(GINT_TO_POINTER(-1));
         // break;
      }

      // printf(".");
      // fflush(stdout);
   }  // while

    return NULL;
}
#endif


void dummy_display_change_handler(
        Displays_Change_Type changes,
        GPtrArray *          removed,
        GPtrArray *          added)
{
   bool debug = false;
   // DBGTRC_STARTING(debug, TRACE_GROUP, "changes = %s", displays_change_type_name(changes));
   if (removed && removed->len > 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Removed displays: %s", join_string_g_ptr_array_t(removed, ", ") );
   }
   if (added && added->len > 0) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Added   displays: %s", join_string_g_ptr_array_t(added, ", ") );
   }
   // DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Starts thread that watches for addition or removal of displays
 *
 *  \retval  DDCRC_OK
 *  \retval  DDCRC_INVALID_OPERATION  thread already running
 */
DDCA_Status
ddc_start_watch_displays()
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "" );
   DDCA_Status ddcrc = DDCRC_OK;

   char * class_drm_dir =
#ifdef TARGET_BSD
         "/compat/sys/class/drm";
#else
         "/sys/class/drm";
#endif
   Byte_Bit_Flags drm_card_numbers = get_sysfs_drm_card_numbers();
   if (bbf_count_set(drm_card_numbers) == 0) {
      rpt_vstring(0, "No video cards found in %s. Disabling experimental detection of display hotplug events.", class_drm_dir);
      ddcrc = DDCRC_INVALID_OPERATION;
   }
   else {

      g_mutex_lock(&watch_thread_mutex);

      if (watch_thread)
         ddcrc = DDCRC_INVALID_OPERATION;
      else {
         terminate_watch_thread = false;
         Watch_Displays_Data * data = calloc(1, sizeof(Watch_Displays_Data));
         memcpy(data->marker, WATCH_DISPLAYS_DATA_MARKER, 4);
         data->display_change_handler = dummy_display_change_handler;
         data->main_process_id = getpid();
         // data->main_thread_id = syscall(SYS_gettid);
         data->main_thread_id = get_thread_id();
         data->drm_card_numbers = drm_card_numbers;
         watch_thread = g_thread_new(
                          "watch_displays",             // optional thread name
   #if ENABLE_UDEV
                          watch_displays_using_udev,
   #else
                          watch_displays_using_poll,
   #endif
                          data);
      }

      g_mutex_unlock(&watch_thread_mutex);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "watch_thread=%p, returning: %s", watch_thread, ddcrc_desc_t(ddcrc));
   return ddcrc;
}


// only makes sense if polling!
// locks udev_monitor_receive_device() blocks

/** Halts thread that watches for addition or removal of displays.
 *
 *  Does not return until the watch thread exits.
 *
 *  \retval  DDCRC_OK
 *  \retval  DDCRC_INVALID_OPERATION  no watch thread running
 */
DDCA_Status
ddc_stop_watch_displays()
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "" );
   DDCA_Status ddcrc = DDCRC_OK;
   g_mutex_lock(&watch_thread_mutex);

   // does not work if watch_displays_using_udev(), loop doesn't wake up unless there's a udev event
   if (watch_thread) {
      terminate_watch_thread = true;  // signal watch thread to terminate
#ifndef ENABLE_UDEV
      // if using udev, thread never terminates because udev_monitor_receive_device() is blocking,
      // so termiate flag doesn't get checked
      // no big deal, ddc_stop_watch_displays() is only called at program termination to
      // release resources for tidyness
      g_thread_join(watch_thread);
#endif
      watch_thread = NULL;
   }
   else
      ddcrc = DDCRC_INVALID_OPERATION;

   g_mutex_unlock(&watch_thread_mutex);
   DBGTRC_RETURNING(debug, TRACE_GROUP, ddcrc, "watch_thread=%p", watch_thread);
   return ddcrc;
}


void init_ddc_watch_displays() {
   RTTI_ADD_FUNC(check_displays);
   RTTI_ADD_FUNC(ddc_start_watch_displays);
   RTTI_ADD_FUNC(ddc_stop_watch_displays);
   RTTI_ADD_FUNC(dummy_display_change_handler);
   RTTI_ADD_FUNC(watch_displays_using_poll);
#ifdef ENABLE_UDEV
   RTTI_ADD_FUNC(watch_displays_using_udev);
#endif
}
