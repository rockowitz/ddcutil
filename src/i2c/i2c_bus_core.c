/** @file i2c_bus_core.c
 *
 * I2C bus detection and inspection
 */
// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef USE_X11
// #include <X11/extensions/dpmsconst.h>
#endif
/** \endcond */

#include "util/coredefs_base.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/i2c_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#ifdef ENABLE_UDEV
#include "util/udev_i2c_util.h"
#endif
#include "util/utilrpt.h"
#ifdef USE_X11
#include "util/x11_util.h"
#endif

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif
#include "i2c/i2c_display_lock.h"
#include "i2c/i2c_dpms.h"
#include "i2c/i2c_strategy_dispatcher.h"
#include "i2c/i2c_sysfs.h"
#include "i2c/i2c_execute.h"
#include "i2c/i2c_edid.h"

#include "i2c/i2c_bus_core.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

bool i2c_force_bus = false;  // Another ugly global variable for testing purposes
bool drm_enabled = false;
bool force_read_edid = true;
int  i2c_businfo_async_threshold = DEFAULT_BUS_CHECK_ASYNC_THRESHOLD;
bool cross_instance_locks_enabled = DEFAULT_ENABLE_FLOCK;
int  flock_poll_millisec = DEFAULT_FLOCK_POLL_MILLISEC;
int  flock_max_wait_millisec = DEFAULT_FLOCK_MAX_WAIT_MILLISEC;


void i2c_enable_cross_instance_locks(bool yesno) {
   bool debug = false;
   cross_instance_locks_enabled = yesno;
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "yesno = %s", SBOOL(yesno));
}


/** Gets a list of all /dev/i2c devices by checking the file system
 *  if devices named /dev/i2c-N exist.
 *
 *  @return Byte_Value_Array containing the valid bus numbers
 */
Byte_Value_Array get_i2c_devices_by_existence_test(bool include_ignorable_devices) {
   Byte_Value_Array bva = bva_create();
   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno)) {
         if (include_ignorable_devices || !sysfs_is_ignorable_i2c_device(busno))
            bva_append(bva, busno);
      }
   }
   return bva;
}


//
// Bus open and close
//

static GMutex  open_failures_mutex;
static Bit_Set_256 open_failures_reported;


/** Adds a set of bus numbers to the set of bus numbers
 *  whose open failure has already been reported.
 *
 *  @param failures   set of bus numbers
 */
void add_open_failures_reported(Bit_Set_256 failures) {
   g_mutex_lock(&open_failures_mutex);
   open_failures_reported = bs256_or(open_failures_reported, failures);
   g_mutex_unlock(&open_failures_mutex);
}


/** Adds a single bus number to the set of open failures already reported.
 *
 *  @param  busno     /dev/i2c-N bus number
 */
void include_open_failures_reported(int busno) {
   g_mutex_lock(&open_failures_mutex);
   open_failures_reported = bs256_insert(open_failures_reported, busno);
   g_mutex_unlock(&open_failures_mutex);
}


/** Open an I2C bus device.
 *
 *  @param busno     bus number
 *  @param callopts  call option flags, controlling failure action
 *
 *  @retval >=0     Linux file descriptor
 *  @retval -errno  negative Linux errno if open fails
 *
 *  Call options recognized
 *  - CALLOPT_WAIT
 */
Error_Info * i2c_open_bus(int busno, Byte callopts, int* fd_loc) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, callopts=0x%02x=%s",
         busno, callopts, interpret_call_options_t(callopts));

   char filename[20];
   Error_Info * master_error = NULL;
   assert(fd_loc);
   *fd_loc = -1;   // ?

   Display_Lock_Flags ddisp_flags = DDISP_WAIT;
   DDCA_IO_Path dpath;
   dpath.io_mode = DDCA_IO_I2C;
   dpath.path.i2c_busno = busno;
   master_error = lock_display_by_dpath(dpath, ddisp_flags);
   if (master_error) {
      goto bye;
   }

   int fd = -1;
   snprintf(filename, 19, "/dev/"I2C"-%d", busno);
   RECORD_IO_EVENT(
         -1,
         IE_OPEN,
         ( fd = open(filename, (callopts & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR) )
         );
   // if successful returns file descriptor, if fail, returns -1 and errno is set
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "open(%s) returned %d", filename, fd);

   if (fd < 0) {
      master_error = ERRINFO_NEW(-errno, "Open failed for %s", filename);
      Error_Info * err = unlock_display_by_dpath(dpath);
      // only error returned is DDCRC_LOCKED, which is impossible in this case
      assert(!err);    // avoid coverity warning
      goto bye;
   }

   if (cross_instance_locks_enabled) {
      int operation = LOCK_EX|LOCK_NB;
      int poll_microsec = flock_poll_millisec * 1000;
      uint64_t max_wait_millisec = (callopts & CALLOPT_WAIT) ? flock_max_wait_millisec : 0;
      uint64_t max_nanos = cur_realtime_nanosec() + (max_wait_millisec * 1000 * 1000);
      DBGTRC(debug, DDCA_TRC_NONE, "flock_poll_millisec=%jd, flock_max_wait_millisec=%jd, max_wait_millisec=%jd",
            flock_poll_millisec, flock_max_wait_millisec, max_wait_millisec);
      Status_Errno lockrc = 0;
      int flock_call_ct = 0;
      while(true) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling flock(%d,0x%04x)...", fd, operation);
         flock_call_ct++;
         int flockrc = flock(fd, operation);
         if (flockrc == 0)  {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "flock succeeded");
#ifdef EXPLORING
            int inode = get_inode_by_fd(fd);
            intmax_t pid = get_process_id();
            DBGMSG("pid=%jd filename = %s, inode=%d", pid, filename, inode);
            execute_shell_cmd_rpt("lslocks|grep /dev/i2c", 1);
            char cmd[80];
            // g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d", inode);
            // execute_shell_cmd_rpt(cmd, 1);
            g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d | cut -d' ' -f'1'", inode);
            execute_shell_cmd_rpt(cmd, 1);
            GPtrArray * pids_locking_inode = execute_shell_cmd_collect(cmd);
            rpt_vstring(1, "Processing locking inode %jd:", inode);
            for (int ndx = 0; ndx < pids_locking_inode->len; ndx++) {
               rpt_vstring(2, "%s", g_ptr_array_index(pids_locking_inode, ndx));
            }

            // g_snprintf(cmd, 80, "ls /proc/%jd", pid);
            // execute_shell_cmd_rpt(cmd, 1);
            // g_snprintf(cmd, 80, "cat /proc/%jd/cmdline", pid);
            // execute_shell_cmd_rpt(cmd, 1);
            g_snprintf(cmd, 80, "cat /proc/%jd/status | egrep -e Name -e State -e '^Pid:'", pid);
            execute_shell_cmd_rpt(cmd, 1);
            GPtrArray * pids = execute_shell_cmd_collect(cmd);
            for (int ndx = 0; ndx < pids->len; ndx++) {
               rpt_vstring(3, "%s", g_ptr_array_index(pids, ndx));
            }
#endif
            break;
         }
         assert(flockrc == -1);
         int errsv = errno;
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "busno=%d, flock() returned: %s", busno, psc_desc(-errsv));
         if (errsv == EWOULDBLOCK ) {          // n. EWOULDBLOCK == EAGAIN
           uint64_t now = cur_realtime_nanosec();
           if (now < max_nanos) {
              // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Resource locked. Sleeping");
              if (flock_call_ct == 1)
                 MSG_W_SYSLOG(DDCA_SYSLOG_NOTICE, "%s locked.  Retrying...", filename);
              usleep(poll_microsec);
              continue;
           }
           else {
              MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Max wait exceeded for %s", filename);
              if (IS_DBGTRC(true, DDCA_TRC_NONE)) {
                 char cmd[80];

                 MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Programs holding %s open:", filename);
                 rpt_lsof(filename, 1);
                 g_snprintf(cmd, 80, "lsof %s", filename);
                 GPtrArray* lsof_lines = execute_shell_cmd_collect(cmd);
                 for (int ndx = 0; ndx < lsof_lines->len; ndx++) {
                    MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "   %s", (char*) g_ptr_array_index(lsof_lines, ndx));
                 }
                 g_ptr_array_free(lsof_lines, true);

                 int inode = get_inode_by_fn(filename);
                 // int inode2 = get_inode_by_fd(fd);
                 // assert(inode == inode2);

                 MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "Processes locking %s (inode %d): ", filename, inode);
                 g_snprintf(cmd, 80, "cat /proc/locks | cut -d' ' -f'7 8' | grep 00:05:%d | cut -d' ' -f'1'", inode);
                 execute_shell_cmd_rpt(cmd, 1);  // *** TEMP ***
                 GPtrArray * pids = execute_shell_cmd_collect(cmd);
                 // rpt_vstring(1, "Processes locking inode %jd", inode);
                 for (int ndx = 0; ndx < pids->len; ndx++) {
                    char * spid = g_ptr_array_index(pids, ndx);
                    rpt_vstring(2, "%s", spid);  // *** TEMP ***
                    g_snprintf(cmd, 80, "cat /proc/%s/status | egrep -e Name -e State -e '^Pid:'", spid);
                    execute_shell_cmd_rpt(cmd, 1); // *** TEMP ***
                    GPtrArray * status_lines = execute_shell_cmd_collect(cmd);
                    for (int k = 0; k < status_lines->len; k ++) {
                       MSG_W_SYSLOG(DDCA_SYSLOG_WARNING, "   %s", (char*) g_ptr_array_index(status_lines, k));
                    }
                    rpt_nl();
                    g_ptr_array_free(status_lines, true);
                 }
              }
              lockrc = DDCRC_FLOCKED;
              break;
           }
        }
        else {
            DBGTRC_NOPREFIX(true, TRACE_GROUP, "Unexpected error from flock() for %s: %s",
                  filename, psc_desc(-errsv));
            lockrc = -errsv;
            break;
        }
     }
      if (lockrc != 0) {
         DBGTRC_NOPREFIX(true, TRACE_GROUP, "Cross instance locking failed");
         close(fd);
         unlock_display_by_dpath(dpath);
         master_error = ERRINFO_NEW(lockrc, "Cross instance locking failed. busno=%d", busno);
      }
   }

bye:
   if (master_error) {
      // DBGTRC_RET_DDCRC(true, TRACE_GROUP, fd, "busno=%d", busno);
   }
   else {
      *fd_loc = fd;
      // DBGTRC_DONE(debug, TRACE_GROUP, "busno=%d, Returning file descriptor: %d", busno, fd);
   }

   ASSERT_IFF(master_error, *fd_loc == -1);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, master_error, "busno=%d, Set file descriptor *fd_loc = %d", busno, *fd_loc);
   return master_error;
}


/** Closes an open I2C bus device.
 *
 * @param  busno     i2c_bus_number
 * @param  fd        Linux file descriptor
 * @param  callopts  call option flags, controlling failure action
 *
 * @retval 0  success
 * @retval <0 negative Linux errno value if close fails
 */
Status_Errno i2c_close_bus(int busno, int fd, Call_Options callopts) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
          "busno=%d, fd=%d - %s, callopts=%s",
          busno, fd, filename_for_fd_t(fd), interpret_call_options_t(callopts));

   Status_Errno result = 0;
   int rc = 0;

   if (cross_instance_locks_enabled) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Calling flock(%d,LOCK_UN)...", fd);
      int rc = flock(fd, LOCK_UN);
      if (rc < 0) {
         int errsv = errno;
         DBGTRC_NOPREFIX(true, TRACE_GROUP, "Unexpected error from flock(..,LOCK_UN): %s",
               psc_desc(-errsv));
      }
   }
   DDCA_IO_Path dpath;
   dpath.io_mode = DDCA_IO_I2C;
   dpath.path.i2c_busno = busno;
   Error_Info * erec = unlock_display_by_dpath(dpath);
   if (erec) {
      char * s = g_strdup_printf("Unexpected error %s from unlock_display_by_dpath(%s)",
            psc_name(erec->status_code), dpath_repr_t(&dpath));
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "%s", s);
      SYSLOG2(DDCA_SYSLOG_ERROR, "%s", s);
      free(s);
      errinfo_free(erec);
   }

   RECORD_IO_EVENT(fd, IE_CLOSE, ( rc = close(fd) ) );
   assert( rc == 0 || rc == -1);   // per documentation
   int errsv = errno;
   if (rc < 0) {
      // EBADF (9)  fd isn't a valid open file descriptor
      // EINTR (4)  close() interrupted by a signal
      // EIO   (5)  I/O error
      if (callopts & CALLOPT_ERR_MSG)
         f0printf(ferr(), "Close failed for %s, errno=%s\n",
                          filename_for_fd_t(fd), linux_errno_desc(errsv));
      result = -errsv;
   }
   assert(result <= 0);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "fd=%d",fd);
   return result;
}



//
// I2C Bus Inspection - Slave Addresses
//

#ifdef UNUSED
#define IS_EDP_DEVICE(_busno) is_laptop_drm_connector(_busno, "-eDP-")
#define IS_LVDS_DEVICE(_busno) is_laptop_drm_connector(_busno, "-LVDS-")


/** Checks whether a /dev/i2c-n device represents an eDP or LVDS device,
 *  i.e. a laptop display.
 *
 *  @param  busno   i2c bus number
 *  #param  drm_name_fragment  string to look for
 *  @return true/false
 *
 *  @remark Works only for DRM displays, therefore use only
 *          informationally.
 */
// Attempting to recode using opendir() and readdir() produced
// a complicated mess.  Using execute_shell_cmd_collect() is simple.
// Simplicity has its virtues.
static bool is_laptop_drm_connector(int busno, char * drm_name_fragment) {
   bool debug = false;
   // DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, drm_name_fragment=|%s|", busno, drm_name_fragment);
   bool result = false;

   char cmd[100];
   snprintf(cmd, 100, "ls -d /sys/class/drm/card*/card*/"I2C"-%d", busno);
   // DBGMSG("cmd: %s", cmd);

   GPtrArray * lines = execute_shell_cmd_collect(cmd);
   if (lines)  {    // command should never fail, but just in case
      for (int ndx = 0; ndx < lines->len; ndx++) {
         char * s = g_ptr_array_index(lines, ndx);
         if (strstr(s, drm_name_fragment)) {
            result = true;
            break;
         }
      }
      g_ptr_array_free(lines, true);
   }

   DBGTRC_EXECUTED(debug, TRACE_GROUP, "busno=%d, drm_name_fragment |%s|, Returning: %s",
                              busno, drm_name_fragment, sbool(result));
   return result;
}

#endif


bool is_laptop_drm_connector_name(const char * connector_name) {
   bool debug = false;
   bool result = strstr(connector_name, "-eDP-") ||
                 strstr(connector_name, "-LVDS-");
   DBGF(debug, "connector_name=|%s|, returning %s", connector_name, sbool(result));
   return result;
}


//
// Check display status
//

bool i2c_check_edid_exists_by_dh(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dh = %s", dh_repr(dh));

   Buffer * edidbuf = buffer_new(256, "");
   Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(dh->fd, edidbuf);
   bool result = (rc == 0);
   buffer_free(edidbuf, "");

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


bool i2c_check_edid_exists_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno = %d", businfo->busno);
   bool result = false;
   int fd = -1;
   Error_Info * erec = i2c_open_bus(businfo->busno, CALLOPT_ERR_MSG, &fd);
   if (!erec) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened bus /dev/i2c-%d", businfo->busno);
      Buffer * edidbuf = buffer_new(256, "");
      Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(fd, edidbuf);
      if (rc == 0)
         result = true;
      buffer_free(edidbuf, "");
      i2c_close_bus(businfo->busno,fd, CALLOPT_ERR_MSG);
    }
   else
      ERRINFO_FREE(erec);
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


#ifdef OUT
// *** wrong for Nvidia driver ***
Error_Info * i2c_check_bus_responsive_using_drm(const char * drm_connector_name) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "drm_connector_name = %s", drm_connector_name);
   assert(sys_drm_connectors);
   assert(drm_connector_name);

   Error_Info * result = NULL;
   char * status;
   RPT_ATTR_TEXT(-1, &status, "/sys/class/drm", drm_connector_name, "status");
   if (streq(status, "disconnected"))   // *** WRONG Nvidia driver always reports "disconnected"
         result = ERRINFO_NEW(DDCRC_DISCONNECTED, "Display was disconnected");
   else {
      char * dpms;
      RPT_ATTR_TEXT(-1, &dpms, "/sys/class/drm", drm_connector_name, "dpms");
      if ( !streq(dpms, "On"))
         result = ERRINFO_NEW(DDCRC_DPMS_ASLEEP, "Display is in a DPMS sleep mode");
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "");
   return result;
}
#endif


/**
 *
 *  @param  dh     display handle
 *  @retval NULL   ok
 *  @retval Error_Info with status DDCRC_DISCONNECTED or DDCRC_DPMS_ASLEEP
 */

Error_Info * i2c_check_open_bus_alive(Display_Handle * dh) {
   bool debug = false;
   assert(dh->dref->io_path.io_mode == DDCA_IO_I2C);
   I2C_Bus_Info * businfo = dh->dref->detail;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, businfo=%p", businfo->busno, businfo );
   assert(businfo && ( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) );
   assert( (businfo->flags & I2C_BUS_EXISTS) &&
           (businfo->flags & I2C_BUS_VALID_NAME_CHECKED) &&
           (businfo->flags & I2C_BUS_HAS_VALID_NAME) &&
           (businfo->flags & I2C_BUS_PROBED)
         );
   assert(sys_drm_connectors);

   Error_Info * result = NULL;
   bool edid_exists = false;
   if (businfo->drm_connector_name) {
      edid_exists = GET_ATTR_EDID(NULL, "/sys/class/drm/", businfo->drm_connector_name, "edid");
      // edid_exists = i2c_check_bus_responsive_using_drm(businfo->drm_connector_name);  // fails for Nvidia
   }
   else {
      // read edid
      edid_exists = i2c_check_edid_exists_by_dh(dh);
   }
   if (!edid_exists) {
      result = ERRINFO_NEW(DDCRC_DISCONNECTED,
               "/dev/i2c-%d", dh->dref->io_path.path.i2c_busno);
   }
   else {
      if (dpms_check_drm_asleep_by_dref(dh->dref))
         result = ERRINFO_NEW(DDCRC_DPMS_ASLEEP,
               "/dev/i2c-%d", dh->dref->io_path.path.i2c_busno);
   }
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "");
   return result;
}


#ifdef UNUSED
Bit_Set_256 check_edids(GPtrArray * buses) {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "buses=%p, len=%d", buses, buses->len);
   Bit_Set_256 result = EMPTY_BIT_SET_256;
   for (int ndx = 0; ndx < buses->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(buses, ndx);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_open_bus..");
      bool ok = i2c_check_edid_exists_by_businfo(businfo);
      if (ok)
         bs256_insert(result, businfo->busno);
   }
   DBGTRC_RETURNING(debug, TRACE_GROUP, "%s", bs256_to_string(result, "", ", "));
   return result;
}
#endif


//
// I2C Bus Inspection - Fill in and report Bus_Info
//

static Status_Errno_DDC
i2c_detect_x37(int fd) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d - %s", fd, filename_for_fd_t(fd) );

   // Quirks
   // - i2c_set_addr() Causes screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   //   avoided by never calling this function for an eDP screen
   // - Dell P2715Q does not respond to single byte read, but does respond to
   //   a write (7/2018), so this function checks both
   Status_Errno_DDC rc = 0;
   // regard either a successful write() or a read() as indication slave address is valid
   Byte    writebuf = {0x00};

   rc = invoke_i2c_writer(fd, 0x37, 1, &writebuf);
   // rc = i2c_ioctl_writer(fd, 0x37, 1, &writebuf);
   // rc = 6; // for testing
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_writer() for slave address x37 returned %s", psc_name_code(rc));
   if (rc != 0) {
      Byte    readbuf[4];  //  4 byte buffer
      rc = invoke_i2c_reader(fd, 0x37, false, 4, readbuf);
      //rc = i2c_ioctl_reader(fd, 0x37, false, 4, readbuf);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                      "invoke_i2c_reader() for slave address x37 returned %s", psc_name_code(rc));
   }
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc,"");
   return rc;
}


/** Inspects an I2C bus.
 *
 *  Takes the number of the bus to be inspected from the #I2C_Bus_Info struct passed
 *  as an argument.
 *
 *  @param  bus_info  pointer to #I2C_Bus_Info struct in which information will be set
 */
void i2c_check_bus(I2C_Bus_Info * bus_info) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, bus_info=%p", bus_info->busno, bus_info );
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "force_read_edid=%s", sbool(force_read_edid));
   assert(bus_info && ( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0) );
   assert( (bus_info->flags & I2C_BUS_EXISTS) &&
           (bus_info->flags & I2C_BUS_VALID_NAME_CHECKED) &&
           (bus_info->flags & I2C_BUS_HAS_VALID_NAME)
         );
   assert(sys_drm_connectors);

   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Probing");
      bus_info->flags |= I2C_BUS_PROBED;
      bus_info->driver = get_driver_for_busno(bus_info->busno);
      char * connector = get_drm_connector_name_by_busno(bus_info->busno);
      bus_info->flags |= I2C_BUS_DRM_CONNECTOR_CHECKED;
      // connector = NULL;   // *** TEST ***
      if (connector) {
         bus_info->drm_connector_name = connector;
         bus_info->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
         if ( is_laptop_drm_connector_name(connector))
            bus_info->flags |= I2C_BUS_LVDS_OR_EDP;

         if (!force_read_edid) {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                          "Getting edid from sysfs for connector %s", bus_info->drm_connector_name);
            GByteArray*  edid_bytes = NULL;
            // int d = IS_DBGTRC(debug, TRACE_GROUP) ? 1 : -1;
            int d = -1;
            RPT_ATTR_EDID(d, &edid_bytes, "/sys/class/drm", bus_info->drm_connector_name, "edid");
            if (edid_bytes && edid_bytes->len >= 128) {
               DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Got edid from sysfs");
               bus_info->edid = create_parsed_edid2(edid_bytes->data, "SYSFS");
               if (debug) {
                  if (bus_info->edid)
                     report_parsed_edid(bus_info->edid, false /* verbose */, 0);
                  else
                     DBGMSG("create_parsed_edid() failed");
               }
               if (bus_info->edid) {
                  bus_info->flags |= I2C_BUS_ADDR_0X50;
                  bus_info->flags |= I2C_BUS_SYSFS_EDID;
                  // memcpy(bus_info->edid->edid_source, "SYSFS", 6); // redundant
               }
            }
            if (edid_bytes)
               g_byte_array_free(edid_bytes,true);
         }
      }

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_open_bus..");
      int fd = -1;
      Error_Info *err = i2c_open_bus(bus_info->busno, CALLOPT_WAIT, &fd);
      if (fd < 0) {
         bus_info->open_errno = err->status_code;
         ERRINFO_FREE(err);
      }
      else {    //open succeeded
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened bus /dev/i2c-%d", bus_info->busno);
          bus_info->flags |= I2C_BUS_ACCESSIBLE;
          bus_info->functionality = i2c_get_functionality_flags_by_fd(fd);
#ifdef TEST_EDID_SMBUS
          if (EDID_Read_Uses_Smbus) {
             // for the smbus hack
             assert(bus_info->functionality & I2C_FUNC_SMBUS_READ_BYTE_DATA);
          }
#endif

          if (!bus_info->edid) {
             DDCA_Status ddcrc = i2c_get_parsed_edid_by_fd(fd, &bus_info->edid);
#ifdef TEST
             if (!result) {
                if (bus_info->busno == 6 || bus_info->busno == 8) {
                   result = -EBUSY;
                   bus_info->edid = NULL;
                   DBGMSG("Forcing -EBUSY on get_parsed_edid_by_fd()");
                }
             }
#endif
             DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, i2c_get_parsed_edid_by_fd() returned %s",
                   bus_info->busno, psc_desc(ddcrc));
             if (ddcrc != 0) {
                bus_info->open_errno =  ddcrc;
             }
             else {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, already have EDID", bus_info->busno);
                bus_info->flags |= I2C_BUS_ADDR_0X50;

                if (!bus_info->drm_connector_name &&    // if not already checked for laptop
                    is_laptop_parsed_edid(bus_info->edid) )
                {
                      bus_info->flags |= I2C_BUS_APPARENT_LAPTOP;
                }
             }
          }

          if (bus_info->flags & (I2C_BUS_LVDS_OR_EDP)) {
             DBGTRC(debug, TRACE_GROUP, "Laptop display detected, not checking x37");
          }
          else {  // start, x37 check
             // The check here for slave address x37 had previously been removed.
             // It was commented out in commit 78fb4b on 4/29/2013, and the code
             // finally delete by commit f12d7a on 3/20/2020, with the following
             // comments:
             //    have seen case where laptop display reports addr 37 active, but
             //    it doesn't respond to DDC
             // 8/2017: If DDC turned off on U3011 monitor, addr x37 still detected
             // DDC checking was therefore moved entirely to the DDC layer.
             // 6/25/2023:
             // Testing for slave address x37 turns out to be needed to avoid
             // trying to reload cached display information for a display no
             // longer present
             int rc = i2c_detect_x37(fd);
#ifdef TEST
             if (rc == 0) {
                if (bus_info->busno == 6 || bus_info->busno == 8) {
                     rc = -EBUSY;
                     DBGMSG("Forcing -EBUSY on i2c_detect_37()");
                }
             }
#endif
             if (rc == 0)
                bus_info->flags |= I2C_BUS_ADDR_0X37;
             // else if (rc == -EBUSY)
             //    bus_info->flags |= I2C_BUS_BUSY;
          }    // end x37 check

          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Closing bus...");
          i2c_close_bus(bus_info->busno, fd, CALLOPT_ERR_MSG);
      }

      // conformant driver, so drm_connector_name set, but reading EDID failed,
      // probably because of EBUSY.  Get the EDID so we have it for messages.

      // Not all drivers provide for getting the bus number directly using
      // /sys/bus/drm.  If the connector name is not yet set but reading
      // the EDID was successful, find the connector name by EDID
      if (!bus_info->drm_connector_name && bus_info->edid) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Finding connector by EDID...");
         char * connector = get_drm_connector_name_by_edid(bus_info->edid->bytes);  // NULL if not drm driver
         if (connector) {
            bus_info->drm_connector_name = connector;
            bus_info->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_EDID;
         }
      }

      bus_info->last_checked_dpms_asleep = dpms_check_drm_asleep_by_businfo(bus_info);
   }   // probing complete

   if ( IS_DBGTRC(debug, TRACE_GROUP)) {
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "flags = %s", i2c_interpret_bus_flags_t(bus_info->flags));

      // DBGTRC_NOPREFIX(true, TRACE_GROUP, "bus_info:");
      // i2c_dbgrpt_bus_info(bus_info, 2);
      DBGTRC_DONE(true, TRACE_GROUP, "");
   }
}


STATIC void *
threaded_initial_checks_by_businfo(gpointer data) {
   bool debug = false;

   I2C_Bus_Info * businfo = data;
   TRACED_ASSERT(memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = /dev/i2c-%d", businfo->busno );

   i2c_check_bus(businfo);
   // g_thread_exit(NULL);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning NULL. bus=/dev/i2c-%d", businfo->busno );
   return NULL;
}


/** Spawns threads to perform initial checks and waits for them all to complete.
 *
 *  @param all_displays #GPtrArray of pointers to #I2c_Bus_Info
 */
STATIC void
i2c_async_scan(GPtrArray * i2c_buses) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "i2c_buses=%p, bus count=%d",
                                       i2c_buses, i2c_buses->len);

   GPtrArray * threads = g_ptr_array_new();
   for (int ndx = 0; ndx < i2c_buses->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(i2c_buses, ndx);
      TRACED_ASSERT( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0 );

      char buf[16];
      g_snprintf(buf, 16, "/dev/i2c-%d", businfo->busno);
      GThread * th =
      g_thread_new(
            buf,                // thread name
            threaded_initial_checks_by_businfo,
            businfo);                            // pass pointer to display ref as data
      g_ptr_array_add(threads, th);
   }
   DBGMSF(debug, "Started %d threads", threads->len);
   for (int ndx = 0; ndx < threads->len; ndx++) {
      GThread * thread = g_ptr_array_index(threads, ndx);
      g_thread_join(thread);  // implicitly unrefs the GThread
   }
   DBGMSF(debug, "Threads joined");
   g_ptr_array_free(threads, true);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Loops through a list of I2C_Bus_Info, performing initial checks on each.
 *
 *  @param i2c_buses #GPtrArray of pointers to #I2C_Bus_Info
 */
void
i2c_non_async_scan(GPtrArray * i2c_buses) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "checking %d buses", i2c_buses->len);

   for (int ndx = 0; ndx < i2c_buses->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(i2c_buses, ndx);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_check_bus() synchronously for bus %d", businfo->busno);
      i2c_check_bus(businfo);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}

// Bit_Set_256 attached_buses;

Byte_Value_Array i2c_detect_attached_buses() {
   bool debug = false;
#ifdef ENABLE_UDEV
   // do not include devices with ignorable name, etc.:
   Byte_Value_Array i2c_bus_bva =
            get_i2c_device_numbers_using_udev(/*include_ignorable_devices=*/ false);
#else
   Byte_Value_Array i2c_bus_bva =
            get_i2c_devices_by_existence_test(/*include_ignorable_devices=*/ false);
#endif
   if (IS_DBGTRC(debug, TRACE_GROUP)) {
      char * s = bva_as_string(i2c_bus_bva,  false,  ", ");
      DBGTRC_EXECUTED(true, DDCA_TRC_NONE, "possible i2c device bus numbers: %s", s);
      free(s);
   }
   return i2c_bus_bva;;
}


Bit_Set_256 i2c_detect_attached_buses_as_bitset() {
   Byte_Value_Array bva = i2c_detect_attached_buses();
   Bit_Set_256  cur_buses = bs256_from_bva(bva);
   bva_free(bva);
   return cur_buses;
}


#ifdef UNUSED
void i2c_check_attached_buses() {
   Bit_Set_256 cur_attached_buses = i2c_detect_attached_buses();
   if (!bs256_eq(cur_attached_buses, attached_buses)) {   // will be rare
      Bit_Set_256 newly_attached_buses = bs256_and_not(cur_attached_buses, attached_buses);
      Bit_Set_256 newly_detached_buses = bs256_and_not(attached_buses, cur_attached_buses);
   }
}
#endif


/** Detect all currently attached buses and checks each to see if a display
 *  is connected, i.e. if an EDID is present
 *
 *  @return  array of #I2C_Bus_Info for all attached buses
 */
GPtrArray * i2c_detect_buses0() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "");

   // rpt_label(0, "*** Temporary code to exercise get_all_i2c_infos() ***");
   // GPtrArray * i2c_infos = get_all_i2c_info(true, -1);
   // dbgrpt_all_sysfs_i2c_info(i2c_infos, 2);

   Byte_Value_Array i2c_bus_bva = i2c_detect_attached_buses();
   GPtrArray * buses = g_ptr_array_sized_new(bva_length(i2c_bus_bva));
   for (int ndx = 0; ndx < bva_length(i2c_bus_bva); ndx++) {
      int busno = bva_get(i2c_bus_bva, ndx);
      DBGMSF(debug, "Checking busno = %d", busno);
      I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
      DBGMSF(debug, "Valid bus: /dev/"I2C"-%d", busno);
      g_ptr_array_add(buses, businfo);
   }
   bva_free(i2c_bus_bva);

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "buses->len = %d, i2c_businfo_async_threhold=%d",
         buses->len, i2c_businfo_async_threshold);
   if (buses->len < i2c_businfo_async_threshold) {
      i2c_non_async_scan(buses);
   }
   else {
      i2c_async_scan(buses);
   }

   if (debug) {
      for (int ndx = 0; ndx < buses->len; ndx++) {
         I2C_Bus_Info * businfo = g_ptr_array_index(buses, ndx);
         i2c_dbgrpt_bus_info(businfo, 0);
      }
   }

   if (debug) {
      for (int ndx = 0; ndx < buses->len; ndx++) {
         I2C_Bus_Info * businfo = g_ptr_array_index(buses, ndx);
         GPtrArray * conflicts = collect_conflicting_drivers(businfo->busno, -1);
         report_conflicting_drivers(conflicts, 1);
         DBGMSG("Conflicting drivers: %s", conflicting_driver_names_string_t(conflicts));
         free_conflicting_drivers(conflicts);
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_I2C,
         "Returning: %p containing %d I2C_Bus_Info records", buses, buses->len);
   return buses;
}


/** Creates a bit set in which the nth bit is set corresponding to the number
 *  of each bus in an array of #I2C_Bus_Info for which a monitor is connected,
 *  i.e. for which an EDID is detected.
 *
 *  @param  buses   array of I2C_Bus_Info
 *  @return bit set
 */
Bit_Set_256 buses_bitset_from_businfo_array(GPtrArray * businfo_array, bool only_connected) {
   bool debug = false;
   assert(businfo_array);
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo_array=%p, len=%d, only_connected=%s",
         businfo_array, businfo_array->len, SBOOL(only_connected));

   Bit_Set_256 result = EMPTY_BIT_SET_256;
   for (int ndx = 0; ndx < businfo_array->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(businfo_array, ndx);
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "businfo=%p", businfo);
      if (!only_connected || businfo->flags & I2C_BUS_ADDR_0X50) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "BUS_ADDR_0X50 set");
         result = bs256_insert(result, businfo->busno);
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s", bs256_to_string_decimal_t(result, "", ", "));
   return result;
}


/** Detect buses if not already detected.
 *
 *  Stores the result in global array all_i2c_buses and also
 *  the bitset connected_buses.
 *
 *  @return number of i2c buses
 */
int i2c_detect_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "all_i2c_buses = %p", all_i2c_buses);

   if (!all_i2c_buses) {
      all_i2c_buses = i2c_detect_buses0();
      g_ptr_array_set_free_func(all_i2c_buses, (GDestroyNotify) i2c_free_bus_info);
   }
   int result = all_i2c_buses->len;
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %d", result);
   return result;
}


/** Discard all known buses */
void i2c_discard_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (all_i2c_buses) {
      g_ptr_array_free(all_i2c_buses, true);
      all_i2c_buses= NULL;
   }
   // connected_buses = EMPTY_BIT_SET_256;
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


I2C_Bus_Info * i2c_detect_single_bus(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno = %d", busno);
   I2C_Bus_Info * businfo = NULL;

   if (i2c_device_exists(busno) ) {
      if (!all_i2c_buses) {
         all_i2c_buses = g_ptr_array_sized_new(1);
         g_ptr_array_set_free_func(all_i2c_buses, (GDestroyNotify) i2c_free_bus_info);
      }
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
      i2c_check_bus(businfo);
      if (debug)
         i2c_dbgrpt_bus_info(businfo, 0);
      g_ptr_array_add(all_i2c_buses, businfo);
   }

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "busno=%d, returning: %p", busno, businfo);
   return businfo;
}


//
// I2C Bus Inquiry
//

#ifdef UNUSED
/** Checks whether an I2C bus supports DDC.
 *
 *  @param  busno      I2C bus number
 *  @param  callopts   standard call options, used to control error messages
 *
 *  @return  true or false
 */
bool i2c_is_valid_bus(int busno, Call_Options callopts) {
   bool emit_error_msg = callopts & CALLOPT_ERR_MSG;
   bool debug = false;
   if (debug) {
      char * s = interpret_call_options_a(callopts);
      DBGMSG("Starting. busno=%d, callopts=%s", busno, s);
      free(s);
   }
   bool result = false;
   char * complaint = NULL;

   // Bus_Info * businfo = i2c_get_bus_info(busno, DISPSEL_NONE);
   I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
   if (debug && businfo)
      i2c_dbgrpt_bus_info(businfo, 1);

   bool overridable = false;
   if (!businfo)
      complaint = "I2C bus not found:";
   else if (!(businfo->flags & I2C_BUS_EXISTS))
      complaint = "I2C bus not found: /dev/i2c-%d\n";
   else if (!(businfo->flags & I2C_BUS_ACCESSIBLE))
      complaint = "Inaccessible I2C bus:";
   else if (!(businfo->flags & I2C_BUS_ADDR_0X50)) {
      complaint = "No monitor found on bus";
      overridable = true;
   }
   else if (!(businfo->flags & I2C_BUS_ADDR_0X37))
      complaint = "Cannot communicate DDC on I2C bus slave address 0x37";
   else
      result = true;

   if (complaint && emit_error_msg) {
      f0printf(ferr(), "%s /dev/i2c-%d\n", complaint, busno);
   }
   if (complaint && overridable && (callopts & CALLOPT_FORCE)) {
      f0printf(ferr(), "Continuing.  --force option was specified.\n");
      result = true;
   }

   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}
#endif


//
// Reports
//

/** Reports bus information for a single active display.
 *
 * Output is written to the current report destination.
 * Content shown is dependant on output level
 *
 * @param   businfo     bus record
 * @param   depth       logical indentation depth
 *
 * @remark
 * This function is used by detect, interrogate commands, C API
 */
void i2c_report_active_bus(I2C_Bus_Info * businfo, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p", businfo);
   assert(businfo);

   int d1 = depth+1;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_NORMAL)
      rpt_vstring(depth, "I2C bus:  /dev/"I2C"-%d", businfo->busno);
   // will work for amdgpu, maybe others

   if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED))
      i2c_check_businfo_connector(businfo);

   int title_width = (output_level >= DDCA_OL_VERBOSE) ? 39 : 25;
   if (businfo->drm_connector_name && output_level >= DDCA_OL_NORMAL) {
      int d = (output_level >= DDCA_OL_VERBOSE) ? d1 : depth;
      rpt_vstring( d, "%-*s%s", title_width, "DRM connector:",
                   (businfo->drm_connector_name) ? businfo->drm_connector_name : "Not found" );
      if (output_level >= DDCA_OL_VERBOSE) {
         if (businfo->drm_connector_name) {
            char title_buf[100];
            int tw = title_width; // 35;  // title_width;
            char * attr_value = NULL;

            char * attr = "dpms";
            attr_value = i2c_get_drm_connector_attribute(businfo, attr);
            g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
            rpt_vstring(d, "%-*s%s", tw, title_buf, attr_value);
            free(attr_value);

            attr = "enabled";
            attr_value = i2c_get_drm_connector_attribute(businfo, attr);
            g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
            rpt_vstring(d, "%-*s%s", tw, title_buf, attr_value);
            free(attr_value);

            attr = "status";
            attr_value = i2c_get_drm_connector_attribute(businfo, attr);
            g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
            rpt_vstring(d, "%-*s%s", tw, title_buf, attr_value);
            free(attr_value);

#ifdef OLD
            char * dpms    = NULL;
            char * status  = NULL;
            char * enabled = NULL;
            RPT_ATTR_TEXT(-1, &dpms,    "/sys/class/drm", businfo->drm_connector_name, "dpms");
            RPT_ATTR_TEXT(-1, &enabled, "/sys/class/drm", businfo->drm_connector_name, "enabled");
            RPT_ATTR_TEXT(-1, &status,  "/sys/class/drm", businfo->drm_connector_name, "status");
            if (dpms) {
               rpt_vstring(d+1,  "%-*s%s", title_width-3, "dpms:", dpms);
               free(dpms);
            }
            if (enabled) {
               rpt_vstring(d+1,  "%-*s%s", title_width-3, "enabled:", enabled);
               free(enabled);
            }
            if (status) {
               rpt_vstring(d+1,  "%-*s%s", title_width-3, "status:", status);
               free(status);
            }
#endif
         }
      }
   }

   // 08/2018 Disable.
   // Test for DDC communication is now done more sophisticatedly at the DDC level
   // The simple X37 test can have both false positives (DDC turned off in monitor but
   // X37 responsive), and false negatives (Dell P2715Q)
   // if (output_level >= DDCA_OL_NORMAL)
   // rpt_vstring(depth, "Supports DDC:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level >= DDCA_OL_VERBOSE) {
      rpt_vstring(d1, "Driver:                                %s", (businfo->driver) ? businfo->driver : "Unknown");
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(d1, "I2C address 0x30 (EDID block#)  present: %-5s", srepr(businfo->flags & I2C_BUS_ADDR_0X30));
#endif
      rpt_vstring(d1, "I2C address 0x50 (EDID) responsive:    %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(d1, "I2C address 0x37 (DDC)  responsive:    %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));
#ifdef OLD
      rpt_vstring(d1, "Is eDP device:                         %-5s", sbool(businfo->flags & I2C_BUS_EDP));
      rpt_vstring(d1, "Is LVDS device:                        %-5s", sbool(businfo->flags & I2C_BUS_LVDS));
#endif
      rpt_vstring(d1, "Is LVDS or EDP display:                %-5s", sbool(businfo->flags & I2C_BUS_LVDS_OR_EDP));
      rpt_vstring(d1, "Is laptop display by EDID:             %-5s", sbool(businfo->flags & I2C_BUS_APPARENT_LAPTOP));
      rpt_vstring(d1, "Is laptop display:                     %-5s", sbool(businfo->flags & I2C_BUS_LAPTOP));

      // if ( !(businfo->flags & (I2C_BUS_EDP|I2C_BUS_LVDS)) )
      // rpt_vstring(d1, "I2C address 0x37 (DDC) responsive:  %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

      char fn[PATH_MAX];     // yes, PATH_MAX is dangerous, but not as used here
      sprintf(fn, "/sys/bus/i2c/devices/i2c-%d/name", businfo->busno);
      char * sysattr_name = file_get_first_line(fn, /* verbose*/ false);
      rpt_vstring(d1, "%-*s%s", title_width, fn, sysattr_name);
      free(sysattr_name);
      sprintf(fn, "/sys/bus/i2c/devices/i2c-%d", businfo->busno);
      char * path = NULL;
      GET_ATTR_REALPATH(&path, fn);
      rpt_vstring(d1, "PCI device path:                       %s", path);
      free(path);

#ifdef REDUNDANT
#ifndef TARGET_BSD2
      if (output_level >= DDCA_OL_VV) {
         I2C_Sys_Info * info = get_i2c_sys_info(businfo->busno, -1);
         dbgrpt_i2c_sys_info(info, depth);
         free_i2c_sys_info(info);
      }
#endif
#endif
   }

   if (businfo->edid) {
      if (output_level == DDCA_OL_TERSE) {
         rpt_vstring(depth, "I2C bus:          /dev/"I2C"-%d", businfo->busno);
         if (businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_FOUND)
            rpt_vstring(depth, "DRM connector:    %s", businfo->drm_connector_name);
         rpt_vstring(depth, "Monitor:          %s:%s:%s",
                            businfo->edid->mfg_id,
                            businfo->edid->model_name,
                            businfo->edid->serial_ascii);
      }
      else
         report_parsed_edid_base(businfo->edid,
                           (output_level >= DDCA_OL_VERBOSE), // was DDCA_OL_VV
                           (output_level >= DDCA_OL_VERBOSE),
                           depth);
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


static void init_i2c_bus_core_func_name_table() {
   RTTI_ADD_FUNC(i2c_check_bus);
   RTTI_ADD_FUNC(i2c_check_businfo_connector);
   RTTI_ADD_FUNC(i2c_check_open_bus_alive);
   RTTI_ADD_FUNC(i2c_close_bus);
   RTTI_ADD_FUNC(i2c_detect_buses);
   RTTI_ADD_FUNC(i2c_detect_single_bus);
   RTTI_ADD_FUNC(i2c_detect_x37);
   RTTI_ADD_FUNC(i2c_discard_buses);
   RTTI_ADD_FUNC(i2c_enable_cross_instance_locks);
   RTTI_ADD_FUNC(i2c_open_bus);
   RTTI_ADD_FUNC(i2c_report_active_bus);
   RTTI_ADD_FUNC(is_laptop_drm_connector_name);
   RTTI_ADD_FUNC(threaded_initial_checks_by_businfo);
}


void subinit_i2c_bus_core() {
   // init_sysfs_drm_connector_names();
}


void init_i2c_bus_core() {
   init_i2c_bus_core_func_name_table();
   open_failures_reported = EMPTY_BIT_SET_256;
   // attached_buses = EMPTY_BIT_SET_256;
   // connected_buses = EMPTY_BIT_SET_256;
}

