/* query_sysenv.c
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


// #include <base/parms.h>    // put first for USE_LIBEXPLAIN

#include <config.h>

#define _GNU_SOURCE 1       // for function group_member

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <grp.h>
#include <limits.h>
// #include <libosinfo-1.0/osinfo/osinfo.h>
// #include <libudev.h>        // not yet used
#include <linux/hiddev.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include "util/device_id_util.h"
#include "util/edid.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/x11_util.h"

#ifdef USE_USB
#include "util/udev_util.h"
#include "usb_util/hiddev_reports.h"
#include "usb_util/hiddev_util.h"
// #include "usb_util/hidapi_util.h"
#include "usb_util/hidraw_util.h"
// #include "util/libusb_reports.h"
#include "usb_util/libusb_util.h"
#include "usb_util/usb_hid_common.h"
#endif

#include "base/core.h"
#include "base/linux_errno.h"

#include "i2c/i2c_bus_core.h"
#include "adl/adl_shim.h"
#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "app_ddcutil/query_sysenv.h"


#ifndef MAX_PATH
#define MAX_PATH 256
#endif


static char * known_video_driver_modules[] = {
      "amdgpu",
      "fglrx",
      "nvidia",
      "nouveau",
      "radeon",
      "vboxvideo",
      NULL
};

static char * prefix_matches[] = {
      "amdgpu",
      "i2c",
      "video",
      NULL
};

static char * other_driver_modules[] = {
      "i2c_dev",
      "i2c_algo_bit",
      "i2c_piix4",
      NULL
};


struct driver_name_node;

struct driver_name_node {
   char * driver_name;
   struct driver_name_node * next;
};



char * read_sysfs_attr(char * dirname, char * attrname, bool verbose) {
   char fn[MAX_PATH];
   sprintf(fn, "%s/%s", dirname, attrname);
   return file_get_first_line(fn, verbose);
}


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


static int query_proc_modules_for_video() {
   int rc = 0;

   GPtrArray * garray = g_ptr_array_sized_new(300);

   printf("Scanning /proc/modules for driver environment...\n");
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
            printf("   Loaded drm module depends on: %s\n", mod_dependencies);
         }
         else if (streq(mod_name, "video") ) {
            printf("   Loaded video module depends on: %s\n", mod_dependencies);
         }
         else if (exactly_matches_any(mod_name, known_video_driver_modules) >= 0 ) {
            printf("   Found video driver module: %s\n", mod_name);
         }
         else if ( starts_with_any(mod_name, prefix_matches) >= 0 ) {
            printf("   Found other loaded module: %s\n", mod_name);
         }
      }
   }

   return rc;
}


static bool show_one_file(char * dir_name, char * simple_fn, bool verbose, int depth) {
   bool result = false;
   char fqfn[400];
   strcpy(fqfn,dir_name);
   if (!str_ends_with(dir_name, "/"))
      strcat(fqfn,"/");
   strcat(fqfn,simple_fn);
   if (regular_file_exists(fqfn)) {
      rpt_vstring(depth, "%s:", fqfn);
      rpt_file_contents(fqfn, depth+1);
      result = true;
   }
   else if (verbose)
      rpt_vstring(depth, "File not found: %s", fqfn);
   return result;
}


static bool query_proc_driver_nvidia() {
   bool debug = true;
   bool result = false;
   char * dn = "/proc/driver/nvidia/";
   if ( directory_exists(dn) ) {
      printf("Examining /proc/driver/nvidia:\n");
      result = true;
      show_one_file(dn, "version",  debug, 1);
      show_one_file(dn, "registry", debug, 1);
      show_one_file(dn, "params",   debug, 1);
      char * dn_gpus = "/proc/driver/nvidia/gpus/";
      if (directory_exists(dn_gpus)) {
         DIR * dp = opendir(dn_gpus);
         struct dirent * ep;

         while ( (ep = readdir(dp)) ) {
            if ( !streq(ep->d_name,".") && !streq(ep->d_name, "..") ) {
               // puts(ep->d_name);
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
                  show_one_file(dirbuf, "information", debug, 1);
                  show_one_file(dirbuf, "registry",    debug, 1);
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


bool only_fglrx(struct driver_name_node * driver_list) {
   int driverct = 0;
   bool fglrx_seen = false;
   struct driver_name_node * curnode = driver_list;
   while (curnode) {
      driverct++;
      if (str_starts_with(curnode->driver_name, "fglrx"))
         fglrx_seen = true;
      curnode = curnode->next;
   }
   bool result = (driverct == 1 && fglrx_seen);
   // DBGMSG("driverct = %d, returning %d", driverct, result);
   return result;
}


static bool only_nvidia_or_fglrx(struct driver_name_node * driver_list) {
   int driverct = 0;
   bool other_driver_seen = false;
   struct driver_name_node * curnode = driver_list;
   while (curnode) {
      driverct++;
      if (!str_starts_with(curnode->driver_name, "fglrx") &&
          !streq(curnode->driver_name, "nvidia")
         )
      {
         other_driver_seen = true;
      }
      curnode = curnode->next;
   }
   bool result = (!other_driver_seen && driverct > 0);
   // DBGMSG("driverct = %d, returning %d", driverct, result);
   return result;
}


static bool found_driver(struct driver_name_node * driver_list, char * driver_name) {
   bool found = false;
   struct driver_name_node * curnode = driver_list;
   while (curnode) {
      if ( str_starts_with(curnode->driver_name, driver_name) )
         found = true;
      curnode = curnode->next;
   }
   // DBGMSG("driver_name=%s, returning %d", driver_name, found);
   return found;
}


static void query_base_env() {
   printf("\nSystem information (uname):\n");
   // uname response:
   char * version_line = file_get_first_line("/proc/version", true /* verbose */);
   if (version_line)
      printf("   %s\n", version_line);
   else
      printf("   System information unavailable\n");
}



static void check_i2c_devices(struct driver_name_node * driver_list) {
   bool debug = false;
   // int rc;
   // char username[32+1];       // per man useradd, max username length is 32
   char *uname = NULL;
   // bool have_i2c_devices = false;

   printf("\nChecking /dev/i2c-* devices...\n");
   Output_Level output_level = get_output_level();

   bool just_fglrx = only_fglrx(driver_list);
   if (just_fglrx){
      printf("\nApparently using only the AMD proprietary driver fglrx.\n"
             "Devices /dev/i2c-* are not required.\n");
      if (output_level >= OL_VERBOSE)
         printf("/dev/i2c device detail is purely informational.\n");
      else
         return;
   }

   printf("\nUnless the system is using the AMD proprietary driver fglrx, devices /dev/i2c-*\n"
          "must exist and the logged on user must have read/write permission for those\n"
          "devices (or at least those devices associated with monitors).\n"
          "Typically, this access is enabled by:\n"
          "  - setting the group for /dev/i2c-* to i2c\n"
          "  - setting group RW permissions for /dev/i2c-*\n"
          "  - making the current user a member of group i2c\n"
          "Alternatively, this could be enabled by just giving everyone RW permission\n"
          "The following tests probe for these conditions.\n"
         );

   printf("\nChecking for /dev/i2c-* devices...\n");
   execute_shell_cmd("ls -l /dev/i2c-*", 1);

#ifdef OLD
   rc = getlogin_r(username, sizeof(username));
   printf("(%s) getlogin_r() returned %d, strlen(username)=%zd\n", __func__,
          rc, strlen(username));
   if (rc == 0)
      printf("(%s) username = |%s|\n", __func__, username);
   // printf("\nLogged on user:  %s\n", username);
   printf("(%s) getlogin() returned |%s|\n", __func__, getlogin());
   char * cmd = "echo $LOGNAME";
   printf("(%s) executing command: %s\n", __func__, cmd);
   bool ok = execute_shell_cmd(cmd, 0);
   printf("(%s) execute_shell_cmd() returned %s\n", __func__, bool_repr(ok));

#endif
   uid_t uid = getuid();
   // uid_t euid = geteuid();
   // printf("(%s) uid=%u, euid=%u\n", __func__, uid, euid);
   struct passwd *  pwd = getpwuid(uid);
   printf("\nCurrent user: %s (%u)\n\n", pwd->pw_name, uid);
   uname = strdup(pwd->pw_name);

   bool all_i2c_rw = false;
   int busct = i2c_get_busct();   // Consider replacing with local code
   if (busct == 0 && !just_fglrx) {
      printf("WARNING: No /dev/i2c-* devices found\n");
   }
   else {
      all_i2c_rw = true;
      int  busno;
      char fnbuf[20];

      for (busno=0; busno < 32; busno++) {
         if (i2c_bus_exists(busno)) {
            snprintf(fnbuf, sizeof(fnbuf), "/dev/i2c-%d", busno);
            int rc;
            int errsv;
            DBGMSF(debug, "Calling access() for %s", fnbuf);
            rc = access(fnbuf, R_OK|W_OK);
            if (rc < 0) {
               errsv = errno;
               printf("Device %s is not readable and writable.  Error = %s\n",
                      fnbuf, linux_errno_desc(errsv) );
               all_i2c_rw = false;
            }
         }
      }

      if (!all_i2c_rw) {
         printf("WARNING: Current user (%s) does not have RW access to all /dev/i2c-* devices.\n",
 //               username);
                uname);
      }
      else
         printf("Current user (%s) has RW access to all /dev/i2c-* devices.\n",
               // username);
               uname);
   }

   if (!all_i2c_rw || output_level >= OL_VERBOSE) {
      printf("\nChecking for group i2c...\n");
      // replaced by C code
      // execute_shell_cmd("grep i2c /etc/group", 1);

      bool group_i2c_exists = false;   // avoid special value in gid_i2c
      // gid_t gid_i2c;
      struct group * pgi2c = getgrnam("i2c");
      if (pgi2c) {
         printf("   Group i2c exists\n");
         group_i2c_exists = true;
         // gid_i2c = pgi2c->gr_gid;
         // DBGMSG("getgrnam returned gid=%d for group i2c", gid_i2c);
         // DBGMSG("getgrnam() reports members for group i2c: %s", *pgi2c->gr_mem);
         int ndx=0;
         char * curname;
         bool found_curuser = false;
         while ( (curname = pgi2c->gr_mem[ndx]) ) {
            rtrim_in_place(curname);
            // DBGMSG("member_names[%d] = |%s|", ndx, curname);
            if (streq(curname, uname /* username */)) {
               found_curuser = true;
            }
            ndx++;
         }
         if (found_curuser) {
            printf("   Current user %s is a member of group i2c\n", uname  /* username */);
         }
         else {
            printf("   WARNING: Current user %s is NOT a member of group i2c\n", uname /*username*/);

         }
      }
      if (!group_i2c_exists) {
         printf("   Group i2c does not exist\n");
      }
      free(uname);
   #ifdef BAD
      // getgroups, getgrouplist returning nonsense
      else {
         uid_t uid = geteuid();
         gid_t gid = getegid();
         struct passwd * pw = getpwuid(uid);
         printf("Effective uid %d: %s\n", uid, pw->pw_name);
         char * uname = strdup(pw->pw_name);
         struct group * pguser = getgrgid(gid);
         printf("Effective gid %d: %s\n", gid, pguser->gr_name);
         if (group_member(gid_i2c)) {
            printf("User %s (%d) is a member of group i2c (%d)\n", uname, uid, gid_i2c);
         }
         else {
            printf("WARNING: User %s (%d) is a not member of group i2c (%d)\n", uname, uid, gid_i2c);
         }

         size_t supp_group_ct = getgroups(0,NULL);
         gid_t * glist = calloc(supp_group_ct, sizeof(gid_t));
         int rc = getgroups(supp_group_ct, glist);
         int errsv = errno;
         DBGMSF(debug, "getgroups() returned %d", rc);
         if (rc < 0) {
            DBGMSF(debug, "getgroups() returned %d", rc);

         }
         else {
            DBGMSG("Found %d supplementary group ids", rc);
            int ndx;
            for (ndx=0; ndx<rc; ndx++) {
               DBGMSG("Supplementary group id: %d", *glist+ndx);
            }

         }

         int supp_group_ct2 = 100;
         glist = calloc(supp_group_ct2, sizeof(gid_t));
         DBGMSG("Calling getgrouplist for user %s", uname);
         rc = getgrouplist(uname, gid, glist, &supp_group_ct2);
         errsv = errno;
         DBGMSG("getgrouplist returned %d, supp_group_ct=%d", rc, supp_group_ct2);
         if (rc < 0) {
            DBGMSF(debug, "getgrouplist() returned %d", rc);
         }
         else {
            DBGMSG("getgrouplist found %d supplementary group ids", rc);
            int ndx;
            for (ndx=0; ndx<rc; ndx++) {
               DBGMSG("Supplementary group id: %d", *glist+ndx);
            }
         }
      }
   #endif

      printf("\nLooking for udev nodes files that reference i2c:\n");
      execute_shell_cmd("grep -H i2c /etc/udev/makedev.d/*", 1);
      printf("\nLooking for udev rules files that reference i2c:\n");
      execute_shell_cmd("grep -H i2c "
                        "/lib/udev/rules.d/*rules "
                        "/run/udev/rules.d/*rules "
                        "/etc/udev/rules.d/*rules", 1 );
   }
}


/* Looks in the /sys file system to check if a module is loaded.
 *
 * Arguments:
 *   module_name    module name
 *
 * Returns:         true if the module is loaded, false if not
 */
bool is_module_loaded_using_sysfs(char * module_name) {
   bool debug = false;
   struct stat statbuf;
   char   module_fn[100];
   bool found = false;
   snprintf(module_fn, sizeof(module_fn), "/sys/module/%s", module_name);
   int rc = stat(module_fn, &statbuf);
   if (rc < 0) {
      // int errsv = errno;
      // will be ENOENT (2) if file not found
      // DBGMSF(debug, "stat(%s) returned %d, errno = %s",
      //       module_fn, rc, linux_errno_desc(errsv));
      found = false;
   }
   else {
      // if (S_ISDIR(statbuf.st_mode))   // pointless
         found = true;
   }
   DBGMSF(debug, "module_name = %s, returning %d", module_name, found);
   return found;
}


/* Checks if a module is built in to the kernel.
 *
 * Arguments:
 *   module_name    simple module name, as it appears in the file system, e.g. i2c-dev
 *
 * Returns:         true/false
 */
static bool is_module_builtin(char * module_name) {
   bool debug = false;
   bool result = false;

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);
   // DBGMSG("uname() returned release: %s", &utsbuf.release);

   // works, but simpler to use uname() that doesn't require free(osrelease)
   // char * osrelease = file_get_first_line("/proc/sys/kernel/osrelease", true /* verbose */);
   // assert(streq(utsbuf.release, osrelease));

   char modules_builtin_fn[100];
   snprintf(modules_builtin_fn, 100, "/lib/modules/%s/modules.builtin", utsbuf.release);
   // free(osrelease);

   char cmdbuf[200];

   snprintf(cmdbuf, 200, "grep -H %s.ko %s", module_name, modules_builtin_fn);
   // DBGMSG("cmdbuf = |%s|", cmdbuf);

   GPtrArray * response = execute_shell_cmd_collect(cmdbuf);
   // internal rc =  0 if found, 256 if not found
   // returns 0 lines if not found

   // DBGMSG("execute_shell_cmd_collect() returned %d lines", response->len);
   // for (int ndx = 0; ndx < response->len; ndx++) {
   //    puts(g_ptr_array_index(response, ndx));
   // }

   result = (response->len > 0);
   g_ptr_array_free(response, true);

   DBGMSF(debug, "module_name = %s, returning %s", module_name, bool_repr(result));
   return result;
}



/* Checks if module i2c_dev is required and if so whether it is loaded.
 * Reports the result.
 *
 * Arguments:
 *    video_driver_list  list of video drivers
 *
 * Returns:              nothing
 */
static void check_i2c_dev_module(struct driver_name_node * video_driver_list) {
   printf("\nChecking for module i2c_dev...\n");

   Output_Level output_level = get_output_level();

   bool module_required = !only_nvidia_or_fglrx(video_driver_list);
   if (!module_required) {
      printf("Using only proprietary nvidia or fglrx driver. Module i2c_dev not required.\n");
      if (output_level < OL_VERBOSE)
         return;
      printf("Remaining i2c_dev detail is purely informational.\n");
   }

   bool is_builtin = is_module_builtin("i2c-dev");
   printf("   Module %-16s is %sbuilt into kernel\n", "i2c_dev", (is_builtin) ? "" : "NOT ");
   if (is_builtin) {
      if (output_level < OL_VERBOSE)
         return;
      if (module_required)  // no need for duplicate message
         printf("Remaining i2c_dev detail is purely informational.\n");
   }

   bool is_loaded = is_module_loaded_using_sysfs("i2c_dev");
      // DBGMSF(debug, "is_loaded=%d", is_loaded);
   if (!is_builtin)
      printf("   Module %-16s is %sloaded\n", "i2c_dev", (is_loaded) ? "" : "NOT ");

   if ( (!is_loaded && !is_builtin) || output_level >= OL_VERBOSE) {
      printf("\nCheck that kernel module i2c_dev is being loaded by examining files where this would be specified...\n");
      execute_shell_cmd("grep -H i2c[-_]dev "
                        "/etc/modules "
                        "/etc/modules-load.d/*conf "
                        "/run/modules-load.d/*conf "
                        "/usr/lib/modules-load.d/*conf "
                        , 1);

      printf("\nCheck for any references to i2c_dev in /etc/modprobe.d ...\n");
      execute_shell_cmd("grep -H i2c[-_]dev "
                        "/etc/modprobe.d/*conf "
                        "/run/modprobe.d/*conf "
                        , 1);
   }
}


static void query_packages() {
   printf("\nddcutil requiries package i2c-tools.  Use both dpkg and rpm to look for it.\n"
          "While we're at it, check for package libi2c-dev which is used for building\n"
          "ddcutil.\n"
         );

   bool ok;
   // n. apt show produces warning msg that format of output may change.
   // better to use dpkg
   ok = printf("\nUsing dpkg to look for package i2c-tools...\n");
   execute_shell_cmd("dpkg --status i2c-tools", 1);
   if (!ok)
      printf("dpkg command not found\n");
   else {
      execute_shell_cmd("dpkg --listfiles i2c-tools", 1);
   }

   ok = printf("\nUsing dpkg to look for package libi2c-dev...\n");
   execute_shell_cmd("dpkg --status libi2c-dev", 1);
   if (!ok)
      printf("dpkg command not found\n");
   else {
      execute_shell_cmd("dpkg --listfiles libi2c-dev", 1);
   }

   printf("\nUsing rpm to look for package i2c-tools...\n");
   ok = execute_shell_cmd("rpm -q -l --scripts i2c-tools", 1);
   if (!ok)
      printf("rpm command not found\n");
}


static bool query_card_and_driver_using_lspci() {
   // DBGMSG("Starting");
   bool ok = true;
   FILE * fp;

   printf("Using lspci to examine driver environment...\n");
   fp = popen("lspci", "r");
   if (!fp) {
      // int errsv = errno;
      printf("Unable to execute command lspci: %s\n", strerror(errno));

      printf("lspci command unavailable\n");       // why doesn't this print?
      printf("lspci command really unavailable\n");  // or this?
      ok = false;
   }
   else {
      char * a_line = NULL;
      size_t len = 0;
      ssize_t read;
      char pci_addr[15];
      // char device_title[100];
      char device_name[300];
      while ( (read=getline(&a_line, &len, fp)) != -1) {
         if (strlen(a_line) > 0)
            a_line[strlen(a_line)-1] = '\0';
         // UGLY UGLY - WHY DOESN'T SCANF WORK ???
         // DBGMSG("lspci line: |%s|", a_line);
#ifdef SCAN_FAILS
         // doesn't find ':'
         // char * pattern = "%s %s:%s";
         char * pattern = "%[^' '],%[^':'], %s";
         int ct = sscanf(a_line, pattern, pci_addr, device_title, device_name);

         DBGMSG("ct=%d, t_read=%ld, pci_addr=%s, device_title=%s", ct, len, pci_addr, device_title);
         if (ct == 3) {
            if ( str_starts_with("VGA", device_title) ) {
               printf("Video controller: %s\n", device_name);
            }
         }
#endif
         int ct = sscanf(a_line, "%s %s", pci_addr, device_name);
         // DBGMSG("ct=%d, t_read=%ld, pci_addr=%s, device_name=%s", ct, len, pci_addr, device_name);
         if (ct == 2) {
            if ( str_starts_with("VGA", device_name) ) {
               // printf("Video controller 0: %s\n", device_name);
               char * colonpos = strchr(a_line + strlen(pci_addr), ':');
               if (colonpos)
                  printf("Video controller: %s\n", colonpos+1);
               else
                  printf("colon not found\n");
            }
         }
      }
      pclose(fp);
   }
   return ok;
}



static struct driver_name_node * query_card_and_driver_using_sysfs() {
   // bool debug = true;
   printf("Obtaining card and driver information from /sys...\n");

   // also of possible interest:
   // /sys/class/i2c-dev/i2c-*/name
   //    refers to video driver or piix4_smbus
   // also accessed at:
   // /sys/bus/i2c/devices/i2c-*/name
   // /sys/bus/pci/drivers/nouveau
   // /sys/bus/pci/drivers/piix4_smbus
   // /sys/bus/pci/drivers/nouveau/0000:01:00.0
   //                                           /name
   //                                           i2c-dev
   // /sys/module/nvidia
   // /sys/module/i2c_dev ?
   // /sys/module/... etc

   // bool ok = true;
   char * driver_name = NULL;
   struct driver_name_node * driver_list = NULL;

   struct dirent *dent;
   DIR           *d;

   char * d0 = "/sys/bus/pci/devices";
   d = opendir(d0);
   if (!d) {
      printf("Unable to open directory %s: %s\n", d0, strerror(errno));
   }
   else {
      while ((dent = readdir(d)) != NULL) {
         // DBGMSG("%s", dent->d_name);

         char cur_fn[100];
         char cur_dir_name[100];
         if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
            sprintf(cur_dir_name, "%s/%s", d0, dent->d_name);
            sprintf(cur_fn, "%s/class", cur_dir_name);
            char * class_id = read_sysfs_attr(cur_dir_name, "class", true);
            // printf("%s: |%s|\n", cur_fn, class_id);
            // if (streq(class_id, "0x030000")) {
            if (str_starts_with(class_id, "0x03")) {
               // printf("%s = 0x030000\n", cur_fn);

#ifdef WORKS
               printf("\nReading values from individual attribute files:\n");
               printf("vendor: %s\n", read_sysfs_attr(cur_dir_name, "vendor", true));
               printf("device: %s\n", read_sysfs_attr(cur_dir_name, "device", true));
               printf("subsystem_device: %s\n", read_sysfs_attr(cur_dir_name, "subsystem_device", true));
               printf("subsystem_vendor: %s\n", read_sysfs_attr(cur_dir_name, "subsystem_vendor", true));
#endif
               char * modalias = read_sysfs_attr(cur_dir_name, "modalias", true);
               // printf("modalias: %s\n", modalias);

               printf("\nDetermining driver name and possibly version...\n");
               // DBGMSG("cur_dir_name: %s", cur_dir_name);
               char workfn[PATH_MAX];
               sprintf(workfn, "%s/%s", cur_dir_name, "driver");
               char resolved_path[PATH_MAX];
               char * rpath = realpath(workfn, resolved_path);
               if (!rpath) {
                  int errsv = errno;
                  if (errsv == ENOENT) {
                     // fail in virtual environment?
                     printf("Cannot determine driver name\n");
                  }
                  else {
                     DBGMSG("realpath(%s) returned NULL, errno=%d (%s)", workfn, errsv, linux_errno_name(errsv));
                  }
               }
               else {
                  // printf("realpath returned %s\n", rpath);
                  // printf("%s --> %s\n",workfn, resolved_path);
                  char * final_slash_ptr = strrchr(rpath, '/');
                  // TODO: handle case where there are more than 1 video drivers loaded,
                  // say if the system contains both an AMD and Nvidia card
                  driver_name = final_slash_ptr+1;
                  printf(    "   Driver name:    %s\n", driver_name);
                  struct driver_name_node * new_node = calloc(1, sizeof(struct driver_name_node));
                  new_node->driver_name = strdup(driver_name);
                  new_node->next = driver_list;
                  driver_list = new_node;


                  char driver_module_dir[PATH_MAX];
                  sprintf(driver_module_dir, "%s/driver/module", cur_dir_name);
                  // printf("driver_module_dir: %s\n", driver_module_dir);
                  char * driver_version = read_sysfs_attr(driver_module_dir, "version", false);
                  if (driver_version)
                      printf("   Driver version: %s\n", driver_version);
                  else
                     printf( "   Unable to determine driver version\n");
               }


               // printf("\nParsing modalias for values...\n");
               char * colonpos = strchr(modalias, ':');
               assert(*(colonpos+1) == 'v');    // vendor_id
               char * vendor_id = substr(colonpos, 2, 8);
               // printf("vendor_id:        %s\n", vendor_id);
               assert(*(colonpos+10) == 'd');
               char * device_id = lsub(colonpos+11,8);
               // printf("device_id:        %s\n", device_id);
               assert( *(colonpos+19) == 's');
               assert( *(colonpos+20) == 'v');
               char * subsystem_vendor = lsub(colonpos+21,8);
               // printf("subsystem_vendor: %s\n", subsystem_vendor);
               assert( *(colonpos+29) == 's');
               assert( *(colonpos+30) == 'd');
               char * subsystem_device = lsub(colonpos+31,8);
               // printf("subsystem_device: %s\n", subsystem_device);
               assert( *(colonpos+39) == 'b');
               assert( *(colonpos+40) == 'c');
               // not used
               //char * base_class = lsub(colonpos+41,2);
               // printf("base_class:       %s\n", base_class);     // bytes 0-1 of value from class
               assert( *(colonpos+43) == 's');
               assert( *(colonpos+44) == 'c');
               // not used
               // char * sub_class = lsub(colonpos+45,2);          // bytes 1-2 of value from class
               // printf("sub_class:        %s\n", sub_class);
               assert( *(colonpos+47) == 'i');
               // not used
               // char * interface_id = lsub(colonpos+48,2);
               // printf("interface_id:     %s\n", interface_id);  // bytes 4-5 of value from class?


               // printf("\nConverting modalias strings to ushort...\n");
               ushort xvendor_id    = h2ushort(vendor_id);
               ushort xdevice_id    = h2ushort(device_id);
               ushort xsubvendor_id = h2ushort(subsystem_vendor);
               ushort xsubdevice_id = h2ushort(subsystem_device);

               // printf("\nLooking up names in pci.ids...\n");
               printf("\nVideo card identification:\n");
               bool pci_ids_ok = devid_ensure_initialized();
               if (pci_ids_ok) {
                  Pci_Usb_Id_Names names = devid_get_pci_names(
                                  xvendor_id,
                                  xdevice_id,
                                  xsubvendor_id,
                                  xsubdevice_id,
                                  4);
                  if (!names.vendor_name)
                     names.vendor_name = "unknown vendor";
                  if (!names.device_name)
                     names.device_name = "unknown device";

                  printf("   Vendor:              %04x       %s\n", xvendor_id, names.vendor_name);
                  printf("   Device:              %04x       %s\n", xdevice_id, names.device_name);
                  if (names.subsys_or_interface_name)
                  printf("   Subvendor/Subdevice: %04x/%04x  %s\n", xsubvendor_id, xsubdevice_id, names.subsys_or_interface_name);
               }
               else {
                  printf("Unable to find pci.ids file for name lookup.\n");
                  printf("   Vendor:              %04x       \n", xvendor_id);
                  printf("   Device:              %04x       \n", xdevice_id);
                  printf("   Subvendor/Subdevice: %04x/%04x  \n", xsubvendor_id, xsubdevice_id);
               }
               free(vendor_id);
               free(device_id);
               free(subsystem_vendor);
               free(subsystem_device);
            }
         }
      }
      closedir(d);
   }

   return driver_list;
}


/* Performs checks specific to the nvidia and fglrx proprietary video drivers.
 *
 * Arguments:
 *    driver list    list of loaded drivers
 *
 * Returns:          nothing
 */
static void driver_specific_tests(struct driver_name_node * driver_list) {
   printf("\nPerforming driver specific checks...\n");
   bool found_driver_specific_checks = false;
   if (found_driver(driver_list, "nvidia")) {
      found_driver_specific_checks = true;
      printf("\nChecking for special settings for proprietary Nvidia driver \n");
      printf("(needed for some newer Nvidia cards).\n");
      execute_shell_cmd("grep -iH i2c /etc/X11/xorg.conf /etc/X11/xorg.conf.d/*", 1);
   }

   if (found_driver(driver_list, "fglrx")) {
      found_driver_specific_checks = true;
      printf("\nPerforming ADL specific checks...\n");
#ifdef HAVE_ADL
     if (!adlshim_is_available()) {
        set_output_level(OL_VERBOSE);  // force error msg that names missing dll
        bool ok = adlshim_initialize();
        if (!ok)
           printf("WARNING: Using AMD proprietary video driver fglrx but unable to load ADL library");
     }
#else
     printf("WARNING: Using AMD proprietary video driver fglrx but ddcutil built without ADL support");
#endif
   }
   if (!found_driver_specific_checks)
      printf("No driver specific checks apply.");
}


static void query_loaded_modules_using_sysfs() {
   printf("\nTesting if modules are loaded using /sys...\n");
   // known_video_driver_modules
   // other_driver_modules

   char ** pmodule_name = known_video_driver_modules;
   char * curmodule;
   int ndx;
   for (ndx=0; (curmodule=pmodule_name[ndx]) != NULL; ndx++) {
      bool is_loaded = is_module_loaded_using_sysfs(curmodule);
      // DBGMSF(debug, "is_loaded=%d", is_loaded);
      printf("   Module %-16s is %sloaded\n", curmodule, (is_loaded) ? "" : "NOT ");
   }
   pmodule_name = other_driver_modules;
   for (ndx=0; (curmodule=pmodule_name[ndx]) != NULL; ndx++) {
      bool is_loaded = is_module_loaded_using_sysfs(curmodule);
      printf("   Module %-16s is %sloaded\n", curmodule, (is_loaded) ? "" : "NOT ");
   }
}


static void query_i2c_bus_using_sysfs() {
   struct dirent *dent;
   DIR           *d;
   char          *dname;

   printf("\nExamining /sys/bus/i2c/devices...\n");
   dname = "/sys/bus/i2c";
   d = opendir(dname);
   if (!d) {
      rpt_vstring(1, "i2c bus not defined in sysfs. Unable to open directory %s: %s\n",
                     dname, strerror(errno));
   }
   else {
      closedir(d);
      dname = "/sys/bus/i2c/devices";
      d = opendir(dname);
      if (!d) {
         rpt_vstring(1, "Unable to open sysfs directory %s: %s\n", dname, strerror(errno));
      }
      else {
         bool i2c_seen = false;
         while ((dent = readdir(d)) != NULL) {
            // DBGMSF("%s", dent->d_name);
            // char cur_fn[100];
            char cur_dir_name[100];
            if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
               // DBGMSF(debug, "dent->dname: %s", dent->d_name);
               sprintf(cur_dir_name, "%s/%s", dname, dent->d_name);
               char * dev_name = read_sysfs_attr(cur_dir_name, "name", true);
               rpt_vstring(1, "%s/name: %s", cur_dir_name, dev_name);
               i2c_seen = true;
            }
         }
         if (!i2c_seen)
            rpt_vstring(1, "No i2c devices found in %s", dname);
         closedir(d);
      }
   }
}


#ifdef UNUSED
static bool query_card_and_driver_using_osinfo() {
   bool ok = false;

#ifdef FAILS
   printf("Trying Osinfo\n");

   OsinfoDb * info_db = osinfo_db_new();

   OsinfoDeviceList * device_list = osinfo_db_get_device_list(info_db);
   gint device_ct = osinfo_list_get_length(device_list);
   int ndx = 0;
   for (ndx=0; ndx < ct; ndx++) {
      OsinfoEntity * entity = osinfo_list_get_nth(device_list, ndx);
      char * entity_id = osinfo_entity_get_id(entity);
      DBGMSG("osinfo entity id = %s", entity_id );

   }
#endif

   return ok;
}
#endif


static void query_i2c_buses() {
   printf("\nExamining i2c buses...\n");
   i2c_report_buses(true, 1 /* indentation depth */);
}


/* Reports EDIDs known to X11
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
void query_x11() {
   GPtrArray* edid_recs = get_x11_edids();
   puts("");
   printf("EDIDs reported by X11 for connected xrandr outputs:\n");
   // DBGMSG("Got %d X11_Edid_Recs\n", edid_recs->len);

   for (int ndx=0; ndx < edid_recs->len; ndx++) {
      X11_Edid_Rec * prec = g_ptr_array_index(edid_recs, ndx);
      // printf(" Output name: %s -> %p\n", prec->output_name, prec->edid);
      // hex_dump(prec->edid, 128);
      rpt_vstring(1, "xrandr output: %s", prec->output_name);
      Parsed_Edid * parsed_edid = create_parsed_edid(prec->edidbytes);
      if (parsed_edid) {
         bool verbose_edid = false;
         report_parsed_edid(parsed_edid, verbose_edid, 2 /* depth */);
         free_parsed_edid(parsed_edid);
      }
      else {
         printf(" Unparsable EDID for output name: %s -> %p\n", prec->output_name, prec->edidbytes);
         hex_dump(prec->edidbytes, 128);
      }
   }
   free_x11_edids(edid_recs);

   // Display * x11_disp = open_default_x11_display();
   // GPtrArray *  outputs = get_x11_connected_outputs(x11_disp);
   // close_x11_display(x11_disp);
}


/* Uses i2cdetect to probe active addresses on I2C buses
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
static void query_using_i2cdetect() {
   printf("Examining I2C buses using i2cdetect: \n");
   GPtrArray * busnames = execute_shell_cmd_collect("ls /dev/i2c*");
   for (int ndx = 0; ndx < busnames->len; ndx++) {
      // printf("ndx=%d, value=|%s|\n", ndx, (char *) g_ptr_array_index(busnames, ndx));
      char cmd[80];
      char * busname = (char *) g_ptr_array_index(busnames, ndx);
      busname+=9;   // strip off "/dev/i2c-"
      snprintf(cmd, 80, "i2cdetect -y %s", busname);
      printf("\nProbing bus /dev/i2c-%d using command \"%s\"\n", ndx, cmd);
      // DBGMSG("Executing command: |%s|\n", cmd);
      int rc = execute_shell_cmd(cmd, 1 /* depth */);
      // DBGMSG("execute_shell_cmd(\"%s\") returned %d", cmd, rc);
      if (rc != 1) {
         printf("i2cdetect command unavailable\n");
         break;
      }
   }
   g_ptr_array_free(busnames, true);
}


#ifdef USE_USB

static bool is_hid_monitor_rdesc(const char * fn) {
   bool debug = false;
   bool result = false;

   char * first_line = file_get_first_line(fn, /*verbose=*/ true);
   DBGMSF(debug, "First line: %s", first_line);
   if ( first_line && str_starts_with( first_line, "05 80"))
      result = true;

   DBGMSF(debug, "fn=%s, returning: %s", fn, bool_repr(result));
   return result;
}


/* Probe using the UHID debug interface in /sys/kernel/debug/hid
 *
 * For each HID device that's a monitor, displays the HID Report Descriptor.
 *
 * Arguments:
 *    depth     logical indentation depth
 *
 * Returns:     nothing
 */
static void probe_uhid(int depth) {
   int d1 = depth+1;
   int d2 = depth+2;

   bool debug = false;
   DBGMSF(debug, "Starting");

   struct dirent * ep;
   char * dirname = "/sys/kernel/debug/hid/";
   DIR * dp;
   dp = opendir(dirname);
   if (!dp) {
      int errsv = errno;
      printf("Unable to open directory %s: %s\n", dirname, strerror(errsv));
   }
   else {
      while ( (ep = readdir(dp))) {
         // puts(ep->d_name);
         char fqfn[MAX_PATH];
         if (!streq(ep->d_name, ".") && !streq(ep->d_name, "..") ) {
            // file names look like: "0003:0424:3328:004D"
            // field 1:    ?
            // field 2:    vid
            // field 3:    pid
            // field 4:    appears to be a sequence number of some sort
            //             increases with each call to ddcutil env -v
            snprintf(fqfn, MAX_PATH, "%s%s/rdesc", dirname, ep->d_name);
            // puts(fqfn);
#ifdef FAILS
            puts(ep->d_name);
            printf("(%s) strlen(d_name) = %ld\n", __func__, strlen(ep->d_name));
            uint16_t vid, pid, x, seq = 0;

            int ct = sscanf(ep->d_name, "%hX:%hX:%hX:%hX", &x, &vid, &pid, &seq);
            if (ct != 2)
               printf("(%s) sscanf failed, ct = %d\n", __func__, ct);
            else
               printf("(%s) sscanf ok, vid=0x%04x, pid=0x%04x\n", __func__, vid, pid);
            printf("(%s) x=0x%04x, vid=0x%04x, pid=0x%04x, seq=0x%04x\n", __func__,x,vid,pid,seq);
            // returns ct = 3, seq is unset,  why?
#endif

            bool is_monitor = is_hid_monitor_rdesc(fqfn);
            if (!is_monitor) {
               char * endptr;
               uint16_t vid = (uint16_t) strtoul(ep->d_name+5,  &endptr, 16);
               uint16_t pid = (uint16_t) strtoul(ep->d_name+10, &endptr, 16);
               is_monitor = force_hid_monitor_by_vid_pid(vid, pid);
            }
            if (is_monitor) {
               puts("");
               rpt_vstring(d1, "%s:", fqfn);
               rpt_file_contents(fqfn, d2);
            }
         }
      }
      closedir(dp);
   }

   DBGMSF(debug, "Done");
}

static void probe_hiddev(int depth) {
   int d1 = depth+1;
   int rc;

   // rpt_vstring(0, "Checking for USB HID devices using hiddev...");
   GPtrArray * hiddev_devices = get_hiddev_device_names();
   rpt_vstring(depth, "Found %d USB HID devices.", hiddev_devices->len);
   for (int devndx=0; devndx<hiddev_devices->len; devndx++) {
      puts("");
      errno=0;
      char * curfn = g_ptr_array_index(hiddev_devices,devndx);
      int fd = usb_open_hiddev_device(curfn, CALLOPT_RDONLY);    // do not emit error msg
      if (fd < 0) {      // fd is -errno
          rpt_vstring(depth, "Unable to open device %s: %s", curfn, strerror(errno));
      }
      else {
          char * cgname = get_hiddev_name(fd);
          struct hiddev_devinfo dev_info;
          errno = 0;
          rc = ioctl(fd, HIDIOCGDEVINFO, &dev_info);
          if (rc != 0) {
             rpt_vstring(d1, "Device %s, unable to retrieve information: %s",
                    curfn, strerror(errno));
          }
          else {
             char dev_summary[200];
             snprintf(dev_summary, 200,
                      "Device %s, devnum.busnum: %d.%d, vid:pid: %04x:%04x - %s",
                      curfn,
                      dev_info.busnum, dev_info.devnum,
                      dev_info.vendor, dev_info.product & 0xffff,
                      cgname);
             rpt_vstring(depth, "%s", dev_summary);
             bool b0 = is_hiddev_monitor(fd);
             if (b0)
                rpt_vstring(d1, "Identifies as a USB HID monitor");
             else
                rpt_vstring(d1, "Not a USB HID monitor");

             if (get_output_level() >= OL_VERBOSE) {
                if (!b0) {
                   b0 = force_hiddev_monitor(fd);
                   if (b0)
                      rpt_vstring(d1, "Device vid/pid matches exception list.  Forcing report for device.\n");
                }
                if (b0) {
                   char * simple_devname = strstr(curfn, "hiddev");
                   Udev_Usb_Devinfo * dinfo = get_udev_usb_devinfo("usbmisc", simple_devname);
                   if (dinfo) {
                      // report_hidraw_devinfo(dinfo, d2);
                      rpt_vstring(
                         d1,"Busno:Devno as reported by get_udev_usb_devinfo() for %s: %03d:%03d",
                            simple_devname, dinfo->busno, dinfo->devno);
                      free(dinfo);
                   }
                   else
                      rpt_vstring(d1, "Error getting busno:devno using get_udev_usb_devinfo()");

                   report_hiddev_device_by_fd(fd, d1);
                }
             }
          }

          close(fd);
      }
   }

   // need to set destroy function
   g_ptr_array_free(hiddev_devices, true);
}


/* Report information about USB connected monitors
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
static void query_usb_monitors() {
   printf("\nChecking for USB connected monitors...\n");

   Output_Level output_level = get_output_level();

   puts("");
   rpt_vstring(1, "Using lsusb to summarize USB devices...");
   execute_shell_cmd("lsusb|sort", 2);
   puts("");
   rpt_vstring(1, "USB device toplogy...");
   execute_shell_cmd("lsusb -t", 2);
   puts("");

   rpt_vstring(1, "Listing /dev/usb...");
   execute_shell_cmd("ls -l /dev/usb", 2);
   puts("");
   rpt_vstring(1, "Listing /dev/hiddev*...");
   execute_shell_cmd("ls -l /dev/hiddev*", 2);
   puts("");
   rpt_vstring(1, "Listing /dev/bus/usb...");
   execute_shell_cmd("ls -l /dev/bus/usb", 2);
   puts("");
   rpt_vstring(1, "Listing /dev/hidraw*...");
   execute_shell_cmd("ls -l /dev/hidraw*", 2);
   puts("");

   if (output_level >= OL_VERBOSE) {
      char * subsys_name = "usbmisc";
      printf("\nProbing USB HID devices using udev, susbsystem %s...\n", subsys_name);
      probe_udev_subsystem(subsys_name, /*show_usb_parent=*/ true, 1);
      subsys_name = "hidraw";
      printf("\nProbing USB HID devices using udev, susbsystem %s...\n", subsys_name);
      probe_udev_subsystem(subsys_name, /*show_usb_parent=*/ true, 1);
   }

   if (output_level >= OL_VERBOSE) {
      // currently an overwhelming amount of information - need to display
      // only possible HID connected monitors
      printf("\nProbing possible HID monitors using libusb...\n");
      probe_libusb(/*possible_monitors_only=*/ true, /*depth=*/ 1);

      printf("\nChecking for USB connected monitors on /dev/hidraw* ...\n");
      probe_hidraw(
            true,    // possible_monitors_only
            1);      // logical indentation depth

       // printf("\nProbing using hidapi...\n");
       // don't use.  wipes out /dev/hidraw  and /dev/usb/hiddev devices it opens
       // no addional information.   Feature values are returned as with libusb -
       // leading byte is report number
       // note that probe_hidapi() tests all possible report numbers, not just those
       // listed in the report descriptor.  Found some additional reports in the
       // vendor specific range on the Apple Cinema display
       // probe_hidapi(1);
   }

   puts("");
   rpt_vstring(0, "Checking for USB HID devices using hiddev...");
   probe_hiddev(1);

   puts("");
   rpt_vstring(0, "Checking for USB HID Report Descriptors in /sys/kernel/debug/hid...");
   probe_uhid(1);
}

#endif  // USE_USB


/* Master function to query the system environment
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
void query_sysenv() {
   query_base_env();

   printf("\n*** Primary Check 1: Identify video card and driver ***\n");
   struct driver_name_node * driver_list = query_card_and_driver_using_sysfs();

   printf("\n*** Primary Check 2: Check that /dev/i2c-* exist and writable ***\n");
   check_i2c_devices(driver_list);

   printf("\n*** Primary Check 3: Check that module i2c_dev is loaded ***\n");
   check_i2c_dev_module(driver_list);

   printf("\n*** Primary Check 4: Driver specific checks ***\n");
   driver_specific_tests(driver_list);

   struct driver_name_node * cur_node = driver_list;
   while (cur_node) {
      struct driver_name_node * next_node = cur_node->next;
      free(cur_node);
      cur_node = next_node;
   }

   printf("\n*** Primary Check 5: Installed packages ***\n");
   query_packages();
   puts("");

   printf("\n*** Additional probes ***\n");
   // printf("Gathering card and driver information...\n");
   puts("");
   query_proc_modules_for_video();
   puts("");
   query_card_and_driver_using_lspci();
   puts("");
   query_loaded_modules_using_sysfs();
   query_i2c_bus_using_sysfs();

   Output_Level output_level = get_output_level();
   if (output_level >= OL_VERBOSE) {
      puts("");
      query_proc_driver_nvidia();
   }

   if (output_level >= OL_VERBOSE) {
      query_i2c_buses();

      puts("");
      printf("xrandr connection report:\n");
      execute_shell_cmd("xrandr|grep connected", 1 /* depth */);
      puts("");

      query_using_i2cdetect();

      query_x11();

#ifdef USE_USB
      // probe_udev_subsystem() is in udev_util.c, which is only linked in if USE_USB
      char * subsys_name = "i2c-dev";
      printf("\nProbing I2C devices using udev, susbsystem %s...\n", subsys_name);
      probe_udev_subsystem(subsys_name, /*show_usb_parent=*/ false, 1);
#endif
   }


}


#ifdef USE_USB
/* Master function to query USB aspects of the system environment
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
void query_usbenv() {
   query_usb_monitors();
}
#endif
