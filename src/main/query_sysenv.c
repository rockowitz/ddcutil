/* query_sysenv.c
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


// #include <base/parms.h>    // put first for USE_LIBEXPLAIN

#include <config.h>

#define _GNU_SOURCE 1       // for function group_member

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>
#include <limits.h>
#include <sys/stat.h>
// #include <libosinfo-1.0/osinfo/osinfo.h>

#include "util/file_util.h"
#include "util/pci_id_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/msg_control.h"
#include "base/linux_errno.h"
#include "base/util.h"

#include "i2c/i2c_bus_core.h"
#include "adl/adl_shim.h"

#include "main/query_sysenv.h"



#ifndef MAX_PATH
#define MAX_PATH 256
#endif


char * known_video_driver_modules[] = {
      "fglrx",
      "nvidia",
      "nouveau",
      "radeon",
      "vboxvideo",
      NULL
};

char * prefix_matches[] = {
      "i2c",
      "video",
      NULL
};



char * read_sysfs_attr(char * dirname, char * attrname, bool verbose) {
   char fn[MAX_PATH];
   sprintf(fn, "%s/%s", dirname, attrname);
   return read_one_line_file(fn, verbose);
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


int query_proc_modules_for_video() {
   int rc = 0;

   GPtrArray * garray = g_ptr_array_sized_new(300);

   printf("Scanning /proc/modules for driver environment...\n");
   int ct = file_getlines("/proc/modules", garray);
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


/* Executes a shell command and writes the output to the terminal
 *
 */
bool execute_shell_cmd(char * shell_cmd, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. shell_cmd = |%s|", shell_cmd);
   bool ok = true;
   FILE * fp;
   char cmdbuf[200];
   snprintf(cmdbuf, sizeof(cmdbuf), "(%s) 2>&1", shell_cmd);
   // printf("(%s) cmdbuf=|%s|\n", __func__, cmdbuf);
   fp = popen(cmdbuf, "r");
   // printf("(%s) open. errno=%d\n", __func__, errno);
    if (!fp) {
       // int errsv = errno;
       printf("Unable to execute command \"%s\": %s\n", shell_cmd, strerror(errno));
       ok = false;
    }
    else {
       char * a_line = NULL;
       size_t len = 0;
       ssize_t read;
       bool first_line = true;
       while ( (read=getline(&a_line, &len, fp)) != -1) {
          if (strlen(a_line) > 0)
             a_line[strlen(a_line)-1] = '\0';
             if (first_line) {
                if (str_ends_with(a_line, "not found")) {
                   // printf("(%s) found \"not found\"\n", __func__);
                   ok = false;
                   break;
                }
                first_line = false;
             }
          rpt_title(a_line, depth);
          // fputs(a_line, stdout);
          // free(a_line);
       }
       int pclose_rc = pclose(fp);
       // printf("(%s) plose() rc = %d\n", __func__, pclose_rc);
    }
    return ok;
 }


void query_env() {
   bool debug = true;
   char username[32+1];       // per man useradd, max username length is 32

   printf("\nSystem information (uname):\n");
   // uname response:
   char * version_line = read_one_line_file("/proc/version", true /* verbose */);
   if (version_line)
      printf("   %s\n", version_line);
   else
      printf("   System information unavailable\n");

   printf("\nUnless the system is using AMD proprietary driver fglrx, devices /dev/i2c-*\n"
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

   getlogin_r(username, sizeof(username));
   // printf("\nLogged on user:  %s\n", username);


   // TODO: replace with actual code, eliminate call that violates inclusion ordering
   int busct = i2c_get_busct();
   int busno;
   char fnbuf[20];
   bool all_i2c_rw = true;
   for (busno=0; busno < busct; busno++) {
      snprintf(fnbuf, sizeof(fnbuf), "/dev/i2c-%d", busno);
      int rc;
      int errsv;
      rc = access(fnbuf, R_OK|W_OK);
      if (rc < 0) {
         errsv = errno;
         DBGMSF(debug, "Calling access() for %s", fnbuf);
         printf("Device %s is not readable and writable.  Error = %s\n",
                fnbuf, linux_errno_desc(errsv) );
         all_i2c_rw = false;
      }
   }

   if (!all_i2c_rw) {
      printf("\nWARNING: Current user (%s) does not have RW access to all /dev/i2c-* devices.\n", username);
   }
   else
      printf("\nCurrent user (%s) has RW access to all /dev/i2c-* devices.\n", username);




   printf("\nChecking for group i2c...\n");
   // replaced by C code
   // execute_shell_cmd("grep i2c /etc/group", 1);

   bool group_i2c_exists = false;   // avoid special value in gid_i2c
   gid_t gid_i2c;
   struct group * pgi2c = getgrnam("i2c");
   if (pgi2c) {
      printf("   Group i2c exists\n");
      group_i2c_exists = true;
      gid_i2c = pgi2c->gr_gid;
      // DBGMSG("getgrnam returned gid=%d for group i2c", gid_i2c);
      // DBGMSG("getgrnam() reports members for group i2c: %s", *pgi2c->gr_mem);
      int ndx=0;
      char * curname;
      bool found_curuser = false;
      while ( (curname = pgi2c->gr_mem[ndx]) ) {
         rtrim_in_place(curname);
         // DBGMSG("member_names[%d] = |%s|", ndx, curname);
         if (streq(curname, username)) {
            found_curuser = true;
         }
         ndx++;

      }
      if (found_curuser) {
         printf("   Current user %s is a member of group i2c\n", username);
      }
      else {
         printf("   WARNING: Current user %s is NOT a member of group i2c\n", username);

      }

#ifdef NO
      Null_Terminated_String_Array  member_names = strsplit(*pgi2c->gr_mem, ",");
      int ndx;
      for (ndx=0; ndx<null_terminated_string_array_length(member_names);ndx++){
         DBGMSG("member_names[%d] = |%s|", ndx, member_names[ndx]);
      }
      null_terminated_string_array_free(member_names);
#endif



   }
   if (!group_i2c_exists) {
      printf("   Group i2c does not exist\n");
   }
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


   printf("\nLooking for udev rules files that reference i2c:\n");
   execute_shell_cmd("grep i2c /etc/udev/makedev.d/*", 1);
   execute_shell_cmd("grep i2c "
                     "/lib/udev/rules.d/*rules "
                     "/lib/udev/rules.d/*rules "
                     "/etc/udev/rules.d/*rules", 1 );

#ifdef OLD
   printf("\nudev rules files in /lib/udev/rules.d referencing i2c:\n");
   execute_shell_cmd("grep i2c /lib/udev/rules.d/*rules");
   printf("\nudev rules files in /run/udev/rules.d referencing i2c:\n");
   execute_shell_cmd("grep i2c /run/udev/rules.d/*rules");
   printf("\nudev rules files in /etc/udev/rules.d referencing i2c:\n");
   execute_shell_cmd("grep i2c /etc/udev/rules.d/*rules");
#endif




   bool ok;
#ifdef NO
   // Produces lots of ugly output.   Really should be parsed.  Not worth it.


   // apt list doesn't exist on some systems, and apt show presents TMI
   printf("\nUsing apt to look for package i2c-tools...\n");
   // execute_shell_cmd("apt list i2c-tools");  // list command unavailable on Mint 17.3
   ok = execute_shell_cmd("apt show i2c-tools");
   if (!ok)
      printf("apt command not found\n");
#endif

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


   printf("\nCheck that kernel module i2c_dev is being loaded by examining files where this would be specified...\n");
   execute_shell_cmd("grep i2c[-_]dev "
                     "/etc/modules "
                     "/etc/modules-load.d/*conf "
                     "/etc/modprobe.d/*conf "
                     "/usr/lib/modules-load.d/*conf", 1);

}


bool query_card_and_driver_using_lspci() {
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


bool query_card_and_driver_using_sysfs() {
   bool debug = true;
   // DBGMSF(debug, "Starting");
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

   bool ok = true;
   char * driver_name = NULL;

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
               bool pci_ids_ok = init_pci_ids();
               if (pci_ids_ok) {
                  Pci_Id_Names names = pci_id_get_names(
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
                  if (names.subsys_name)
                  printf("   Subvendor/Subdevice: %04x/%04x  %s\n", xsubvendor_id, xsubdevice_id, names.subsys_name);
               }
               else {
                  printf("Unable to find pci.ids file for name lookup.\n");
                  printf("   Vendor:              %04x       \n", xvendor_id);
                  printf("   Device:              %04x       \n", xdevice_id);
                  printf("   Subvendor/Subdevice: %04x/%04x  \n", xsubvendor_id, xsubdevice_id);

               }
            }
         }
      }
      closedir(d);
   }

   if (driver_name && streq(driver_name, "nvidia")) {
      printf("\nChecking for special settings for proprietary Nvidia driver \n");
      printf("(needed for some newer Nvidia cards).\n");
      execute_shell_cmd("grep -i i2c /etc/X11/xorg.conf /etc/X11/xorg.conf.d/*", 1);
   }

   if (str_starts_with(driver_name, "fglrx")) {
#ifdef HAVE_ADL
      if (!adlshim_is_available()) {
         set_output_level(OL_VERBOSE);  // force error msg that names missing dll
         ok = adlshim_initialize();
         if (!ok)
            printf("WARNING: Using AMD proprietary video driver fglrx but unable to ADL library");
      }
#else
      printf("WARNING: Using AMD proprietary video driver fglrx but ddctool built without ADL support");
#endif
   }

   printf("\nExamining /sys/bus/i2c/devices...\n");
   d0 = "/sys/bus/i2c";
   d = opendir(d0);
   if (!d) {
      rpt_vstring(1, "i2c bus not defined in sysfs. Unable to open directory %s: %s\n", d0, strerror(errno));
   }
   else {
      d0 = "/sys/bus/i2c/devices";
      d = opendir(d0);
      if (!d) {
         rpt_vstring(1, "Unable to open sysfs directory %s: %s\n", d0, strerror(errno));
      }
      else {
         bool i2c_seen = false;
         while ((dent = readdir(d)) != NULL) {
            // DBGMSF("%s", dent->d_name);
            char cur_fn[100];
            char cur_dir_name[100];
            if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
               // DBGMSF(debug, "dent->dname: %s", dent->d_name);
               sprintf(cur_dir_name, "%s/%s", d0, dent->d_name);
               char * dev_name = read_sysfs_attr(cur_dir_name, "name", true);
               rpt_vstring(1, "%s/name: %s", cur_dir_name, dev_name);
               i2c_seen = true;
            }
         }
         if (!i2c_seen)
            rpt_vstring(1, "No i2c devices found in %s", d0);
      }
   }


   return ok;
}


bool query_card_and_driver_using_osinfo() {
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


void query_card_and_driver() {
   puts("");
   printf("Gathering card and driver information...\n");
   puts("");
   query_proc_modules_for_video();
   puts("");
   query_card_and_driver_using_lspci();
   puts("");
   query_card_and_driver_using_sysfs();
}


