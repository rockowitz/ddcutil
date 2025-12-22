/** @file i2c_bus_core.c
 *
 * I2C bus detection and inspection
 */
// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
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
/** \endcond */

#include "util/coredefs_base.h"
#include "util/debug_util.h"
#include "util/data_structures.h"
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
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#include "util/traced_function_stack.h"
#ifdef ENABLE_UDEV
#include "util/udev_i2c_util.h"
#endif
#include "util/utilrpt.h"
#ifdef USE_X11
#include "util/x11_util.h"
#endif

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/display_lock.h"
#include "base/flock.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"

#include "sysfs/sysfs_i2c_info.h"
#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_base.h"
#include "sysfs/sysfs_sys_drm_connector.h"
#include "sysfs/sysfs_conflicting_drivers.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif

#include "i2c/i2c_strategy_dispatcher.h"
#include "i2c/i2c_execute.h"
#include "i2c/i2c_edid.h"

#include "i2c/i2c_bus_core.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

bool i2c_force_bus = false;  // Another ugly global variable for testing purposes
bool all_video_adapters_implement_drm = false;
bool use_drm_connector_states = false;
bool try_get_edid_from_sysfs_first = true;
int  i2c_businfo_async_threshold = DEFAULT_BUS_CHECK_ASYNC_THRESHOLD;


// quick and dirty for debugging
static
char * edid_summary_from_bytes(Byte * edidbytes) {
   static GPrivate  key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&key, 200);
   if (!edidbytes)
      strcpy(buf, "null edid ptr");
   else {
      Parsed_Edid * parsed = create_parsed_edid(edidbytes);
      if (!parsed)
         strcpy(buf, "Invalid EDID");
      else {
         strcpy(buf, parsed->model_name);
         free_parsed_edid(parsed);
      }
   }

   return buf;
}





/** Gets a list of all /dev/i2c devices by checking the file system
 *  if devices named /dev/i2c-N exist.
 *
 *  @return Byte_Value_Array containing the valid bus numbers
 */
Byte_Value_Array i2c_get_devices_by_existence_test(bool include_ignorable_devices) {
   Byte_Value_Array bva = bva_create();
   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      // if (!i2c_bus_is_ignored(busno)) { // done in i2c_device_exists()
         if (i2c_device_exists(busno)) {
            if (include_ignorable_devices || !sysfs_is_ignorable_i2c_device(busno))
               bva_append(bva, busno);
         }
      // }
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


#ifdef ALT_LOCK_RECORD
Error_Info *
lock_display_by_businfo(
      I2C_Bus_Info *     businfo,
      Display_Lock_Flags flags)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = BusInfo[/dev/i2c-%d]", businfo->busno);
   Display_Lock_Record * lockid = businfo->lock_record;
   Error_Info * result = lock_display2(lockid, flags);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "device=/dev/i2c-%d", businfo->busno);
   return result;
}


Error_Info *
unlock_display_by_businfo(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = BusInfo[/dev/i2c-%d]", businfo->busno);
   Display_Lock_Record * lockid = businfo->lock_record;
   Error_Info * result = unlock_display2(lockid);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "device=/dev/i2c-%d", businfo->busno);
   return result;
}
#endif


Error_Info * i2c_open_bus_basic(const char * filename,  Byte callopts, int* fd_loc) {
   bool debug = false;
   Error_Info * err = NULL;
   RECORD_IO_EVENT(
         -1,
         IE_OPEN,
         ( *fd_loc = open(filename, (callopts & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR) )
         );
   // if successful returns file descriptor, if fail, returns -1 and errno is set
   if (*fd_loc < 0) {
      int errsv = -errno;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "open(%s) failed. errno=%s", filename, psc_desc(errsv));
      err = ERRINFO_NEW(errsv,  "Open failed for %s, errno=%s", filename, psc_desc(errsv));
   }

   return err;
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
Error_Info * i2c_open_bus(
      int busno,
#ifdef ALT_LOCK_RECORD
      Display_Lock_Record * lockrec,
#endif
      Byte callopts,
      int* fd_loc)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "/dev/i2c-%d, callopts=0x%02x=%s",
         busno, callopts, interpret_call_options_t(callopts));
   ASSERT_WITH_BACKTRACE(busno >= 0);
#ifdef ALT_LOCK_REC
   assert(lockrec);
#endif
   bool wait = callopts & CALLOPT_WAIT;
   // wait = true;  // *** TEMP ***

#ifdef ALT_LOCK_REC
   I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
   assert(businfo); // !!! fails, all_bus_info not yet set
#endif

   int open_max_wait_millisec = DEFAULT_OPEN_MAX_WAIT_MILLISEC;
   int open_wait_interval_millisec = DEFAULT_OPEN_WAIT_INTERVAL_MILLISEC;
   int total_wait_millisec = 0;

   char filename[20];
   Error_Info * master_error = NULL;
   assert(fd_loc);
   *fd_loc = -1;   // ?

   Display_Lock_Flags ddisp_flags = DDISP_NONE;
   // if (wait)
   //   ddisp_flags |= DDISP_WAIT;
   DDCA_IO_Path dpath;
   dpath.io_mode = DDCA_IO_I2C;
   dpath.path.i2c_busno = busno;
   snprintf(filename, 20, "/dev/"I2C"-%d", busno);
   int tryctr = 0;

   while( *fd_loc < 0 && total_wait_millisec <= open_max_wait_millisec) {
      bool device_locked = false;
      bool device_flocked = false;
      bool device_opened = false;
      tryctr++;

      Error_Info * cur_error = NULL;

      // 1) lock display within this ddcutil/libddcutil instance
      cur_error = lock_display_by_dpath(dpath, ddisp_flags);
      #ifdef ALT_LOCK_REC
      cur_error = lock_display2(businfo->lock_record, ddisp_flags);
      #endif
      if (cur_error) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "lock_display_by_dpath(%s) returned %s", filename,
                         psc_desc(cur_error->status_code));
      }
      else {
         device_locked = true;
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "lock_display_by_dpath(%s) succeeded", dpath_repr_t(&dpath));
      }

      // 2) Open the device
      if (!cur_error) {
         cur_error = i2c_open_bus_basic(filename, callopts, fd_loc);
         if (!cur_error) {
            device_opened = true;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "open(%s) succeeded, tryctr=%d", filename, tryctr);
         }
         else {
            if (cur_error->status_code == -EACCES ||
                cur_error->status_code == -ENOENT) 
            {
               // no point in retrying, force loop exit:
               total_wait_millisec = open_max_wait_millisec + 1;
            }
         }
      }

      // 3) create cross-instance lock
      if (!cur_error && cross_instance_locks_enabled) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Acquiring cross instance lock for %s", filename);
         Status_Errno flockrc = flock_lock_by_fd(*fd_loc, filename, wait );
         if (flockrc != 0) {
             DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Cross instance locking failed for %s", filename);
             cur_error = ERRINFO_NEW(flockrc, "flock_lock_by_fd(%s) returned %s", filename, psc_desc(flockrc));
#ifdef EXPERIMENTAL_FLOCK_RECOVREY
             Buffer * edidbuf = buffer_new(256, "");
             Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(*fd_loc, edidbuf);
             bool found_edid = (rc == 0);
             buffer_free(edidbuf, "");
             DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "able to read edid directly for /dev/i2c-%d: %s",
                   busno, sbool(found_edid));
             // TODO: read attributes
             // RPT_ATTR_TEXT(1, NULL, "/sys/class/drm", dh->dref->
#endif
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Cross instance locking succeeded for %s", filename);
         }
      }

      // operations complete, back out if error
      if (!cur_error)
         continue;

      // Something failed.  Release attached resources.
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "something failed, %s, cur_error = %s", filename,
            errinfo_summary(cur_error));

      assert (!device_flocked);  // it was the last thing attempted

      // 2) close the device if it was opened
      ASSERT_IFF(*fd_loc >= 0, device_opened);
      if (*fd_loc >= 0) {
         close(*fd_loc);
         *fd_loc = -1;
      }

      // 1) release the cross-thread lock
      if (device_locked) {
          Error_Info * err = unlock_display_by_dpath(dpath);
          // only error returned is DDCRC_LOCKED, which is impossible in this case, but nonetheless:
          if (err) {
             MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "unlock_display_by_dpath(%s) returned %d", dpath_repr_t(&dpath), err->status_code);
             ASSERT_WITH_BACKTRACE(!err);
          }
      }

#ifdef OLD
      if (!master_error)
         master_error = ERRINFO_NEW(DDCRC_OTHER, "i2c_open_bus() failed");  // need an DDCRC_OPEN

      errinfo_add_cause(master_error, cur_error);
#endif
      if (!master_error)
         master_error = cur_error;
      else
         errinfo_add_cause(master_error, cur_error);

      total_wait_millisec += open_wait_interval_millisec;

      if (total_wait_millisec > open_max_wait_millisec)
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Total wait %d exceeds max wait %d, tries=%d", total_wait_millisec, open_max_wait_millisec, tryctr);
      else {
         SLEEP_MILLIS_WITH_SYSLOG(open_wait_interval_millisec, "");
         // usleep(wait_interval_millisec * 1000);
      }
   }

   if (*fd_loc >= 0) {
      ERRINFO_FREE(master_error);
      master_error = NULL;
   }
   else {

      // if all causes have the same status code, replace the status code in the master error
   }

   ASSERT_IFF(master_error, *fd_loc == -1);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, master_error,
      "/dev/i2c-%d, tryctr=%d, Set file descriptor *fd_loc = %d", busno, tryctr, *fd_loc);
   return master_error;
}


Status_Errno i2c_close_bus_basic(int busno, int fd, Call_Options callopts) {
   int rc;
   Status_Errno result = 0;
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
      SYSLOG2(DDCA_SYSLOG_ERROR, "Close failed for %s, errno=%s\n",
            filename_for_fd_t(fd), linux_errno_desc(errsv));
      // assert(rc == 0);     // don't bother with recovery for now
   }
   return result;
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

#ifdef ALT_LOCK_BASIC
   I2C_Bus_Info * businfo = i2c_find_bus_info_by_busno(busno);
   assert(businfo);
   #endif

   Status_Errno result = 0;

   // 3) release cross-instance lock
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "calling flock() for /dev/i2c-%d...", busno);
   if (cross_instance_locks_enabled) {
      int rc = flock_unlock_by_fd(fd);
      if (rc < 0) {
         DBGTRC_NOPREFIX(true, TRACE_GROUP,
               "/dev/i2c-%d. Unexpected error from flock(..,LOCK_UN): %s",
               busno, psc_desc(rc));
      }
   }

   // 2) Close the device
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_close_bus for /dev/i2c-%d...", busno);
   result = i2c_close_bus_basic(busno, fd, callopts);
   assert(result == 0);   // TODO; handle failure
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "/dev/i2c-%d.  i2c_close_bus_basic() returned %d", busno, result);
   assert(result == 0);   // TODO; handle failure

   // 1) Release the cross-thread lock
   DDCA_IO_Path dpath;
   dpath.io_mode = DDCA_IO_I2C;
   dpath.path.i2c_busno = busno;
#ifdef ALT_LOCK_REC
   Error_Info * erec = unlock_display2(businfo->lock_record);
#endif
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling unlock_display_by_dpath(/dev/i2c-%d)...", busno);
   Error_Info * erec = unlock_display_by_dpath(dpath);
   if (erec) {
      char * s = g_strdup_printf("Unexpected error %s from unlock_display_by_dpath(%s)",
            psc_name(erec->status_code), dpath_repr_t(&dpath));
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "%s", s);
      SYSLOG2(DDCA_SYSLOG_ERROR, "%s", s);
      free(s);
      errinfo_free(erec);
   }

   assert(result <= 0);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "busno=%d, fd=%d",busno, fd);
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

STATIC bool
is_laptop_drm_connector_name(const char * connector_name) {
   bool debug = false;
   bool result = strstr(connector_name, "-eDP-") ||
                 strstr(connector_name, "-LVDS-");
   DBGF(debug, "connector_name=|%s|, returning %s", connector_name, sbool(result));
   return result;
}


//
// Check display status
//

/** Checks if the EDID of an existing display handle can be read
 *  using the handle's I2C bus.  Failure indicates that the display
 *  has been disconnected and the display handle is no longer valid.
 *
 *  @param  dh  display handle
 *  @return true if the EDID can be read, false if not
 */
STATIC bool
i2c_check_edid_exists_by_dh(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dh = %s", dh_repr(dh));

   Buffer * edidbuf = buffer_new(256, "");
   Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(dh->fd, edidbuf);
   bool result = (rc == 0);
   buffer_free(edidbuf, "");

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
   return result;
}


#ifdef UNUSED
/** Attempts to read the EDID on the I2C bus specified in
 *  a #Businfo record.
 *
 *  @param  businfo
 *  @return true if the EDID can be read, false if not
 */
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
#endif


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


static Status_Errno_DDC
i2c_detect_x37(int fd, char * driver) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d - %s, driver=%s", fd, filename_for_fd_t(fd), driver );

   // Quirks
   // - i2c_set_addr() Causes screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   //   avoided by never calling this function for an eDP screen
   // - Dell P2715Q does not respond to single byte read, but does respond to
   //   a write (7/2018), so this function checks both
   Status_Errno_DDC rc = 0;
   int max_tries = DETECT_X37_MAX_TRIES;  //2;   // ***TEMP*** 3;
   int poll_wait_millisec = DETECT_X37_RETRY_MILLISEC;  // 400;
   int loopctr;
   for (loopctr = 0; loopctr < max_tries; loopctr++) {  // retries seem to give no benefit
      // regard either a successful write() or a read() as indication slave address is valid
      Byte    writebuf = {0x00};
      rc = invoke_i2c_writer(fd, 0x37, 1, &writebuf);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_writer() for slave address x37 returned %s", psc_name_code(rc));
      if (rc != 0) {
         Byte    readbuf[4];  //  4 byte buffer
         rc = invoke_i2c_reader(fd, 0x37, false, 4, readbuf);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_reader() for slave address x37 returned %s", psc_name_code(rc));
      }
      if (rc == 0)
         break;

      int wait = poll_wait_millisec;
      if (streq(driver, "nvidia"))
         wait = 2000;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "driver=%s, sleeping for %d millisec", driver, wait);
            // usleep(poll_wait_millisec*1000);
      SLEEP_MILLIS_WITH_SYSLOG(wait, "Extra x37 sleep");
      // sleep_millis(wait);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc,"loopctr=%d", loopctr);
   return rc;
}


/** Tests if an open display handle is still valid
 *
 *  @param  dh     display handle
 *  @retval NULL   ok
 *  @retval Error_Info with status DDCRC_DISCONNECTED or DDCRC_DPMS_ASLEEP
 *                                 DDCRC_OTHER  slave addr x37 unresponsive
 *
 *  @remark
 *  Called from ddc_write_read_with_retry()
 */
Error_Info * i2c_check_open_bus_alive(Display_Handle * dh) {
   bool debug = false;
   assert(dh->dref->io_path.io_mode == DDCA_IO_I2C);
   I2C_Bus_Info * businfo = dh->dref->detail;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, busno=%d, businfo=%p", dh_repr(dh), businfo->busno, businfo );
   assert(businfo && ( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) );
   assert( (businfo->flags & I2C_BUS_EXISTS) &&
           (businfo->flags & I2C_BUS_PROBED)
         );

   if (current_traced_function_stack_size() > 0) {
      if (IS_DBGTRC(debug, TRACE_GROUP)) {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Traced function stack on entry to i2c_check_open_bus_alive","");
         // show_backtrace(0);   // all blank lines
         dbgrpt_current_traced_function_stack(false, true, 0);
      }
      syslog(LOG_DEBUG, "Traced function stack on entry to i2c_check_open_bus_alive()");
      current_traced_function_stack_to_syslog(LOG_DEBUG, /*reverse*/ false);
   }

   Error_Info * err = NULL;
   bool edid_exists = false;
   int tryctr = 1;
   for (; !edid_exists && tryctr <= CHECK_OPEN_BUS_ALIVE_MAX_TRIES; tryctr++) {
      if (tryctr > 1) {
         // DBGTRC_NOPREFIX(debug, TRACE_GROUP,
         //       "!!! (A) Retrying i2c_check_edid_exists, busno=%d, tryctr=%d, dh=%s",
         //       businfo->busno, tryctr, dh_repr(dh));
         // SYSLOG2(DDCA_SYSLOG_WARNING,
         //       "!!! (B) Retrying i2c_check_edid_exists_by_dh, tryctr=%d, dh=%s", tryctr, dh_repr(dh));
         SLEEP_MILLIS_WITH_SYSLOG2(DDCA_SYSLOG_WARNING, CHECK_OPEN_BUS_ALIVE_RETRY_MILLISEC,
                          "Retrying i2c_check_edid_exists_by_dh() tryctr=%d, dh=%s", tryctr, dh_repr(dh));
      }
#ifdef SYSFS_PROBLEMATIC   // apparently not by driver vfd on Raspberry pi
      if (businfo->drm_connector_name) {
         i2c_edid_exists = GET_ATTR_EDID(NULL, "/sys/class/drm/", businfo->drm_connector_name, "edid");
         // edid_exists = i2c_check_bus_responsive_using_drm(businfo->drm_connector_name);  // fails for Nvidia
      }
      else {
         // read edid
         i2c_edid_exists = i2c_check_edid_exists_by_dh(dh);
      }
#else
      edid_exists = i2c_check_edid_exists_by_dh(dh);
#endif
   }

   if (!edid_exists) {
      SYSLOG2(DDCA_SYSLOG_ERROR, "/dev/i2c-%d, Checking EDID failed after %d tries (B)",
            businfo->busno, CHECK_OPEN_BUS_ALIVE_MAX_TRIES);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "/dev/i2c-%d: Checking EDID failed (A)", businfo->busno);
      err = ERRINFO_NEW(DDCRC_DISCONNECTED, "/dev/i2c-%d", businfo->busno);
      businfo->flags &= ~(I2C_BUS_HAS_EDID|I2C_BUS_ADDR_X37);
   }
   else {
      if (tryctr > 1) {
         SYSLOG2(DDCA_SYSLOG_WARNING, "/dev/i2c-%d: Checking EDID succeeded after %d tries (G)",
               businfo->busno, tryctr);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "/dev/i2c-%d: Checking EDID succeeded after %d tries (H)",
               businfo->busno,tryctr);
      }
      char * driver = businfo->driver;
      int ddcrc = i2c_detect_x37(dh->fd, driver);
      if (ddcrc){
         err = ERRINFO_NEW(DDCRC_OTHER, "/dev/i2c-%d: Slave address x37 unresponsive. io status = %s",
               businfo->busno, psc_desc(ddcrc));
         businfo->flags &= ~I2C_BUS_ADDR_X37;
      }
   }
   if (!err) {
      if (dpms_check_drm_asleep_by_businfo(businfo))
         err = ERRINFO_NEW(DDCRC_DPMS_ASLEEP,
               "/dev/i2c-%d", dh->dref->io_path.path.i2c_busno);
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "");
   return err;
}


#ifdef UNUSED
Bit_Set_256 check_edids(GPtrArray * buses) {
   bool debug = false;
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

#ifdef UNUSED
/** The EDID can be read in several ways.  This function exists to
 *  verify that these methods obtain the same value.  It should be
 *  used only for test purposes.
 *  - value currently in struct I2C_Bus_Info
 *  - direct read using I2C
 *  - edid attribute in sysfs card-connector directory
 *  - using the DRM API
 *
 *  @param  fd       file descriptor for open /dev/i2c bus
 *  @param  businfo  I2C_Bus_Info struct
 */
void compare_edid_read_methods(int fd, I2C_Bus_Info * businfo) {
   assert(businfo->edid);
   // 1 - does sysfs bus info match directly read
   // if not:
   // 2 - trigger sysfs reread
   // 2a - does value read from drm match directly read value?
   // 2b - does value now read from sysfs match directly read value?

   bool debug =  true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", businfo->busno);

   Parsed_Edid * true_i2c_edid;
   DDCA_Status ddcrc = i2c_get_parsed_edid_by_fd(fd, &true_i2c_edid);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, i2c_get_parsed_edid_by_fd() returned %s",
         businfo->busno, psc_desc(ddcrc));
   bool reset = false;
   if (!true_i2c_edid) {
      SEVEREMSG("EDID read from sysfs but not from I2C. Discarding sysfs value");
      reset = true;
   }
   else if (memcmp( businfo->edid->bytes, true_i2c_edid->bytes, 128) != 0) {
      SEVEREMSG("busno=%d, Edid from sysfs does not match value read from i2c", businfo->busno);
      reset = true;
   }
   else {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "busno=%d, Edid initially read from sysfs matches direct read from I2C", businfo->busno);
   }
   if (reset) {
      free_parsed_edid(businfo->edid);
      businfo->flags &= ~ I2C_BUS_HAS_EDID;

      if (use_drm_connector_states) {
         DBGMSG("Resetting sysfs data using redetect_connector_states()");
         redetect_drm_connector_states();
      }

      // get the edid from connector states

      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
              "Getting edid from Drm Connector States for connector %s", businfo->drm_connector_name);
      Drm_Connector_Identifier dci =  parse_sys_drm_connector_name(businfo->drm_connector_name);
      if (use_drm_connector_states) {
         Drm_Connector_State * cstate = find_drm_connector_state(dci);
         if (cstate) {
            if (cstate->edid && true_i2c_edid) {
               if (memcmp(true_i2c_edid->bytes, cstate->edid->bytes, 128) == 0) {
                  DBGMSG("Correct edid now read from drm connector state");
               }
               else {
                  SEVEREMSG("Incorrect edid read from drm connector state");
               }
            }
            else if (cstate->edid && !true_i2c_edid) {
               SEVEREMSG("edid that should be nonexistent read from drm");
            }
            else if (!cstate->edid && true_i2c_edid) {
               SEVEREMSG("I2C edid exists but not read from drm");
            }
            else {
               assert (!cstate->edid && !true_i2c_edid);
               DBGMSG("I2C edid non-existent and none read from drm");
            }
         }
         else {
            SEVEREMSG("Drm_Connector_State not found for %s, %s",
                  businfo->drm_connector_name, dci_repr_t(dci));
         }
      }

      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                               "Getting edid from sysfs for connector %s", businfo->drm_connector_name);
      GByteArray*  sysfs_edid_bytes = NULL;
      // int d = IS_DBGTRC(debug, TRACE_GROUP) ? 1 : -1;
      int d = -1;
      RPT_ATTR_EDID(d, &sysfs_edid_bytes, "/sys/class/drm", businfo->drm_connector_name, "edid");
      if (sysfs_edid_bytes && true_i2c_edid) {
         if (memcmp(true_i2c_edid->bytes, sysfs_edid_bytes, 128) == 0) {
            DBGMSG("Correct edid now read from sysfs");
         }
         else {
            SEVEREMSG("Incorrect edid still read from sysfs");
         }
      }
      else if (sysfs_edid_bytes && !true_i2c_edid) {
         SEVEREMSG("edid that should be nonexistent read from sysfs");
      }
      else if (!sysfs_edid_bytes && true_i2c_edid) {
         SEVEREMSG("I2C edid exists but not read from sysfs");
      }
      else {
         assert (!sysfs_edid_bytes && !true_i2c_edid);
         DBGMSG("I2C edid non-existent and none read from sysfs");
      }

   }
   if (true_i2c_edid) {
      free_parsed_edid(true_i2c_edid);
   }

   if (reset) {
      free_parsed_edid(businfo->edid);
      businfo->edid = NULL;
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}
#endif


bool is_displaylink_device(int busno) {
   bool debug = false;
   bool result = false;
   char bus_path[40];
   g_snprintf(bus_path, 40, "/sys/bus/i2c/devices/i2c-%d", busno);
   char * name;
   RPT_ATTR_TEXT((debug)? 1 : -1, &name, bus_path, "name");
   if (name) {
      result =  streq(name, "DisplayLink I2C Adapter");
      free(name);
   }
   return result;
}


typedef struct {
   char *                 connector_name;
   int                    connector_id;
   Drm_Connector_Found_By found_by;
} Found_Sys_Drm_Connector;


void free_found_sys_drm_connector_result_contents(Found_Sys_Drm_Connector rec) {
   free(rec.connector_name);
}

void dbgrpt_found_sys_drm_connector(Found_Sys_Drm_Connector val, int depth) {
   rpt_vstring(depth, "Found_Sys_Drm_Connector:");
   rpt_vstring(depth+1, "connector_name:   %s", val.connector_name);
   rpt_vstring(depth+1, "connector_id:     %d", val.connector_id);
   rpt_vstring(depth+1, "found_by:         %s", drm_connector_found_by_name(val.found_by));
}


/** Locates a drm-card-connector directory using either an
 *  I2C bus number or EDID value.
 *
 *  @param  busno      (-1 for not set)
 *  @param  edid_bytes pointer to 128 byte edid
 *  @return #DRM
 *
 *  @remark
 *  Either one of busno edid_bytes should be set.  Having both parameters
 *  avoids having 2 separate functions, one for bus number and one for EDID,
 *  with essentially the same logic.
 *  @remark
 *  Given its small size, the result is returned on the stack, not
 *  the heap, avoiding the need for the caller to free.
 */
// n. result returned on stack
Found_Sys_Drm_Connector find_sys_drm_connector_by_busno_or_edid(
                                 int busno, Byte * edid_bytes)
{
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, " busno = %d, edid = %p" , busno, edid_bytes);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "edid=%p -> %s",
         edid_bytes, edid_summary_from_bytes(edid_bytes));
   int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;
   if (busno == 255)  // happens somehow
      busno = -1;
   bool check_busno = (busno != -1);
   bool check_edid = edid_bytes;
   assert(check_busno || check_edid);

   Found_Sys_Drm_Connector result;
   result.connector_name = NULL;
   result.found_by = DRM_CONNECTOR_NOT_FOUND;
   result.connector_id = 0;

   Sysfs_Connector_Names cnames = get_sysfs_drm_connector_names();
   GPtrArray * drm_connector_names = cnames.all_connectors;
   bool found = false;
   for (int ndx = 0; ndx < drm_connector_names->len && !found; ndx++) {
      char * cname = g_ptr_array_index(drm_connector_names, ndx);
      if (check_busno) {
         Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
         get_connector_bus_numbers("/sys/class/drm", cname, cbn);
         if (cbn->i2c_busno == busno){
            found = true;
            result.connector_name = strdup(cname);
            result.found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
            result.connector_id = cbn->connector_id;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Found connector %s by i2c bus number match for bus i2c-%d", cname, busno);
         }
         free_connector_bus_numbers(cbn);
      }
      if (check_edid) {
         // don't bother if we already have the answer
         if (result.found_by != DRM_CONNECTOR_FOUND_BY_BUSNO) {
            GByteArray*  edid_bytes_array = NULL;
            possibly_write_detect_to_status_by_connector_name(cname);
            RPT_ATTR_EDID(d, &edid_bytes_array, "/sys/class/drm", cname, "edid");
            if (edid_bytes_array && edid_bytes_array->len >= 128) {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Got edid from sysfs: %s",
                      edid_summary_from_bytes(edid_bytes_array->data));
                if (memcmp(edid_bytes_array->data, edid_bytes, 128) == 0) {
                   found = true;
                   result.connector_name = strdup(cname);
                   result.found_by = DRM_CONNECTOR_FOUND_BY_EDID;
                   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                         "Found connector %s by EDID match for bus i2c-%d", cname, busno);
                }
                g_byte_array_free(edid_bytes_array, true);
            }
         }
      }
   }
   free_sysfs_connector_names_contents(cnames);

   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      dbgrpt_found_sys_drm_connector(result, 1);
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
   return result;
}


/** Returns the value of the edid attribute for a DRM connector.
 *
 *  @param  connector_name
 *  @return pointer to EDID bytes, caller responsible for freeing
 *          NULL if not found
 */
Byte * get_connector_edid(const char * connector_name) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "connector_name = %s", connector_name);
   int d = (debug) ? 1 : -1;

   // char * driver =  get_i2c_sysfs_driver_by_busno(busno);    // where to get busno;
   // maybe_write_detect_to_status("nvidia", connector_name);     // lie

   Byte * result = NULL;
   GByteArray*  edid_bytes = NULL;
   possibly_write_detect_to_status_by_connector_name(connector_name);
   RPT_ATTR_EDID(d, &edid_bytes, "/sys/class/drm", connector_name, "edid");
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "edid_bytes=%p", edid_bytes);
   if (edid_bytes && edid_bytes->len >= 128) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "edid_bytes->len=%d", edid_bytes->len);
      result = edid_bytes->data;
      g_byte_array_free(edid_bytes, false);
   }
   else {
      if (edid_bytes)   {
         // handle pathological case of < 128 bytes read
         g_byte_array_free(edid_bytes, true);
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "result = %p", result);
   if (IS_DBGTRC(debug, DDCA_TRC_NONE) && result)
      rpt_hex_dump(result, 128, 2);
   return result;
}

#ifdef IRRELEVANT
    BS256 possible_buses = i2c_detect_attached_buses_as_bitset();  // excludes SMBUS devices etc.
    Bit_Set_256 iter = bs256_iter_new(possible_buses);
    while(true) {
       int busno_to_check = bs256_iter_next(iter);
       if (busno_to_check < 0)
          break;
       ///
    }
#endif


 /** Checks if an I2C bus has an EDID
  *
  *  @param  busno
  *  @return true/false
  */
 bool i2c_edid_exists(int busno) {
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d", busno);
    // int d = ( IS_DBGTRC(debug, TRACE_GROUP) ) ? 1 : -1;
    assert(busno >= 0);
    assert(busno != 255);
    char sysfs_name[30];
    char dev_name[15];
    char i2cN[10];  // only need 8, but coverity complains
    g_snprintf(i2cN, 10, "i2c-%d", busno);
    g_snprintf(sysfs_name, 30, "/sys/bus/i2c/devices/%s", i2cN);
    g_snprintf(dev_name,   15, "/dev/%s", i2cN);
    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sysfs_name = |%s|, dev_name = |%s|", sysfs_name, dev_name);
    bool edid_exists = false;
    char * drm_connector_name = NULL;

    Error_Info *master_err = NULL;
    if (!i2c_device_exists(busno)) {
       goto bye;
    }

    Error_Info * err = i2c_check_device_access(dev_name);
    if (err != NULL) {
       errinfo_free(err);   // for now
       goto bye;
    }

    if ( sysfs_is_ignorable_i2c_device(busno) ) {
       goto bye;
    }

    bool is_displaylink = is_displaylink_device(busno);

    bool drm_card_connector_directories_exist = directory_exists("/sys/class/drm");
    // *** Try to find the drm connector by bus number

    if (drm_card_connector_directories_exist) {
       // n. will fail for MST
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Finding DRM connector name for bus %s using busno", dev_name);
       Found_Sys_Drm_Connector res = find_sys_drm_connector_by_busno_or_edid(busno, NULL);
       if (res.connector_name) {
          drm_connector_name = strdup(res.connector_name);
          free_found_sys_drm_connector_result_contents(res);
       }
       else {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "DRM connector not found by busno %d", busno);
       }

       // *** Possibly try to get the EDID from sysfs
       bool checked_connector_for_edid = false;
       if (drm_connector_name)  {   // i.e. DRM_CONNECTOR_FOUND_BY_BUSNO
          if ((try_get_edid_from_sysfs_first && is_sysfs_reliable_for_busno(busno) && !primitive_sysfs ) ||
                is_displaylink)   // X50 can't be read for DisplayLink, must use sysfs
          {
             checked_connector_for_edid = true;
             Byte * edidbytes = get_connector_edid(drm_connector_name);
             if (edidbytes) {
                edid_exists = true;
                free(edidbytes);
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Retrieved edid using DRM connector %s", drm_connector_name);
             }
             else {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Failed to get edid using DRM connector %s", drm_connector_name);
             }
          }
       }
       if (checked_connector_for_edid)
          goto bye;
    }

    // *** Open bus

    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_open_bus for /dev/i2c-%d..", busno);
    int fd = -1;
    master_err = i2c_open_bus(busno, CALLOPT_WAIT, &fd);
 #ifdef ALT_LOCK_REC
    master_err = i2c_open_bus(businfo->busno, businfo->CALLOPT_WAIT, &fd);
 #endif
    if (master_err) {
       goto bye;
    }

    //open succeeded
    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened bus /dev/i2c-%d", busno);
    Buffer * rawedidbuf = buffer_new(EDID_BUFFER_SIZE, NULL);
    Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(fd, rawedidbuf);
    if (rc == 0) {
       edid_exists = true;
    }
    buffer_free(rawedidbuf, NULL);

    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Closing bus...");
     i2c_close_bus(busno, fd, CALLOPT_ERR_MSG);

 bye:
    free(drm_connector_name);
    ERRINFO_FREE_WITH_REPORT(master_err, IS_DBGTRC(debug, TRACE_GROUP) || is_report_ddc_errors_enabled() );
    DBGTRC_RET_BOOL(debug, TRACE_GROUP, edid_exists, "");
    return edid_exists;
 }


 //
 // Functions used only by i2c_check_bus(), but factored out to clarify
 // the function logic
 //

 Parsed_Edid * get_parsed_edid_for_businfo_using_sysfs(I2C_Bus_Info * businfo) {
     assert(businfo);
     bool debug  = false;
     DBGTRC_STARTING(debug, DDCA_TRC_NONE, "businfo = %p, businfo->busno=%d", businfo, businfo->busno);

     Parsed_Edid * pedid = NULL;

     // maybe_write_detect_to_status(businfo->driver, businfo->drm_connector_name);

     Byte * edidbytes = get_connector_edid(businfo->drm_connector_name);
     if (edidbytes) {
        pedid = create_parsed_edid2(edidbytes, "SYSFS");
        if (!pedid) {
           DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Invalid EDID read from /sys/class/drm/%s/edid",
                 businfo->drm_connector_name);
           SYSLOG2(DDCA_SYSLOG_ERROR, "Invalid EDID read from /sys/class/drm/%s/edid",
                 businfo->drm_connector_name);
        }
        else {
           DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Found edid for /dev/i2c-%d using connector name %s",
                 businfo->busno, businfo->drm_connector_name);
        }
        free(edidbytes);
     }
     else {
        DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Failed to get edid using DRM connector %s", businfo->drm_connector_name);
     }

     DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", pedid);
     return pedid;
  }


 /** Determines if a card-connector class attribute represents a
  *  display controller.
  *
  *  @param  adapter_class  class value as string
  *  @return true/false
  */
 bool is_adapter_class_display_controller(const char * adapter_class) {
    bool debug = false;
    DBGTRC_STARTING(debug, DDCA_TRC_NONE, "class = %s", adapter_class);

    bool result = true;
    uint32_t cl2 = 0;
    uint32_t i_class = 0;
    /* bool ok =*/  str_to_int(adapter_class, (int*) &i_class, 16);   // if fails, &result unchanged
    cl2 = i_class & 0xffff0000;
    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cl2 = 0x%08x", cl2);
    if (cl2 != 0x030000 && cl2 != 0x0a0000 /* docking station*/ ) {
        DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Device class not a display driver: 0x%08x", cl2);
        result = false;
    }

    DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "");
    return result;
 }


#ifdef UNUSED
 // TODO: MOVE ELSEWHERE
 /** Scans a single connector directory of /sys/class/drm.
  *
  *  Has typedef Dir_Foreach_Func
  *
  *  Adds the card-connector (as a string) to a #GPtrArray.
  *
  *  @param   dirname
  *  @param   fn
  *  @param   accumulator  #GPtrArray to collect the names
  *  @param   depth        ignored
  */
 void add_one_drm_connector_name(
       const char *  dirname,      // /sys/class/drm
       const char *  fn,           // e.g. card0-DP-1
       void *        accumulator,
       int           depth)
 {
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);

    GPtrArray * connectors = accumulator;
    g_ptr_array_add(connectors, g_strdup(fn));

    DBGTRC_DONE(debug, TRACE_GROUP, "");
 }


 /** Returns a GPtrArray of all the card-connector directory names,
  *  e.g. card1-HDMI-A-1
  *
  *  @return #GPtrArray of names, caller must free
  */
 GPtrArray *  get_drm_connector_names() {
    bool debug = false;
    DBGTRC_STARTING(debug, DDCA_TRC_I2C,"");

    GPtrArray * connector_names = g_ptr_array_new_with_free_func(free_sys_drm_connector);
    dir_filtered_ordered_foreach(
          "/sys/class/drm",
          is_drm_connector,      // filter function
          NULL,                  // ordering function
          add_one_drm_connector_name,
          connector_names,    // accumulator, GPtrArray *
          -1);
    DBGTRC_DONE(debug, DDCA_TRC_I2C, "size of sys_drm_connectors: %d", sys_drm_connectors->len);
    return connector_names;
 }


 /** Do card-connector directores exist?
  *
  *  @return true/false
  */
 bool drm_connectors_exist() {
    bool debug = false;
    DBGTRC_STARTING(debug, DDCA_TRC_I2C, "");
    GPtrArray * connector_names = get_drm_connector_names();
    bool result = (connector_names->len > 0);
    g_ptr_array_free(connector_names, true);
    DBGTRC_RET_BOOL(debug, DDCA_TRC_I2C, result, "");
    return result;
 }
#endif


 /** Sets the card-connector related fields in a #I2C_Bus_Info instance,
  *  by searching for the EDID value in the DRM card-connector directories
  *
  *  @param businfo pointer to I2C_Bus_Info instance
  *
  *  @remark
  *  Note that this function presumes that the video driver for the I2C bus
  *  supports DRM. This is not the case for older drivers, particularly Nvidia.
  *  @remark
  *  Writes to the system and (possibly) to the terminal if the instance is
  *  not found.
  */
void set_connector_for_businfo_using_edid(I2C_Bus_Info * businfo) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
          "Finding DRM connector name for bus i2c-%d using EDID, connector_directories_exist=%s",
          businfo->busno, SBOOL(sysfs_connector_directories_exist()));
   assert(businfo->edid);

   businfo->drm_connector_name = NULL;
   Found_Sys_Drm_Connector conres =    // n.b. struct returned on stack, not pointer
       find_sys_drm_connector_by_busno_or_edid(-1, businfo->edid->bytes);
   if (conres.connector_name) {
        businfo->drm_connector_name = conres.connector_name;
        businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_EDID;
        businfo->drm_connector_id = conres.connector_id;
        DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
              "Finding connector name for /dev/i2c-%d using EDID found: %s",
               businfo->busno, businfo->drm_connector_name);
   }
   else {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "Failed to find connector name for /dev/i2c-%d using EDID %p",
             businfo->busno, businfo->edid->bytes);
      char * msg = g_strdup_printf(
            "Failed to find connector name for /dev/i2c-%d, %s at line %d in file %s. ",
            businfo->busno,  __func__, __LINE__, __FILE__);
      if (sysfs_connector_directories_exist())
         LOGABLE_MSG(DDCA_SYSLOG_ERROR,"%s", msg);
      else {
         SYSLOG2(DDCA_SYSLOG_INFO, "%s", msg);
         SYSLOG2(DDCA_SYSLOG_INFO, "drm connector directories do not exist");
      }
      free(msg);
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE,"");
}


bool is_laptop_for_businfo(I2C_Bus_Info * businfo) {
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno=%d", businfo, businfo->busno);

   bool is_laptop = false;
   if (businfo->drm_connector_name) {
      if ( is_laptop_drm_connector_name(businfo->drm_connector_name) ) {
         // double check, eDP has been seen to be applied to external display, see:
         //   ddcutil issue #384
         //   freedesktop.org issue #10389, DRM connector for external monitor has name card1-eDP-1
         bool b = is_laptop_parsed_edid(businfo->edid);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                   "connector name = %s, is_laptop_parsed_edid() returned %s",
                   businfo->drm_connector_name, SBOOL(b));
         if (b) {
            businfo->flags |= I2C_BUS_LVDS_OR_EDP;
            is_laptop = true;
         }
      }
   }
   else {
      if ( is_laptop_parsed_edid(businfo->edid) ) {
         businfo->flags |= I2C_BUS_APPARENT_LAPTOP;
         is_laptop = true;
      }
   }

   ASSERT_IFF(is_laptop, businfo->flags & (I2C_BUS_LVDS_OR_EDP | I2C_BUS_APPARENT_LAPTOP));
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, is_laptop, "");
   return is_laptop;
}


bool check_x37_for_businfo(int fd, I2C_Bus_Info * businfo) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "fd=%d, businfo=%p, use_x37_detection_table=%s",
         fd, businfo, SBOOL(use_x37_detection_table));

   bool first_x37_check = true;
   X37_Detection_State x37_detection_state = X37_Not_Recorded;
   if (use_x37_detection_table) {
      x37_detection_state = i2c_query_x37_detected(businfo->busno, businfo->edid->bytes);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Restored(1) %s", x37_detection_state_name(x37_detection_state));
      if (x37_detection_state == X37_Detected) {
         businfo->flags |= I2C_BUS_ADDR_X37;
         first_x37_check=false;
      }
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "first_x37_check = %s", SBOOL(first_x37_check));
   if (x37_detection_state != X37_Detected) {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_detect_x37() for /dev/i2c-%d...", businfo->busno);
       int rc = i2c_detect_x37(fd, businfo->driver);
       // if (rc == -EBUSY)
       //    businfo->flags |= I2C_BUS_BUSY;
   #ifdef TEST
          if (rc == 0) {
             if (businfo->busno == 6 || businfo->busno == 8) {
                  rc = -EBUSY;
                  DBGMSG("Forcing -EBUSY on i2c_detect_37()");
             }
          }
   #endif
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "/dev/i2c-%d. i2c_detect_x37() returned %s",
             businfo->busno, psc_desc(rc));

       if (rc == 0) {
          businfo->flags |= I2C_BUS_ADDR_X37;
          x37_detection_state = X37_Detected;
       }
       else
          x37_detection_state = X37_Not_Detected;

       if (use_x37_detection_table) {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Recording %s", x37_detection_state_name(x37_detection_state));
          i2c_record_x37_detected(businfo->busno, businfo->edid->bytes, x37_detection_state);
       }

       if (first_x37_check) {
          businfo->flags &= ~I2C_BUS_DDC_CHECKS_IGNORABLE;
       }
   }
   bool result = (x37_detection_state == X37_Detected);

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "I2C_DDC_CHECKS_IGNORABLE is set: %s",
                            SBOOL(businfo->flags&I2C_BUS_DDC_CHECKS_IGNORABLE) );
   return result;
}


/** Inspects an I2C bus.
 *
 *  Takes the number of the bus to be inspected from the #I2C_Bus_Info struct passed
 *  as an argument.
 *
 *  @param  businfo  pointer to #I2C_Bus_Info struct in which information will be set
 *  @return status code
 */
Error_Info * i2c_check_bus(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, businfo=%p, primitive_sysfs=%s",
         businfo->busno, businfo, SBOOL(primitive_sysfs) );
   assert(businfo && ( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) );
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "businfo->flags = 0x%04x = %s", businfo->flags,
         i2c_interpret_bus_flags_t(businfo->flags));
   if (debug) {
      show_backtrace(1);
   }
   // int d = ( IS_DBGTRC(debug, TRACE_GROUP) ) ? 1 : -1;
   assert(businfo->busno >= 0);
   assert(businfo->busno != 255);
   bool try_get_edid_from_sysfs_first = true;

   // int busno = businfo->busno;
   char sysfs_name[30];
   char dev_name[15];
   char i2cN[10];  // only need 8, but coverity complains
   g_snprintf(i2cN, 10, "i2c-%d", businfo->busno);
   g_snprintf(sysfs_name, 30, "/sys/bus/i2c/devices/%s", i2cN);
   g_snprintf(dev_name,   15, "/dev/%s", i2cN);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sysfs_name = |%s|, dev_name = |%s|", sysfs_name, dev_name);
   // int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;
   bool drm_card_connector_directories_exist = sysfs_connector_directories_exist();

   businfo->flags |= I2C_BUS_PROBED;
   Error_Info *master_err = NULL;
   if (!i2c_device_exists(businfo->busno)) {
      master_err = ERRINFO_NEW(-ENOENT, "Device does not exist: /dev/i2c-%d", businfo->busno);
      goto bye;
   }

   master_err = i2c_check_device_access(dev_name);
   if (master_err != NULL) {
      // if (err->status_code != -ENOENT)
      businfo->open_errno = master_err->status_code;
      // errinfo_free(err);   // for now
      goto bye;
   }

   if (!primitive_sysfs) {
      if (!businfo->driver) {
         Sysfs_I2C_Info * driver_info = get_i2c_driver_info(businfo->busno, -1);
         businfo->driver = g_strdup(driver_info->driver);  // ** LEAKY
         // perhaps save businfo->driver_version
         // assert(driver_info->adapter_class);
         bool is_video_driver = false;
         if (driver_info->adapter_class) {
            is_video_driver = is_adapter_class_display_controller(driver_info->adapter_class);
         }
         if (!is_video_driver) {
            master_err = ERRINFO_NEW(DDCRC_OTHER, "Display controller for bus %d has class %s",
                  businfo->busno, driver_info->adapter_class);
            free_sysfs_i2c_info(driver_info);
            goto bye;
         }
         free_sysfs_i2c_info(driver_info);
      }
   }

   businfo->flags |= I2C_BUS_EXISTS;
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "initial flags = %s", i2c_interpret_bus_flags_t(businfo->flags));

   if (is_displaylink_device(businfo->busno))
      businfo->flags |= I2C_BUS_DISPLAYLINK;

   if (is_sysfs_reliable_for_busno(businfo->busno))
      businfo->flags |= I2C_BUS_SYSFS_KNOWN_RELIABLE;

   // *** Try to find the drm connector by bus number

   if (!businfo->drm_connector_name) {  // i.e. this is not a recheck
      //assert(businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_CHECKED ||
      //       businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_FOUND);
      businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_CHECKED;
      if (drm_card_connector_directories_exist) {
         // n. will fail for MST
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Finding DRM connector name for bus %s using busno", dev_name);
         Found_Sys_Drm_Connector res = find_sys_drm_connector_by_busno_or_edid(businfo->busno, NULL);
         if (res.connector_name) {
            businfo->drm_connector_name = strdup(res.connector_name);  // *** LEAKS ***
            businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
            businfo->drm_connector_id = res.connector_id;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Found DRM connector name %s by busno, found_by=%s",
                  businfo->drm_connector_name, drm_connector_found_by_name(businfo->drm_connector_found_by));
            free_found_sys_drm_connector_result_contents(res);
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "DRM connector not found by busno %d", businfo->busno);
         }
      }
   }

   // *** Possibly try to get the EDID from sysfs
   bool checked_connector_for_edid = false;
   if (businfo->drm_connector_name)  {   // i.e. DRM_CONNECTOR_FOUND_BY_BUSNO
      // assert(businfo->drm_connector_found_by == DRM_CONNECTOR_FOUND_BY_BUSNO);
      if ((try_get_edid_from_sysfs_first && businfo->flags&I2C_BUS_SYSFS_KNOWN_RELIABLE)  ||
            (businfo->flags&I2C_BUS_DISPLAYLINK))   // X50 can't be read for DisplayLink, must use sysfs
      {
         Parsed_Edid * edid = get_parsed_edid_for_businfo_using_sysfs(businfo);
         if (edid) {
            businfo->edid = edid;
            businfo->flags |= I2C_BUS_SYSFS_EDID;
         }
         checked_connector_for_edid = true;
      }
   }

   // *** Open bus

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_open_bus for /dev/i2c-%d..", businfo->busno);
   int fd = -1;
   master_err = i2c_open_bus(businfo->busno, CALLOPT_WAIT, &fd);
#ifdef ALT_LOCK_REC
   master_err = i2c_open_bus(businfo->busno, businfo->CALLOPT_WAIT, &fd);
#endif
   if (master_err) {
      businfo->open_errno = master_err->status_code;
      goto bye;
   }

   //open succeeded
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened bus /dev/i2c-%d", businfo->busno);
   businfo->flags |= I2C_BUS_ACCESSIBLE;
   businfo->functionality = i2c_get_functionality_flags_by_fd(fd);  // is this really needed?
#ifdef TEST_EDID_SMBUS
             if (EDID_Read_Uses_Smbus) {
                // for the smbus hack
                assert(businfo->functionality & I2C_FUNC_SMBUS_READ_BYTE_DATA);
             }
#endif
   if (!checked_connector_for_edid) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, calling i2c_get_parsed_edid", businfo->busno);
      assert(!businfo->edid);
      DDCA_Status ddcrc = i2c_get_parsed_edid_by_fd(fd, &businfo->edid);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, i2c_get_parsed_edid_by_fd() returned %s",
                    businfo->busno, psc_desc(ddcrc));
      // NB It's quite possible that bus has no edid
      if (ddcrc == 0) {
         businfo->flags |=  I2C_BUS_X50_EDID;
      }
      else {
     //    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, i2c_get_parsed_edid_by_fd() returned %s",
       //         businfo->busno, psc_desc(ddcrc));
      }
   }

   // If there's an EDID on the bus and we don't yet have the connector name
   // based on a busno match, try EDID match
   if (!businfo->drm_connector_name && businfo->edid && drm_card_connector_directories_exist) {
      set_connector_for_businfo_using_edid(businfo);
   }

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Bus %s: connector_name=%s, found by: %s",
         dev_name, businfo->drm_connector_name,
         drm_connector_found_by_name(businfo->drm_connector_found_by));

   if (businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_CHECKED)
      businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_FOUND;

   // *** Check if laptop
   bool is_laptop = false;
   if (businfo->edid && !(businfo->flags&I2C_BUS_DISPLAYLINK)) {
      is_laptop = is_laptop_for_businfo(businfo);
   }


   // *** Check x37
   if (is_laptop) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Laptop display detected, not checking x37");
   }
   else  if (businfo->edid) {  // start, x37 check

      Monitor_Model_Key mmk = mmk_value_from_edid(businfo->edid);
      bool disabled_mmk = is_ignored_mmk(mmk);
      if (disabled_mmk) {
         businfo->flags |= I2C_BUS_DDC_DISABLED;
      }
      else {
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

         check_x37_for_businfo(fd,businfo);
      }
   }

   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Closing bus...");
   i2c_close_bus(businfo->busno, fd, CALLOPT_ERR_MSG);

    businfo->flags |= I2C_BUS_INITIAL_CHECK_DONE;

bye:
   if ( IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      // DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "busno=%d, flags = %s",
      //       businfo->busno, i2c_interpret_bus_flags_t(businfo->flags));
      i2c_dbgrpt_bus_info(businfo, /* include_sysinfo */ true, 2);
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, master_err, "");
   return master_err;
}  // i2c_check_bus


#ifdef OUT
void i2c_recheck_bus(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, businfo=%p, flags=%s",
         businfo->busno, businfo, i2c_interpret_bus_flags(businfo->flags) );
   assert(businfo && ( memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0) );
   // show_backtrace(1);
   // int d = ( IS_DBGTRC(debug, TRACE_GROUP) ) ? 1 : -1;
   assert(businfo->busno >= 0);
   assert(businfo->busno != 255);
   // bool try_get_edid_from_sysfs_first = true;
   // int busno = businfo->busno;
   char sysfs_name[30];
   char dev_name[15];
   char i2cN[10];  // only need 8, but coverity complains
   g_snprintf(i2cN, 10, "i2c-%d", businfo->busno);
   g_snprintf(sysfs_name, 30, "/sys/bus/i2c/devices/%s", i2cN);
   g_snprintf(dev_name,   15, "/dev/%s", i2cN);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sysfs_name = |%s|, dev_name = |%s|", sysfs_name, dev_name);
   // int d = (IS_DBGTRC(debug, DDCA_TRC_NONE)) ? 1 : -1;

   i2c_reset_bus_info(businfo);
   businfo->flags |= I2C_BUS_PROBED;
   Error_Info *master_err = NULL;
   // if (!i2c_device_exists(businfo->busno))
   //    goto bye;

   master_err = i2c_check_device_access(dev_name);
   if (master_err != NULL) {
      goto bye;
   }
   businfo->flags |= I2C_BUS_EXISTS | I2C_BUS_ACCESSIBLE;

   assert(businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_CHECKED);

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "flags after i2c_reset_bus() and i2c_check_bus_access() = %s", i2c_interpret_bus_flags_t(businfo->flags));

   // *** Possibly try to get the EDID from sysfs
   bool checked_connector_for_edid = false;
   if ( !(businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_FOUND) &&
        !(businfo->flags&I2C_BUS_SYSFS_UNRELIABLE) )
   {
      checked_connector_for_edid = true;
      Byte * edidbytes = get_connector_edid(businfo->drm_connector_name);
      if (edidbytes) {
         businfo->edid = create_parsed_edid2(edidbytes, "SYSFS");
         if (!businfo->edid) {
            MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Invalid EDID read from /sys/class/drm%s/edid", businfo->drm_connector_name);
         }
         else {
            businfo->flags |= I2C_BUS_SYSFS_EDID;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Found edid for %s using connector name %s", dev_name, businfo->drm_connector_name);
         }
         free(edidbytes);
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Failed to get edid using DRM connector %s", businfo->drm_connector_name);
      }
   }
   else {
      assert(businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_FOUND);
   }

   X37_Detection_State x37_detection_state = X37_Not_Recorded;
   if (businfo->edid) {
      x37_detection_state = i2c_query_x37_detected(businfo->busno, businfo->edid->bytes);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Restored(1) %s", x37_detection_state_name(x37_detection_state));
      if (x37_detection_state == X37_Detected) {
         businfo->flags |= I2C_BUS_ADDR_X37;
      }
   }

   if (!checked_connector_for_edid || x37_detection_state != X37_Not_Recorded) {
      // *** Open bus

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_open_bus for /dev/i2c-%d..", businfo->busno);
      int fd = -1;
      master_err = i2c_open_bus(businfo->busno, CALLOPT_WAIT, &fd);
   #ifdef ALT_LOCK_REC
         master_err = i2c_open_bus(businfo->busno, businfo->CALLOPT_WAIT, &fd);
   #endif
      if (master_err) {
         businfo->open_errno = master_err->status_code;
         goto bye;
      }

      //open succeeded
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Opened bus /dev/i2c-%d", businfo->busno);
      businfo->flags |= I2C_BUS_ACCESSIBLE;

      if (!checked_connector_for_edid) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, calling i2c_get_parsed_edid", businfo->busno);
         DDCA_Status ddcrc = i2c_get_parsed_edid_by_fd(fd, &businfo->edid);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "busno=%d, i2c_get_parsed_edid_by_fd() returned %s",
                    businfo->busno, psc_desc(ddcrc));
         // NB It's quite possible that bus has no edid
         if (ddcrc == 0) {
            businfo->flags |= I2C_BUS_X50_EDID;
         }
      }

      // *** Check x37
      if (businfo->flags & (I2C_BUS_LVDS_OR_EDP)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Laptop display detected, not checking x37");
      }
      else if (businfo->edid) {  // start, x37 check
         x37_detection_state = i2c_query_x37_detected(businfo->busno, businfo->edid->bytes);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Restored(2) %s", x37_detection_state_name(x37_detection_state));
         if (x37_detection_state == X37_Not_Recorded) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_detect() for /dev/i2c-%d...", businfo->busno);
            int rc = i2c_detect_x37(fd);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s. i2c_detect_x37() returned %s", dev_name, psc_desc(rc));
            X37_Detection_State detection_state = X37_Not_Detected;
            if (rc == 0) {
               businfo->flags |= I2C_BUS_ADDR_X37;
               detection_state = X37_Detected;
            }
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Recording %s", x37_detection_state_name(detection_state));
            i2c_record_x37_detected(businfo->busno, businfo->edid->bytes, detection_state);
         }
         else {
            if (x37_detection_state == X37_Detected) {
               businfo->flags |= I2C_BUS_ADDR_X37;
            }
         }
      }    // end x37 check
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Closing bus...");
      i2c_close_bus(businfo->busno, fd, CALLOPT_ERR_MSG);
   }

   // doesn't really belong here
   businfo->last_checked_dpms_asleep = dpms_check_drm_asleep_by_businfo(businfo);

bye:
   businfo->flags |= I2C_BUS_PROBED;
   if ( IS_DBGTRC(debug, TRACE_GROUP)) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "busno=%d, flags = %s", businfo->busno, i2c_interpret_bus_flags_t(businfo->flags));

      // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "businfo:");
      // i2c_dbgrpt_bus_info(businfo, 2);
      DBGTRC_DONE(true, TRACE_GROUP, "busno=%d", businfo->busno);
      ERRINFO_FREE_WITH_REPORT(master_err, true);
   }
   else {
      ERRINFO_FREE_WITH_REPORT(master_err, false);
   }
}
#endif


STATIC void *
i2c_threaded_initial_checks_by_businfo(gpointer data) {
   bool debug = false;

   I2C_Bus_Info * businfo = data;
   TRACED_ASSERT(memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = /dev/i2c-%d", businfo->busno );

   Error_Info * err = i2c_check_bus(businfo);
   // g_thread_exit(NULL);

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "bus=/dev/i2c-%d", businfo->busno );
   free_current_traced_function_stack();
   return err;
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
            i2c_threaded_initial_checks_by_businfo,
            businfo);                            // pass pointer to display ref as data
      g_ptr_array_add(threads, th);
   }
   DBGMSF(debug, "Started %d threads", threads->len);
   for (int ndx = 0; ndx < threads->len; ndx++) {
      GThread * thread = g_ptr_array_index(threads, ndx);
      Error_Info * err = g_thread_join(thread);  // implicitly unrefs the GThread
      ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, TRACE_GROUP) || is_report_ddc_errors_enabled() );
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
   Error_Info * err = NULL;

   for (int ndx = 0; ndx < i2c_buses->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(i2c_buses, ndx);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Calling i2c_check_bus() synchronously for bus %d", businfo->busno);
      err = i2c_check_bus(businfo);
      ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, TRACE_GROUP) || is_report_ddc_errors_enabled() );
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Attached buses
//

/** Gets the numbers of I2C devices
 *
 *  \param  include_ignorable_devices  if true, do not exclude SMBus and other ignorable devices
 *  \return sorted #Byte_Value_Array of I2C device numbers, caller is responsible for freeing
 */
Byte_Value_Array
i2c_get_device_numbers_using_udev(bool include_ignorable_devices) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "include_ignorable_devices=%s", SBOOL(include_ignorable_devices));

   Byte_Value_Array bva = bva_create();

   GPtrArray * summaries = get_i2c_devices_using_udev();
   if (summaries) {
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         int busno = udev_i2c_device_summary_busno(summary);
         assert(busno >= 0);
         assert(busno <= 127);
         // if (!i2c_bus_is_ignored(busno))  { // done by caller
            if ( include_ignorable_devices || !sysfs_is_ignorable_i2c_device(busno) )
               bva_append(bva, busno);
         // }
      }
      free_udev_device_summaries(summaries);
   }

   char * s = bva_as_string(bva, /*as_hex*/ false, ",");
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning I2C bus numbers: %s", s);
   free(s);

   return bva;
}


/** Returns the bus numbers for /dev/i2c buses that could possibly be
 *  connected to a monitor.:
 *
 *  @return array of bus numbers
 */
Byte_Value_Array i2c_detect_attached_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
#ifdef ENABLE_UDEV    // perhaps slightly faster   TODO: perform test
   // do not include devices with ignorable name, etc.:
   Byte_Value_Array bva0 =
            i2c_get_device_numbers_using_udev(/*include_ignorable_devices=*/ false);
#else
   Byte_Value_Array bva0 =
            i2c_get_devices_by_existence_test(/*include_ignorable_devices=*/ false);
#endif

   Byte_Value_Array bva = bva_filter(bva0, i2c_bus_is_not_ignored);
   bva_free(bva0);

   char * s = bva_as_string(bva,  false,  ", ");
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "possible i2c device bus numbers: %s", s);
   free(s);
   return bva;;
}


/** Returns the bus numbers for /dev/i2c buses that could possibly be
 *  connected to a monitor.
 *
 *  @return bitset of bus numbers
 */
Bit_Set_256 i2c_detect_attached_buses_as_bitset() {
   Byte_Value_Array bva = i2c_detect_attached_buses();
   Bit_Set_256  cur_buses = bs256_from_bva(bva);
   bva_free(bva);
   return cur_buses;
}


Bit_Set_256 i2c_filter_buses_w_edid_as_bitset(BS256 bs_all_buses) {
   BS256 bs_buses_w_edid = EMPTY_BIT_SET_256;
   Bit_Set_256_Iterator iter =  bs256_iter_new(bs_all_buses);
   int bitno = bs256_iter_next(iter);
   while (bitno >= 0) {
      if (i2c_edid_exists(bitno))
         bs_buses_w_edid = bs256_insert(bs_buses_w_edid, bitno);
      bitno = bs256_iter_next(iter);
   }
   bs256_iter_free(iter);
   return bs_buses_w_edid;
}


Bit_Set_256 i2c_buses_w_edid_as_bitset() {
   BS256 bs_all_buses = i2c_detect_attached_buses_as_bitset();
   return i2c_filter_buses_w_edid_as_bitset(bs_all_buses);
}


#ifdef UNUSED
void i2c_check_attached_buses(
      Bit_Set_256* newly_attached_buses_loc,
      Bit_Set_256* newly_detached_buses_loc)
{
   Bit_Set_256 cur_attached_buses = i2c_detect_attached_buses_as_bitset();
   *newly_attached_buses_loc = EMPTY_BIT_SET_256;
   *newly_detached_buses_loc = EMPTY_BIT_SET_256;
   if (!bs256_eq(cur_attached_buses, attached_buses)) {   // will be rare
      Bit_Set_256 newly_attached_buses = bs256_and_not(cur_attached_buses, attached_buses);
      Bit_Set_256 newly_detached_buses = bs256_and_not(attached_buses, cur_attached_buses);
      *newly_attached_buses_loc = newly_attached_buses;
      *newly_detached_buses_loc = newly_detached_buses;
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

   BS256 bs_attached_buses = i2c_detect_attached_buses_as_bitset();
   Bit_Set_256_Iterator iter = bs256_iter_new(bs_attached_buses);
   GPtrArray * buses = g_ptr_array_sized_new(bs256_count(bs_attached_buses));
   while (true) {
      int busno = bs256_iter_next(iter);
      if (busno < 0)
         break;
      I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
      assert(businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_CHECKED);
      businfo->flags = I2C_BUS_EXISTS;
      DBGMSF(debug, "Valid bus: /dev/"I2C"-%d", busno);
      g_ptr_array_add(buses, businfo);
   }
   bs256_iter_free(iter);

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
         i2c_dbgrpt_bus_info(businfo, true, 0);
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


I2C_Bus_Info * i2c_get_and_check_bus_info(int busno) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);

   bool new_info = false;
   I2C_Bus_Info* businfo =  i2c_get_bus_info(busno, &new_info);
   if (!new_info)
      i2c_reset_bus_info(businfo);
   Error_Info * err = i2c_check_bus(businfo);
   ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, DDCA_TRC_NONE) || is_report_ddc_errors_enabled());
#ifdef OLD
   if (new_info | !(businfo->flags&I2C_BUS_INITIAL_CHECK_DONE)) {
      i2c_check_bus(businfo);
   }
   else {
      i2c_recheck_bus(businfo);
   }
#endif

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p, new_info=%s", businfo, SBOOL(new_info));
   return businfo;
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


// used only by main.c, not shared library, does not need mutex protection
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
      businfo->flags = I2C_BUS_EXISTS;
      Error_Info * err = i2c_check_bus(businfo);
      ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug, DDCA_TRC_I2C) || is_report_ddc_errors_enabled());
      if (debug)
         i2c_dbgrpt_bus_info(businfo, true, 0);
      g_ptr_array_add(all_i2c_buses, businfo);
   }

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "busno=%d, returning: %p", busno, businfo);
   return businfo;
}


/** Creates a bit set in which the nth bit is set corresponding to the number
 *  of each bus in an array of #I2C_Bus_Info, possibly restricted to those buses
 *  for which a monitor is connected, i.e. for which an EDID is detected.
 *
 *  @param  buses   array of I2C_Bus_Info
 *  @param  only_connected if true, only include buses having EDID
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
      if (!only_connected || businfo->edid) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "EDID exists");
         result = bs256_insert(result, businfo->busno);
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s", bs256_to_string_decimal_t(result, "", ", "));
   return result;
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
   else if (!(businfo->flags & I2C_BUS_ADDR_X37))
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

#define DO_OUTPUT(_indent, _title_width, _title, _value) \
      rpt_vstring(_indent, "%-*s%s", _title_width, _title, _value);

   int d1 = depth+1;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_NORMAL)
      rpt_vstring(depth, "I2C bus:  /dev/"I2C"-%d", businfo->busno);
   // will work for amdgpu, maybe others

   assert(businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_CHECKED);
   // if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED))
   //    i2c_check_businfo_connector(businfo);

   int title_width = (output_level >= DDCA_OL_VERBOSE) ? 43 : 25;
   int d = (output_level >= DDCA_OL_VERBOSE) ? d1 : depth;
   if (businfo->drm_connector_name && output_level >= DDCA_OL_NORMAL) {
      DO_OUTPUT(d, title_width, "DRM_connector:",
            (businfo->drm_connector_name) ? businfo->drm_connector_name : "Not found" );
   }

   if (output_level >= DDCA_OL_VERBOSE) {
      if (businfo->drm_connector_name) {
         char title_buf[100];
         int tw = title_width; // 35;  // title_width;
         char * attr_value = NULL;

         char * attr = "dpms";
         attr_value = i2c_get_drm_connector_attribute(businfo, attr);
         g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
         DO_OUTPUT(d, tw, title_buf, attr_value);
         free(attr_value);

         attr = "enabled";
         attr_value = i2c_get_drm_connector_attribute(businfo, attr);
         g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
         DO_OUTPUT(d, tw, title_buf, attr_value);
         free(attr_value);

         attr = "status";
         attr_value = i2c_get_drm_connector_attribute(businfo, attr);
         g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
         DO_OUTPUT(d, tw, title_buf, attr_value);
         free(attr_value);

         attr = "connector_id";
         attr_value = i2c_get_drm_connector_attribute(businfo, attr);
         g_snprintf(title_buf, 100, "/sys/class/drm/%s/%s", businfo->drm_connector_name, attr);
         DO_OUTPUT(d, tw, title_buf, attr_value);
         free(attr_value);
      }
   }

   // 08/2018 Disable.
   // Test for DDC communication is now done more sophisticatedly at the DDC level
   // The simple X37 test can have both false positives (DDC turned off in monitor but
   // X37 responsive), and false negatives (Dell P2715Q)
   // if (output_level >= DDCA_OL_NORMAL)
   // rpt_vstring(depth, "Supports DDC:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level >= DDCA_OL_VERBOSE) {
      DO_OUTPUT(d1, title_width, "Driver:", (businfo->driver) ? businfo->driver : "Unknown");
// #ifdef DETECT_SLAVE_ADDRS
      DO_OUTPUT(d1, title_width, "I2C address 0x30 (EDID block#)  present:", sbool(businfo->flags & I2C_BUS_ADDR_X30));
// #endif
      DO_OUTPUT(d1, title_width, "EDID exists:",  sbool(businfo->flags & I2C_BUS_HAS_EDID));
      DO_OUTPUT(d1, title_width, "I2C address 0x37 (DDC) responsive:", sbool(businfo->flags & I2C_BUS_ADDR_X37));
#ifdef OLD
      rpt_vstring(d1, "Is eDP device:                         %-5s", sbool(businfo->flags & I2C_BUS_EDP));
      rpt_vstring(d1, "Is LVDS device:                        %-5s", sbool(businfo->flags & I2C_BUS_LVDS));
#endif
      DO_OUTPUT(d1, title_width, "Is LVDS or EDP display:", sbool(businfo->flags & I2C_BUS_LVDS_OR_EDP));
      DO_OUTPUT(d1, title_width, "Is laptop display by EDID:", sbool(businfo->flags & I2C_BUS_APPARENT_LAPTOP));
      DO_OUTPUT(d1, title_width, "Is laptop display:",          sbool(businfo->flags & I2C_BUS_LAPTOP));

      // if ( !(businfo->flags & (I2C_BUS_EDP|I2C_BUS_LVDS)) )
      // rpt_vstring(d1, "I2C address 0x37 (DDC) responsive:  %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

      char fn[PATH_MAX];     // yes, PATH_MAX is dangerous, but not as used here
      sprintf(fn, "/sys/bus/i2c/devices/i2c-%d/name", businfo->busno);
      char * sysattr_name = file_get_first_line(fn, /* verbose*/ false);
      // rpt_vstring(d1, "%-*s%s", title_width, fn, sysattr_name);
      DO_OUTPUT(d1, title_width, fn, sysattr_name);
      free(sysattr_name);
      sprintf(fn, "/sys/bus/i2c/devices/i2c-%d", businfo->busno);
      char * path = NULL;
      GET_ATTR_REALPATH(&path, fn);
      // rpt_vstring(d1, "PCI device path:                       %s", path);
      DO_OUTPUT(d1, title_width, "PCI device path:", path);
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

#undef DO_OUTPUT

   if (businfo->edid) {
      if (output_level == DDCA_OL_TERSE) {
         rpt_vstring(depth, "I2C bus:          /dev/"I2C"-%d", businfo->busno);
         if (businfo->drm_connector_found_by != DRM_CONNECTOR_NOT_FOUND)
            rpt_vstring(depth, "DRM connector:    %s", businfo->drm_connector_name);
         rpt_vstring(depth, "drm_connector_id: %d", businfo->drm_connector_id);
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
   RTTI_ADD_FUNC(find_sys_drm_connector_by_busno_or_edid);
   RTTI_ADD_FUNC(check_x37_for_businfo);
   RTTI_ADD_FUNC(get_connector_edid);
   RTTI_ADD_FUNC(i2c_get_device_numbers_using_udev);
   RTTI_ADD_FUNC(get_parsed_edid_for_businfo_using_sysfs);
   RTTI_ADD_FUNC(i2c_async_scan);
   RTTI_ADD_FUNC(i2c_check_bus);
   RTTI_ADD_FUNC(i2c_check_edid_exists_by_dh);
   RTTI_ADD_FUNC(i2c_check_open_bus_alive);
   RTTI_ADD_FUNC(i2c_close_bus);
   RTTI_ADD_FUNC(i2c_detect_attached_buses);
   RTTI_ADD_FUNC(i2c_detect_buses);
   RTTI_ADD_FUNC(i2c_detect_buses0);
   RTTI_ADD_FUNC(i2c_detect_single_bus);
   RTTI_ADD_FUNC(i2c_detect_x37);
   RTTI_ADD_FUNC(i2c_edid_exists);
   RTTI_ADD_FUNC(i2c_enable_cross_instance_locks);
   RTTI_ADD_FUNC(i2c_get_and_check_bus_info);
   RTTI_ADD_FUNC(i2c_non_async_scan);
   RTTI_ADD_FUNC(i2c_open_bus);
   RTTI_ADD_FUNC(i2c_report_active_bus);
   RTTI_ADD_FUNC(i2c_threaded_initial_checks_by_businfo);
   RTTI_ADD_FUNC(is_adapter_class_display_controller);
   RTTI_ADD_FUNC(is_laptop_drm_connector_name);
   RTTI_ADD_FUNC(is_laptop_for_businfo);
}


void subinit_i2c_bus_core() {
   // init_sysfs_drm_connector_names();
}


void init_i2c_bus_core() {
   init_i2c_bus_core_func_name_table();
   open_failures_reported = EMPTY_BIT_SET_256;
}

