/* @file query_sysenv_access.c
 *
 *  Checks on the the existence of and access to /dev/i2c devices
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/udev_i2c_util.h"

#include "base/linux_errno.h"
/** \endcond */

#include "i2c/i2c_bus_core.h"

#include "query_sysenv_access.h"


/** Perform redundant checks as cross-verification */
bool redundant_i2c_device_identification_checks = false;


//
// Get list of /dev/i2c devices
//
// There are too many ways of doing this throughout the code.
// Consolidate them here.  (IN PROGRESS)
//

/** Gets a list of all /dev/i2c devices by checking the file system
 *  if devices named /dev/i2c-N exist.
 *
 *  @return Byte_Value_Array containing the valid bus numbers
 */
static Byte_Value_Array get_i2c_devices_by_existence_test() {
   Byte_Value_Array bva = bva_create();
   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno)) {
         // if (!is_ignorable_i2c_device(busno))
         bva_append(bva, busno);
      }
   }
   return bva;
}


/** Gets a list of all /dev/i2c-n devices by screen-scraping the output
 *  of "ls /dev/i2c*".
 *
 *  @return Byte_Value_Array containing the valid bus numbers
 */
static Byte_Value_Array get_i2c_devices_by_ls() {
   Byte_Value_Array bva = bva_create();

   int ival;

   // returns array of I2C bus numbers in string form, sorted in numeric order
   GPtrArray * busnums = execute_shell_cmd_collect("ls /dev/i2c* | cut -c 10- | sort -n");

   if (!busnums) {
      rpt_vstring(1, "No I2C buses found");
      goto bye;
   }
   if (busnums->len > 0) {
      bool isint = str_to_int(g_ptr_array_index(busnums,0), &ival, 10);
      if (!isint) {
         rpt_vstring(1, "Apparently no I2C buses");
         goto bye;
      }
   }
   for (int ndx = 0; ndx < busnums->len; ndx++) {
      char * sval = g_ptr_array_index(busnums, ndx);
      bool isint = str_to_int(sval, &ival, 10);
      if (!isint) {
         rpt_vstring(1, "Parsing error.  Invalid I2C bus number: %s", sval);
      }
      else {
         bva_append(bva, ival);
         // is_smbus_device_using_sysfs(ival);
      }
   }
bye:
   if (busnums)
      g_ptr_array_free(busnums, true);

   return bva;
}


/** Consolidated function to identify I2C devices.
 *
 *  \return #ByteValueArray of bus numbers for detected I2C devices
 */
Byte_Value_Array identify_i2c_devices() {

   Byte_Value_Array i2c_device_numbers_result = NULL;   // result

   Byte_Value_Array bva1 = NULL;
   Byte_Value_Array bva2 = NULL;
   Byte_Value_Array bva3 = NULL;
   Byte_Value_Array bva4 = NULL;

   bva1 = get_i2c_devices_by_existence_test();
   if (redundant_i2c_device_identification_checks) {     // normally false, set true for testing
      bva2 = get_i2c_devices_by_ls();
      bva3 = get_i2c_device_numbers_using_udev(/* include_smbus= */ true);
      bva4 = get_i2c_device_numbers_using_udev_w_sysattr_name_filter(NULL);

      assert(bva_sorted_eq(bva1,bva2));
      assert(bva_sorted_eq(bva1,bva3));
      assert(bva_sorted_eq(bva1,bva4));
   }

   i2c_device_numbers_result = bva1;
   if (redundant_i2c_device_identification_checks) {
      bva_free(bva2);
      bva_free(bva3);
      bva_free(bva4);
   }
   // DBGMSG("Identified %d I2C devices", bva_length(bva1));
   return i2c_device_numbers_result;
}


/** Gets the username of the logged on user.
 *
 *  \return user name
 *
 *  The caller is responsible for freeing the returned string.
 */
static char * get_username(Env_Accumulator * accum) {
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
   bool ok = execute_shell_cmd_rpt(cmd, 0);
   printf("(%s) execute_shell_cmd() returned %s\n", __func__, bool_repr(ok));

#endif
   uid_t uid = getuid();
   // uid_t euid = geteuid();
   // printf("(%s) uid=%u, euid=%u\n", __func__, uid, euid);
   // gets logged on user name, user id, group id
   struct passwd *  pwd = getpwuid(uid);
   rpt_vstring(0,"Current user: %s (%u)", pwd->pw_name, uid);
   rpt_nl();
   char * uname = strdup(pwd->pw_name);

   accum->cur_uname = uname;
   accum->cur_uid = uid;

   return uname;
}


/** Checks which /dev/i2c devices are readable and writable.
 *
 *  \param  accum  accumulates environment information
 *
 *  \remark
 *  Sets the following fields in **Env_Accumulator**:
 *  - cur_user_all_devi2c_rw
 *  - cur_user_any_devi2c_rw
 *  - dev.i2c_common_group_name
 *  - any_dev_i2c_is_group_rw
 *  - all_dev_i2c_is_group_rw
 */
static void check_dev_i2c_access(Env_Accumulator * accum) {
   bool debug = false;
   DBGMSF(debug, "Starting");

   // bool all_i2c_rw = false;
   int busct = bva_length(accum->dev_i2c_device_numbers);
   int busct0 = i2c_device_count();   // simple count, no side effects, consider replacing with local code
   assert(busct == busct0);
   if (busct == 0 &&   !accum->dev_i2c_devices_required) {
      rpt_vstring(0,"WARNING: No /dev/i2c-* devices found");
   }
   else {
      // all_i2c_rw = true;
      int  busno;
      char fnbuf[20];

      for (int ndx = 0; ndx < busct; ndx++) {
         busno = bva_get(accum->dev_i2c_device_numbers, ndx);
  //  for (busno=0; busno < 32; busno++) {
  //       if (i2c_device_exists(busno)) {
            snprintf(fnbuf, sizeof(fnbuf), "/dev/i2c-%d", busno);
            int rc;
            int errsv;
            DBGMSF(debug, "Calling access() for %s", fnbuf);
            rc = access(fnbuf, R_OK|W_OK);
            if (rc < 0) {
               errsv = errno;
               rpt_vstring(0,"Device %s is not readable and writable.  Error = %s",
                      fnbuf, linux_errno_desc(errsv) );
               // all_i2c_rw = false;
               accum->cur_user_all_devi2c_rw = false;
            }
            else
               accum->cur_user_any_devi2c_rw = true;

            struct stat fs;
            rc = stat(fnbuf, &fs);
            if (rc < 0) {
               errsv = errno;
               rpt_vstring(0,"Error getting group information for device %s.  Error = %s",
                      fnbuf, linux_errno_desc(errsv) );
            }
            else {
               bool cur_file_grp_rw =  ( fs.st_mode & S_IRGRP ) && (fs.st_mode & S_IWGRP)  ;
               struct group*  grp;
               errno = 0;
               grp = getgrgid(fs.st_gid);
               if (!grp) {
                  errsv = errno;
                  rpt_vstring(0,"Error getting group information for group %d.  Error = %s",
                         fs.st_gid, linux_errno_desc(errsv) );
               }
               else {
                  char * gr_name = grp->gr_name;
                  if (accum->dev_i2c_common_group_name) {
                     if (!streq(accum->dev_i2c_common_group_name, gr_name)) {
                        free(accum->dev_i2c_common_group_name);
                        accum->dev_i2c_common_group_name = strdup("MIXED");
                     }
                  }
                  else
                     accum->dev_i2c_common_group_name = strdup(gr_name);
                  if (streq(gr_name, "i2c"))
                     accum->any_dev_i2c_has_group_i2c = true;
                  else
                     accum->all_dev_i2c_has_group_i2c = false;

                  DBGMSF(debug, "file=%s, st_gid=%d, gr_name=%s, cur_file_grp_rw=%s",
                        fnbuf, fs.st_gid, gr_name, bool_repr(cur_file_grp_rw));

                  if (fs.st_gid != 0) {    // root group is special case
                     if (cur_file_grp_rw)
                        accum->any_dev_i2c_is_group_rw = true;
                     else
                        accum->all_dev_i2c_is_group_rw = false;
                  }
               }
 //           }
         }
      }

      rpt_nl();
      if (!accum->cur_user_all_devi2c_rw) {
         rpt_vstring(
               0,
               "WARNING: Current user (%s) does not have RW access to all /dev/i2c-* devices.",
               accum->cur_uname);
      }
      else
         rpt_vstring(0,"Current user (%s) has RW access to all /dev/i2c-* devices.",
               // username);
               accum->cur_uname);
   }

   DBGMSF(debug, "Done");
}


/** Checks for group i2c and whether the current user is a
 *  member of the group.
 *
 *  \param  accum  accumulates environment information
 *
 *  \remark
 *  Sets the following fields in **Env_Accumulator**:
 *      group_i2c_checked
 *      group_i2c_exists
 *      cur_user_in_group_i2c
 */
static void check_group_i2c(Env_Accumulator * accum, bool verbose) {
   bool debug = false;
   DBGMSF(debug, "Starting. verbose=%s", bool_repr(verbose));
   // verbose = true;

   if (verbose) {
      rpt_nl();
      rpt_vstring(0,"Checking for group i2c...");
   }

   accum->group_i2c_checked = true;
   accum->group_i2c_exists = false;   // avoid special value in gid_i2c
   // gid_t gid_i2c;
   struct group * pgi2c = getgrnam("i2c");
   if (pgi2c) {
      if (verbose)
         rpt_vstring(0,"   Group i2c exists");
      accum->group_i2c_exists = true;
      // gid_i2c = pgi2c->gr_gid;
      // DBGMSG("getgrnam returned gid=%d for group i2c", gid_i2c);
      // DBGMSG("getgrnam() reports members for group i2c: %s", *pgi2c->gr_mem);
      int ndx=0;
      char * curname;
      while ( (curname = pgi2c->gr_mem[ndx]) ) {
         rtrim_in_place(curname);
         // DBGMSG("member_names[%d] = |%s|", ndx, curname);
         if (streq(curname, accum->cur_uname)) {
            accum->cur_user_in_group_i2c = true;
         }
         ndx++;
      }
      if (verbose) {
         if (accum->cur_user_in_group_i2c) {
            rpt_vstring(1,"Current user %s is a member of group i2c", accum->cur_uname);
         }
         else if (streq(accum->cur_uname, "root")) {
            rpt_vstring(1, "Current user is root, membership in group i2c not needed");
         }
         else {
            rpt_vstring(1, "WARNING: Current user %s is NOT a member of group i2c", accum->cur_uname);
         }
      }
   }
   else {
      if (verbose)
         rpt_label(1,"Group i2c does not exist");
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

   DBGMSF(debug, "Done.");
}


static void check_udev() {
   // makedev not used on udev systems.
   // So look for makedev.d, but don't complain if it's not found
   if (directory_exists("/etc/makedev.d")) {
      rpt_nl();
      rpt_vstring(0,"Looking for makedev.d files that reference i2c:");
      execute_shell_cmd_rpt("grep -H i2c /etc/udev/makedev.d/*", 1);
   }

   rpt_nl();
   rpt_vstring(0, "Looking for udev rules files that reference i2c:");
   rpt_vstring(1, "Checking rules directory /lib/udev/rules.d:");
   execute_shell_cmd_rpt("grep -H i2c /lib/udev/rules.d/*rules ", 2);

   rpt_vstring(1, "Checking rules directory /run/udev/rules.d:");
   execute_shell_cmd_rpt("grep -H i2c /run/udev/rules.d/*rules ", 2);

   rpt_vstring(1, "Checking rules directory /etc/udev/rules.d:");
   execute_shell_cmd_rpt("grep -H i2c /etc/udev/rules.d/*rules ", 2);

}


/** Checks on the existence and accessibility of /dev/i2c devices.
 *
 *  \param accum   accumulates environment information
 *
 * Checks that user has RW access to all /dev/i2c devices.
 * Checks if group i2c exists and whether the current user is a member.
 * Checks for references to i2c in /etc/udev/makedev.d
 *
 * If the only driver in driver_list is fglrx, the tests are
 * skipped (or if verbose output, purely informational).
 *
 * TODO: ignore i2c smbus devices
 *
 *  \remark
 *  assumes drivers already detected, i.e. **accum->driver_list** already set
 *
 *  \remark
 *  Sets:
 *    accum->group_i2c_exists
 *    accum->cur_user_in_group_i2c
 *    accum->cur_user_any_devi2c_rw
 *    accum->cur_user_all_devi2c_rw
 */
void check_i2c_devices(Env_Accumulator * accum) {
   bool debug = false;
   DBGMSF(debug, "Starting");

   // Env_Accumulator values already set
   assert(accum->dev_i2c_device_numbers);

#ifdef UNNEEDED
   // defaults now set properly in Env_Environment allocation
   accum->dev_i2c_devices_required = true;
   accum->group_i2c_checked = false;
   accum->group_i2c_exists = false;
   accum->cur_user_in_group_i2c = false;
   accum->cur_user_any_devi2c_rw = false;
   accum->cur_user_all_devi2c_rw = true;  // i.e. none fail the test
   accum->any_dev_i2c_has_group_i2c = false;
   accum->all_dev_i2c_has_group_i2c = true;
   accum->any_dev_i2c_is_group_rw = false;
   accum->all_dev_i2c_is_group_rw = true;
#endif

   Driver_Name_Node * driver_list = accum->driver_list;
   get_username(accum);

   rpt_vstring(0,"Checking /dev/i2c-* devices...");
   DDCA_Output_Level output_level = get_output_level();

   bool just_fglrx = only_fglrx(driver_list);
   if (just_fglrx){
      accum->dev_i2c_devices_required = false;
      rpt_nl();
      rpt_vstring(0,"Apparently using only the AMD proprietary driver fglrx.");
      rpt_vstring(0,"Devices /dev/i2c-* are not required.");
      // TODO: delay leaving to properl set other variables
      if (output_level < DDCA_OL_VERBOSE)
         return;
      rpt_vstring(0, "/dev/i2c device detail is purely informational.");
   }

   rpt_nl();
   rpt_multiline(0,
          "Unless the system is using the AMD proprietary driver fglrx, devices /dev/i2c-*",
          "must exist and the logged on user must have read/write permission for those",
          "devices (or at least those devices associated with monitors).",
          "",
          "Typically, this access is enabled by:",
          "  - setting the group for /dev/i2c-* to i2c",
          "  - setting group RW permissions for /dev/i2c-*",
          "  - making the current user a member of group i2c",
          "",
          "Alternatively, this can be enabled by just giving everyone RW permission",
          "The following tests probe for these conditions.",
          NULL
         );

   rpt_nl();
   rpt_vstring(0,"Checking for /dev/i2c-* devices...");
   execute_shell_cmd_rpt("ls -l /dev/i2c-*", 1);

   check_dev_i2c_access(accum);

   bool verbose = !accum->cur_user_all_devi2c_rw || output_level >= DDCA_OL_VERBOSE;
   check_group_i2c(accum, verbose);

   if (verbose) {
      check_udev();
   }

   DBGMSF(debug, "Done");
}


