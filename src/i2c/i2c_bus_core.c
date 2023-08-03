/** @file i2c_bus_core.c
 *
 * I2C bus detection and inspection
 */
// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef USE_X11
#include <X11/extensions/dpmsconst.h>
#endif
/** \endcond */

#include "util/coredefs_base.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/failsim.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
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
#include "base/last_io_event.h"
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
#include "i2c/i2c_strategy_dispatcher.h"
#include "i2c/i2c_sysfs.h"
#include "i2c/i2c_execute.h"
#include "i2c/i2c_edid.h"

#include "i2c/i2c_bus_core.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;


// Another ugly global variable for testing purposes
bool i2c_force_bus = false;

static GMutex  open_failures_mutex;
static Bit_Set_256 open_failures_reported;

//
// DPMS Detection
//

Dpms_State dpms_state;    // global

Value_Name_Table dpms_state_flags_table = {
      VN(DPMS_STATE_X11_CHECKED),
      VN(DPMS_STATE_X11_ASLEEP),
      VN(DPMS_SOME_DRM_ASLEEP),
      VN(DPMS_ALL_DRM_ASLEEP),
      VN_END
};

char *      interpret_dpms_state_t(Dpms_State state) {
   return VN_INTERPRET_FLAGS_T(state, dpms_state_flags_table, "|");
}


void dpms_check_x11_asleep() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   char * xdg_session_type = getenv("XDG_SESSION_TYPE");
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "XDG_SESSION_TYPE = |%s|", xdg_session_type);

#ifdef USE_X11
   if (streq(xdg_session_type, "x11")) {
      // state indicates whether or not DPMS is enabled (TRUE) or disabled (FALSE).
      // power_level indicates the current power level (one of DPMSModeOn,
      // DPMSModeStandby, DPMSModeSuspend, or DPMSModeOff.)
      unsigned short power_level;
      unsigned char state;
      bool ok =get_x11_dpms_info(&power_level, &state);
      if (ok) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "power_level=%d = %s, state=%s",
            power_level, dpms_power_level_name(power_level), sbool(state));
         if (state && (power_level != DPMSModeOn) )
            dpms_state |= DPMS_STATE_X11_ASLEEP;
         dpms_state |= DPMS_STATE_X11_CHECKED;
      }
      else {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "get_x11_dpms_info() failed.");
         SYSLOG2(DDCA_SYSLOG_ERROR, "get_x11_dpms_info() failed");
      }
   }
   // dpms_state |= DPMS_STATE_X11_ASLEEP; // testing
   // dpms_state = 0;    // testing

#endif
   DBGTRC_DONE(debug, TRACE_GROUP, "dpms_state = 0x%02x = %s", dpms_state, interpret_dpms_state_t(dpms_state));
}


bool dpms_check_drm_asleep(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = /dev/i2c-%d", businfo->busno);

   bool asleep = false;
   if (!businfo->drm_connector_name) {
      Sys_Drm_Connector * conn =  i2c_check_businfo_connector(businfo);
      if (!conn) {
        DBGTRC_NOPREFIX(debug, TRACE_GROUP, "i2c_check_businfo_connector() failed for bus %d", businfo->busno);
        SYSLOG2(DDCA_SYSLOG_ERROR, "i2c_check_businfo_connector() failed for bus %d", businfo->busno);
      }
      else {
        assert(businfo->drm_connector_name);
      }
   }
   if (businfo->drm_connector_name) {
      char * dpms = NULL;
      char * enabled = NULL;
      RPT_ATTR_TEXT(-1, &dpms,    "/sys/class/drm", businfo->drm_connector_name, "dpms");
      RPT_ATTR_TEXT(-1, &enabled, "/sys/class/drm", businfo->drm_connector_name, "enabled");
      asleep = !( streq(dpms, "On") && streq(enabled, "enabled") );
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
            "/sys/class/drm/%s/dpms=%s, /sys/class/drm/%s/enabled=%s",
            businfo->drm_connector_name, dpms, businfo->drm_connector_name, enabled);
      SYSLOG2(DDCA_SYSLOG_DEBUG,
            "/sys/class/drm/%s/dpms=%s, /sys/class/drm/%s/enabled=%s",
            businfo->drm_connector_name, dpms, businfo->drm_connector_name, enabled);
   }

   // if (businfo->busno == 6)   // test case
   //    asleep = true;

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, asleep, "");
   return asleep;
}




//
// Bus open errors
//

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


/** Adds a bus number to the set of open failures reported.
 *
 *  @param  busno     /dev/i2c-N bus number
 */
void include_open_failures_reported(int busno) {
   g_mutex_lock(&open_failures_mutex);
   open_failures_reported = bs256_insert(open_failures_reported, busno);
   g_mutex_unlock(&open_failures_mutex);
}


//
// Basic I2C bus operations
//

/** Open an I2C bus device.
 *
 *  @param busno     bus number
 *  @param callopts  call option flags, controlling failure action
 *
 *  @retval >=0     Linux file descriptor
 *  @retval -errno  negative Linux errno if open fails
 *
 *  Call options recognized
 *  - CALLOPT_ERR_MSG
 */
int i2c_open_bus(int busno, Byte callopts) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, callopts=0x%02x", busno, callopts);

   char filename[20];
   int  fd;             // Linux file descriptor

   snprintf(filename, 19, "/dev/"I2C"-%d", busno);
   RECORD_IO_EVENT(
         IE_OPEN,
         ( fd = open(filename, (callopts & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR) )
         );
   // DBGMSG("post open, fd=%d", fd);
   // returns file descriptor if successful
   // -1 if error, and errno is set
   int errsv = errno;

   if (fd < 0) {
      if (!bs256_contains(open_failures_reported, busno))
         f0printf(ferr(), "Open failed for %s: errno=%s\n", filename, linux_errno_desc(errsv));
      fd = -errsv;
   }
   else {
      RECORD_IO_FINISH_NOW(fd, IE_OPEN);
#ifdef PTD
      ptd_append_thread_description(filename);
#endif
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "busno=%d, Returning file descriptor: %d", busno, fd);
   return fd;
}


/** Closes an open I2C bus device.
 *
 * @param  fd        Linux file descriptor
 * @param  callopts  call option flags, controlling failure action
 *
 * @retval 0  success
 * @retval <0 negative Linux errno value if close fails
 */
Status_Errno i2c_close_bus(int fd, Call_Options callopts) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
          "fd=%d - %s, callopts=%s",
          fd, filename_for_fd_t(fd), interpret_call_options_t(callopts));

   Status_Errno result = 0;
   int rc = 0;

   RECORD_IO_EVENTX(fd, IE_CLOSE, ( rc = close(fd) ) );
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
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "fd=%d, filename=%s",fd, filename_for_fd_t(fd));
   return result;
}


//
// I2C Bus Inspection - Slave Addresses
//

#define IS_EDP_DEVICE(_busno) is_laptop_drm_connector(_busno, "-eDP-")
#define IS_LVDS_DEVICE(_busno) is_laptop_drm_connector(_busno, "-LVDS-")

/** Checks whether a /dev/i2c-n device represents an eDP or LVDS device,
 *  i.e. a laptop display.
 *
 *  @param  busno   i2c bus number
 *  #param  drm_name_fragment  string to look for
 *  @return true/false
 */
// Attempting to recode using opendir() and readdir() produced
// a complicated mess.  Using execute_shell_cmd_collect() is simple.
// Simplicity has its virtues.
static bool is_laptop_drm_connector(int busno, char * drm_name_fragment) {
   bool debug = false;
   // DBGMSF(debug, "Starting.  busno=%d", busno);
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "busno=%d, buf_info=%p", bus_info->busno, bus_info );

   assert(bus_info && ( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0) );

   // void i2c_bus_check_valid_name(bus_info);  // unnecessary
   assert( (bus_info->flags & I2C_BUS_EXISTS) &&
           (bus_info->flags & I2C_BUS_VALID_NAME_CHECKED) &&
           (bus_info->flags & I2C_BUS_HAS_VALID_NAME)
         );

   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      DBGMSF(debug, "Probing");
      bus_info->flags |= I2C_BUS_PROBED;
      bus_info->driver = get_driver_for_busno(bus_info->busno);
      DBGMSF(debug, "Calling i2c_open_bus..");
      int fd = i2c_open_bus(bus_info->busno, CALLOPT_ERR_MSG);
      if (fd >= 0) {
          DBGMSF(debug, "Opened bus /dev/i2c-%d", bus_info->busno);
          bus_info->flags |= I2C_BUS_ACCESSIBLE;

          bus_info->functionality = i2c_get_functionality_flags_by_fd(fd);

          DDCA_Status ddcrc = i2c_get_parsed_edid_by_fd(fd, &bus_info->edid);
          if (bus_info->busno == 6 || bus_info->busno == 8) {
             ddcrc = -EBUSY;
             bus_info->edid = NULL;
          }
          DBGMSF(debug, "i2c_get_parsed_edid_by_fd() returned %s", psc_desc(ddcrc));
          if (ddcrc == 0) {
             bus_info->flags |= I2C_BUS_ADDR_0X50;
             if ( IS_EDP_DEVICE(bus_info->busno) ) {
                DBGMSF(debug, "eDP device detected");
                bus_info->flags |= I2C_BUS_EDP;
             }
             else if ( IS_LVDS_DEVICE(bus_info->busno) ) {
                DBGMSF(debug, "LVDS device detected");
                bus_info->flags |= I2C_BUS_LVDS;
             }
             else {
                // The check here for slave address x37 had previously been removed.
                // It was commented out in commit 78fb4b on 4/29/2013, and the code
                // finally delete by commit f12d7a on 3/20/2020, with the following
                // comments:
                //    have seen case where laptop display reports addr 37 active, but
                //    it doesn't respond to DDC
                // 8/2017: If DDC turned off on U3011 monitor, addr x37 still detected
                // DDC checking was therefore moved entierly to the DDC layer.
                // 6/25/2023:
                // Testing for slave address x37 turns out to be needed to avoid
                // trying to reload cached display information for a display no
                // longer present
                int rc = i2c_detect_x37(fd);
                if (rc == 0)
                   bus_info->flags |= I2C_BUS_ADDR_0X37;
                else if (rc == -EBUSY)
                   bus_info->flags |= I2C_BUS_BUSY;
             }
          }
          else if (ddcrc == -EBUSY) {
             bus_info->flags |= I2C_BUS_BUSY;
          }

          DBGMSF(debug, "Closing bus...");
          i2c_close_bus(fd, CALLOPT_ERR_MSG);
      }
      else {
         bus_info->open_errno = -errno;
      }

      if (bus_info->flags & I2C_BUS_BUSY) {
         DBGMSF(debug, "Getting EDID from sysfs");
         Sys_Drm_Connector * connector_rec = find_sys_drm_connector_by_busno(bus_info->busno);
         if (connector_rec && connector_rec->edid_bytes) {
            bus_info->edid = create_parsed_edid2(connector_rec->edid_bytes, "SYSFS");
            if (debug) {
               if (bus_info->edid)
                  report_parsed_edid(bus_info->edid, false /* verbose */, 0);
               else
                  DBGMSG("create_parsed_edid() returned NULL");
            }
            if (bus_info->edid) {
               bus_info->flags |= I2C_BUS_ADDR_0X50;  // ???
               bus_info->flags |= I2C_BUS_SYSFS_EDID;
               memcpy(bus_info->edid->edid_source, "SYSFS", 6);
            }
         }
      }
   }   // probing complete

   // DBGTRC_DONE(debug, TRACE_GROUP, "flags=0x%04x, bus info:", bus_info->flags );
   if (debug || IS_TRACING() ) {
      char * sflags = interpret_i2c_bus_flags(bus_info->flags);
      DBGTRC_DONE(true, TRACE_GROUP, "flags = 0x%08x = %s", bus_info->flags, sflags);
      // DBGTRC_NOPREFIX(true, TRACE_GROUP, "flags = %s", interpret_i2c_bus_flags_t(bus_info->flags));
      free(sflags);
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "bus_info:");
      i2c_dbgrpt_bus_info(bus_info, 2);
   }
}

// called if display removed
void i2c_reset_bus_info(I2C_Bus_Info * bus_info) {
   bool debug = false;
   assert(bus_info);
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno = %d", bus_info, bus_info->busno);
   bus_info->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
   if (bus_info->edid) {
      free_parsed_edid(bus_info->edid);
      bus_info->edid = NULL;
   }
   if (debug || IS_TRACING()) {
      DBGTRC_DONE(true, TRACE_GROUP, "Final bus_info:");
      i2c_dbgrpt_bus_info(bus_info, 2);
   }
}


Sys_Drm_Connector * i2c_check_businfo_connector(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "Checking I2C_Bus_Info for /dev/i2c-%d", businfo->busno);
   businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_FOUND;
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(businfo->busno);
   if (drm_connector) {
     businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
     businfo->drm_connector_name = g_strdup(drm_connector->connector_name);
   }
   else if (businfo->edid) {
     drm_connector = find_sys_drm_connector_by_edid(businfo->edid->bytes);
     if (drm_connector) {
        businfo->drm_connector_name = g_strdup(drm_connector->connector_name);
        businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_EDID;
     }
   }
   businfo->flags |= I2C_BUS_DRM_CONNECTOR_CHECKED;
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      char * s = interpret_i2c_bus_flags(businfo->flags);
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Final businfo flags: %s", s);
      free(s);
   }
   if (drm_connector)
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning: SYS_Drm_Connector for %s", drm_connector->connector_name);
   else
      DBGTRC_RETURNING(debug, TRACE_GROUP, NULL, "");
   return drm_connector;
}


char * i2c_get_drm_connector_name(I2C_Bus_Info * businfo) {
   if (!(businfo->flags & I2C_BUS_DRM_CONNECTOR_CHECKED) ) {
      i2c_check_businfo_connector(businfo);
   }
   return businfo->drm_connector_name;
}



/** Reports a single active display.
 *
 * Output is written to the current report destination.
 *
 * @param   businfo     bus record
 * @param   depth       logical indentation depth
 *
 * @remark
 * This function is used by detect, interrogate commands, C API
 */
void i2c_report_active_display(I2C_Bus_Info * businfo, int depth) {
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
      rpt_vstring(d,
                          "%-*s%s", title_width, "DRM connector:",
                          (businfo->drm_connector_name)
                               ? businfo->drm_connector_name
                               : "Not found"
                 );
      if (output_level >= DDCA_OL_VERBOSE) {
      if (businfo->drm_connector_name) {

         char buf[100];
         int tw = title_width; // 35;  // title_width;
         g_snprintf(buf, 100, "/sys/class/drm/%s/dpms", businfo->drm_connector_name);
         char * s = file_get_first_line(buf, false);
         if (s) {
            strcat(buf, ":");
            rpt_vstring(d, "%-*s%s", tw, buf, s);
            free(s);
         }
         g_snprintf(buf, 100, "/sys/class/drm/%s/enabled", businfo->drm_connector_name);
         s = file_get_first_line(buf, false);
         if (s) {
            strcat(buf, ":");
            rpt_vstring(d, "%-*s%s", tw, buf, s);
            free(s);
         }
         g_snprintf(buf, 100, "/sys/class/drm/%s/status", businfo->drm_connector_name);
         s = file_get_first_line(buf, false);
         if (s) {
            strcat(buf, ":");
            rpt_vstring(d, "%-*s%s", tw, buf, s);
            free(s);
         }
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
      rpt_vstring(d1, "Is eDP device:                         %-5s", sbool(businfo->flags & I2C_BUS_EDP));
      rpt_vstring(d1, "Is LVDS device:                        %-5s", sbool(businfo->flags & I2C_BUS_LVDS));

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


//
// Simple Bus Detection
//

/** Checks if an I2C bus with a given number exists.
 *
 * @param   busno     bus number
 *
 * @return  true/false
 */
bool i2c_device_exists(int busno) {
   bool result = false;
   bool debug = false;
   int  errsv;
   char namebuf[20];
   struct stat statbuf;
   int  rc = 0;
   sprintf(namebuf, "/dev/"I2C"-%d", busno);
   errno = 0;
   rc = stat(namebuf, &statbuf);
   errsv = errno;
   if (rc == 0) {
      DBGMSF(debug, "Found %s", namebuf);
      result = true;
    }
    else {
        DBGMSF(debug,  "stat(%s) returned %d, errno=%s",
                                   namebuf, rc, linux_errno_desc(errsv) );
    }

    DBGMSF(debug, "busno=%d, returning %s", busno, sbool(result) );
   return result;
}


/** Returns the number of I2C buses on the system, by looking for
 *  devices named /dev/i2c-n.
 *
 *  Note that no attempt is made to open the devices.
 */
int i2c_device_count() {
   bool debug = false;
   int  busct = 0;

   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno))
         busct++;
   }
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Returning %d", busct );
   return busct;
}


//
// Bus inventory
//

/** Gets a list of all /dev/i2c devices by checking the file system
 *  if devices named /dev/i2c-N exist.
 *
 *  @return Byte_Value_Array containing the valid bus numbers
 */
Byte_Value_Array get_i2c_devices_by_existence_test() {
   Byte_Value_Array bva = bva_create();
   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno)) {
         // if (!is_ignorable_i2c_device(busno))
         bva_append(bva, busno);
      }
   }
   return bva;
}


int i2c_detect_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "i2c_buses = %p", i2c_buses);

   // rpt_label(0, "*** Temporary code to exercise get_all_i2c_infos() ***");
   // GPtrArray * i2c_infos = get_all_i2c_info(true, -1);
   // dbgrpt_all_sysfs_i2c_info(i2c_infos, 2);

   if (!i2c_buses) {

#ifdef ENABLE_UDEV
      // do not include devices with ignorable name, etc.:
      Byte_Value_Array i2c_bus_bva =
            get_i2c_device_numbers_using_udev(/*include_ignorable_devices=*/ false);
#else
      Byte_Value_Array i2c_bus_bva = get_i2c_devices_by_existence_test();
#endif
      i2c_buses = g_ptr_array_sized_new(bva_length(i2c_bus_bva));
      g_ptr_array_set_free_func(i2c_buses, i2c_gdestroy_bus_info);
      for (int ndx = 0; ndx < bva_length(i2c_bus_bva); ndx++) {
         int busno = bva_get(i2c_bus_bva, ndx);
         DBGMSF(debug, "Checking busno = %d", busno);
         I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
         businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
         i2c_check_bus(businfo);
         if (debug || IS_TRACING() )
            i2c_dbgrpt_bus_info(businfo, 0);
         if (debug) {
            GPtrArray * conflicts = collect_conflicting_drivers(busno, -1);
            // report_conflicting_drivers(conflicts);
            DBGMSG("Conflicting drivers: %s", conflicting_driver_names_string_t(conflicts));
            free_conflicting_drivers(conflicts);
         }
         DBGMSF(debug, "Valid bus: /dev/"I2C"-%d", busno);
         g_ptr_array_add(i2c_buses, businfo);
      }
      bva_free(i2c_bus_bva);
   }
   int result = i2c_buses->len;
   DBGTRC_DONE(debug, DDCA_TRC_I2C, "Returning: %d", result);
   return result;
}


void i2c_discard_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   if (i2c_buses) {
      g_ptr_array_free(i2c_buses, true);
      i2c_buses= NULL;
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


I2C_Bus_Info * i2c_detect_single_bus(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno = %d", busno);
   I2C_Bus_Info * businfo = NULL;

   if (i2c_device_exists(busno) ) {
      if (!i2c_buses) {
         i2c_buses = g_ptr_array_sized_new(1);
         g_ptr_array_set_free_func(i2c_buses, i2c_gdestroy_bus_info);
      }
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
      i2c_check_bus(businfo);
      if (debug)
         i2c_dbgrpt_bus_info(businfo, 0);
      g_ptr_array_add(i2c_buses, businfo);
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



static void init_i2c_bus_core_func_name_table() {
   RTTI_ADD_FUNC(dpms_check_x11_asleep);
   RTTI_ADD_FUNC(dpms_check_drm_asleep);
   RTTI_ADD_FUNC(i2c_check_bus);
   RTTI_ADD_FUNC(i2c_check_businfo_connector);
   RTTI_ADD_FUNC(i2c_close_bus);
   RTTI_ADD_FUNC(i2c_detect_buses);
   RTTI_ADD_FUNC(i2c_detect_single_bus);
   RTTI_ADD_FUNC(i2c_detect_x37);
   RTTI_ADD_FUNC(i2c_discard_buses);
   RTTI_ADD_FUNC(i2c_open_bus);
   RTTI_ADD_FUNC(i2c_report_active_display);
}


void init_i2c_bus_core() {
   init_i2c_bus_core_func_name_table();
   open_failures_reported = EMPTY_BIT_SET_256;
}

