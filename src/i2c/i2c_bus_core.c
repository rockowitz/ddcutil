/** @file i2c_bus_core.c
 *
 * I2C bus detection and inspection
 */
// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
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
/** \endcond */

#include "util/debug_util.h"
#include "util/failsim.h"
#include "util/file_util.h"
#include "util/glib_string_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/edid.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
#ifdef ENABLE_UDEV
#include "util/udev_i2c_util.h"
#endif
#include "util/utilrpt.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/last_io_event.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/tuned_sleep.h"
#include "base/per_thread_data.h"

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

/** All I2C buses.  GPtrArray of pointers to #I2C_Bus_Info - shared with i2c_bus_selector.c */
/* static */ GPtrArray * i2c_buses = NULL;

#ifdef OLD
/** Global variable.  Controls whether function #i2c_set_addr() attempts retry
 *  after EBUSY error by changing ioctl op I2C_SLAVE to I2C_SLAVE_FORCE.
 */
bool i2c_force_slave_addr_flag = false;
#endif

// Another ugly global variable for testing purposes

bool i2c_force_bus = false;

static GMutex  open_failures_mutex;
static Bit_Set_256 open_failures_reported;

//
// Local utility functions
//

/** Creates a string interpretation of I2C_Bus_Info.flags.
 *
 *  @param  flags flags value
 *  @return string interpretation, caller must free
 *
 *  @remark
 *  Keep the names in sync with flag definitions
 */
static char * interpret_i2c_bus_flags(uint16_t flags) {
   GPtrArray * names = g_ptr_array_new();

#define ADD_NAME(_name) \
   if (_name & flags) g_ptr_array_add(names, #_name)

   ADD_NAME(I2C_BUS_EXISTS             );
   ADD_NAME(I2C_BUS_ACCESSIBLE         );
   ADD_NAME(I2C_BUS_ADDR_0X50          );
   ADD_NAME(I2C_BUS_ADDR_0X37          );
   ADD_NAME(I2C_BUS_ADDR_0X30          );
   ADD_NAME(I2C_BUS_EDP                );
   ADD_NAME(I2C_BUS_LVDS               );
   ADD_NAME(I2C_BUS_PROBED             );
   ADD_NAME(I2C_BUS_VALID_NAME_CHECKED );
   ADD_NAME(I2C_BUS_HAS_VALID_NAME     );
   ADD_NAME(I2C_BUS_BUSY               );
   ADD_NAME(I2C_BUS_SYSFS_EDID         );

#undef ADD_NAME

   char * joined =  join_string_g_ptr_array(names, " | ");
   return joined;
}


#ifdef NOT_WORTH_THE_SPACE
char * interpret_i2c_bus_flags_t(uint16_t flags) {
   static GPrivate  buffer_key = G_PRIVATE_INIT(g_free);
   static GPrivate  buffer_len_key = G_PRIVATE_INIT(g_free);

   char * sflags = interpret_i2c_bus_flags(flags);
   int required_size = strlen(sflags) + 1;
   char * buf = get_thread_dynamic_buffer(&buffer_key, &buffer_len_key, required_size);
   strncpy(buf, sflags, required_size);
   free(sflags);
   return buf;
}
#endif


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
   bool debug = true;
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
      ptd_append_thread_description(filename);
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
   bool debug = true;
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

#ifdef OLD
/** Sets I2C slave address to be used on subsequent calls
 *
 * @param  fd        Linux file descriptor for open /dev/i2c-n
 * @param  addr      slave address
 * @param  callopts  call option flags, controlling failure action\n
 *                   if CALLOPT_FORCE set, use IOCTL op I2C_SLAVE_FORCE
 *                   to take control even if address is in use by another driver
 *
 * @retval  0 if success
 * @retval <0 negative Linux errno, if ioctl call fails
 *
 * \remark
 * Errors which are recovered are counted here using COUNT_STATUS_CODE().
 * The final status code is left for the caller to count
 */
Status_Errno i2c_set_addr(int fd, int addr, Call_Options callopts) {
   bool debug = false;
#ifdef FOR_TESTING
   bool force_i2c_slave_failure = false;
#endif
   // callopts |= CALLOPT_ERR_MSG;    // temporary
   DBGTRC_STARTING(debug, TRACE_GROUP,
                 "fd=%d, addr=0x%02x, filename=%s, i2c_force_slave_addr_flag=%s, callopts=%s",
                 fd, addr,
                 filename_for_fd_t(fd),
                 sbool(i2c_force_slave_addr_flag),
                 interpret_call_options_t(callopts) );
   // FAILSIM;

   Status_Errno result = 0;
   int rc = 0;
   int errsv = 0;
   uint16_t op = (callopts & CALLOPT_FORCE_SLAVE_ADDR) ? I2C_SLAVE_FORCE : I2C_SLAVE;

retry:
   errno = 0;
   RECORD_IO_EVENT( IE_OTHER, ( rc = ioctl(fd, op, addr) ) );
#ifdef FOR_TESTING
   if (force_i2c_slave_failure) {
      if (op == I2C_SLAVE) {
         DBGMSG("Forcing pseudo failure");
         rc = -1;
         errno=EBUSY;
      }
   }
#endif
   errsv = errno;

   if (rc < 0) {
      if (errsv == EBUSY) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "ioctl(%s, I2C_SLAVE, 0x%02x) returned EBUSY",
                   filename_for_fd_t(fd), addr);

         if (op == I2C_SLAVE &&
               i2c_force_slave_addr_flag )  // global setting
             // future?: (i2c_force_slave_addr_flag || (callopts & CALLOPT_FORCE_SLAVE_ADDR)) )
         {
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "Retrying using IOCTL op I2C_SLAVE_FORCE for %s, slave address 0x%02x",
                   filename_for_fd_t(fd), addr );
            // normally errors counted at higher level, but in this case it would be
            // lost because of retry
            COUNT_STATUS_CODE(-errsv);
            op = I2C_SLAVE_FORCE;
            // debug = true;   // force final message for clarity
            goto retry;
         }
      }
      else {
         REPORT_IOCTL_ERROR( (op == I2C_SLAVE) ? "I2C_SLAVE" : "I2C_SLAVE_FORCE", errsv);
      }

      result = -errsv;
   }
   if (result == -EBUSY) {
      char msgbuf[60];
      g_snprintf(msgbuf, 60, "set_addr(%s,%s,0x%02x) failed, error = EBUSY",
                             filename_for_fd_t(fd),
                             (op == I2C_SLAVE) ? "I2C_SLAVE" : "I2C_SLAVE_FORCE",
                             addr);
      DBGTRC(true, TRACE_GROUP, "%s", msgbuf);
      syslog(LOG_ERR, "%s", msgbuf);

   }
   else if (result == 0 && op == I2C_SLAVE_FORCE) {
      char msgbuf[80];
      g_snprintf(msgbuf, 80, "set_addr(%s,I2C_SLAVE_FORCE,0x%02x) succeeded on retry after EBUSY error",
            filename_for_fd_t(fd),
            addr);
      DBGTRC(debug || get_output_level() > DDCA_OL_VERBOSE, TRACE_GROUP, "%s", msgbuf);
      syslog(LOG_INFO, "%s", msgbuf);
   }

   assert(result <= 0);
   // if (addr == 0x37)  result = -EBUSY;    // for testing
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "");
   return result;
}
#endif


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

   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "busno=%d, drm_name_fragment |%s|, Returning: %s",
                              busno, drm_name_fragment, sbool(result));
   return result;
}


//
// I2C Bus Inspection - Fill in and report Bus_Info
//

/** Allocates and initializes a new #I2C_Bus_Info struct
 *
 * @param busno I2C bus number
 * @return newly allocated #I2C_Bus_Info
 */
static I2C_Bus_Info * i2c_new_bus_info(int busno) {
   I2C_Bus_Info * businfo = calloc(1, sizeof(I2C_Bus_Info));
   memcpy(businfo->marker, I2C_BUS_INFO_MARKER, 4);
   businfo->busno = busno;
   return businfo;
}


static Status_Errno_DDC
i2c_detect_x37(int fd) {
   bool debug = true;
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
   bool debug = true;
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
                int rc = i2c_detect_x37(fd);
                if (rc == 0)
                   bus_info->flags |= I2C_BUS_ADDR_0X37;
                else if (rc == -EBUSY)
                   bus_info->flags |= I2C_BUS_BUSY;
             }
          }
          else if (ddcrc == -EBUSY)
             bus_info->flags |= I2C_BUS_BUSY;

          DBGMSF(debug, "Closing bus...");
          i2c_close_bus(fd, CALLOPT_ERR_MSG);
      }
      else {
         bus_info->open_errno = -errno;
      }
   }   // probing complete

   // DBGTRC_DONE(debug, TRACE_GROUP, "flags=0x%04x, bus info:", bus_info->flags );
   if (debug || IS_TRACING() ) {
      char * sflags = interpret_i2c_bus_flags(bus_info->flags);
      DBGTRC_DONE(true, TRACE_GROUP, "flags = 0x%04x = %s", bus_info->flags, sflags);
      // DBGTRC_NOPREFIX(true, TRACE_GROUP, "flags = %s", interpret_i2c_bus_flags_t(bus_info->flags));
      free(sflags);
      DBGTRC_NOPREFIX(true, TRACE_GROUP, "bus_info:");
      i2c_dbgrpt_bus_info(bus_info, 2);
   }
}


void i2c_free_bus_info(I2C_Bus_Info * bus_info) {
   bool debug = false;
   DBGMSF(debug, "bus_info = %p", bus_info);
   if (bus_info) {
      if (memcmp(bus_info->marker, "BINx", 4) != 0) {   // just ignore if already freed
         assert( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0);
         if (bus_info->edid)
            free_parsed_edid(bus_info->edid);
         if (bus_info->driver)
            free(bus_info->driver);
         bus_info->marker[3] = 'x';
         free(bus_info);
      }
   }
}

// satisfies GDestroyNotify()
void i2c_gdestroy_bus_info(gpointer data) {
   i2c_free_bus_info((I2C_Bus_Info*) data);
}


//
// Bus Reports
//

/** Reports on a single I2C bus.
 *
 *  \param   bus_info    pointer to Bus_Info structure describing bus
 *  \param   depth       logical indentation depth
 *
 *  \remark
 *  Although this is a debug type report, it is called by used (indirectly) by the
 *  ENVIRONMENT command.
 */
void i2c_dbgrpt_bus_info(I2C_Bus_Info * bus_info, int depth) {
   bool debug = false;
   DBGMSF(debug, "bus_info=%p", bus_info);
   assert(bus_info);

   rpt_vstring(depth, "Bus /dev/i2c-%d found:   %s", bus_info->busno, sbool(bus_info->flags&I2C_BUS_EXISTS));
   rpt_vstring(depth, "Bus /dev/i2c-%d probed:  %s", bus_info->busno, sbool(bus_info->flags&I2C_BUS_PROBED ));
   if ( bus_info->flags & I2C_BUS_PROBED ) {
      rpt_vstring(depth, "Driver:                  %s", bus_info->driver);
      rpt_vstring(depth, "Bus accessible:          %s", sbool(bus_info->flags&I2C_BUS_ACCESSIBLE ));
      rpt_vstring(depth, "Bus is eDP:              %s", sbool(bus_info->flags&I2C_BUS_EDP ));
      rpt_vstring(depth, "Bus is LVDS:             %s", sbool(bus_info->flags&I2C_BUS_LVDS));
      rpt_vstring(depth, "Valid bus name checked:  %s", sbool(bus_info->flags & I2C_BUS_VALID_NAME_CHECKED));
      rpt_vstring(depth, "I2C bus has valid name:  %s", sbool(bus_info->flags & I2C_BUS_HAS_VALID_NAME));
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(depth, "Address 0x30 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X30));
#endif
      rpt_vstring(depth, "Address 0x37 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(depth, "Device busy:             %s", sbool(bus_info->flags & I2C_BUS_BUSY));
      rpt_vstring(depth, "errno for open:          %d", bus_info->open_errno);
      // not useful and clutters the output
      // i2c_report_functionality_flags(bus_info->functionality, /* maxline */ 90, depth);
      if ( bus_info->flags & I2C_BUS_ADDR_0X50) {
         if (bus_info->edid) {
            report_parsed_edid(bus_info->edid, true /* verbose */, depth);
         }
      }
   }

#ifndef TARGET_BSD
   I2C_Sys_Info * info = get_i2c_sys_info(bus_info->busno, -1);
   dbgrpt_i2c_sys_info(info, depth);
   free_i2c_sys_info(info);
#endif

   DBGMSF(debug, "Done");
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
   DBGMSF(debug, "Starting.  businfo=%p", businfo);
   assert(businfo);

   int d1 = depth+1;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_NORMAL)
      rpt_vstring(depth, "I2C bus:  /dev/"I2C"-%d", businfo->busno);
   // will work for amdgpu, maybe others
   Sys_Drm_Connector * drm_connector = find_sys_drm_connector_by_busno(businfo->busno);
   if (!drm_connector && businfo->edid)
      drm_connector = find_sys_drm_connector_by_edid(businfo->edid->bytes);
   int title_width = (output_level >= DDCA_OL_VERBOSE) ? 36 : 25;
   if (drm_connector && output_level >= DDCA_OL_NORMAL)
      rpt_vstring((output_level >= DDCA_OL_VERBOSE) ? d1 : depth, "%-*s%s", title_width, "DRM connector:", drm_connector->connector_name);

   // 08/2018 Disable.
   // Test for DDC communication is now done more sophisticatedly at the DDC level
   // The simple X37 test can have both false positives (DDC turned off in monitor but
   // X37 responsive), and false negatives (Dell P2715Q)
   // if (output_level >= DDCA_OL_NORMAL)
   // rpt_vstring(depth, "Supports DDC:    %s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level >= DDCA_OL_VERBOSE) {
      rpt_vstring(d1, "Driver:                             %s", (businfo->driver) ? businfo->driver : "Unknown");
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(d1, "I2C address 0x30 (EDID block#)  present: %-5s", srepr(businfo->flags & I2C_BUS_ADDR_0X30));
      rpt_vstring(d1, "I2C address 0x37 (DDC)          present: %-5s", srepr(businfo->flags & I2C_BUS_ADDR_0X37));
#endif
      rpt_vstring(d1, "I2C address 0x50 (EDID) responsive: %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(d1, "Is eDP device:                      %-5s", sbool(businfo->flags & I2C_BUS_EDP));
      rpt_vstring(d1, "Is LVDS device:                     %-5s", sbool(businfo->flags & I2C_BUS_LVDS));

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
      rpt_vstring(d1, "PCI device path:                    %s", path);
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
         if (drm_connector)
            rpt_vstring(depth, "DRM connector:    %s", drm_connector->connector_name);
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
   DBGMSF(debug, "Done.");
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
      // only returns buses with valid name (arg=false)
#ifdef ENABLE_UDEV
      Byte_Value_Array i2c_bus_bva = get_i2c_device_numbers_using_udev(false);
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
         if (businfo->flags & I2C_BUS_BUSY) {
            DBGMSF(debug, "Getting EDID from sysfs");
            Sys_Drm_Connector * connector_rec = find_sys_drm_connector_by_busno(busno);
            if (connector_rec && connector_rec->edid_bytes) {
               businfo->edid = create_parsed_edid2(connector_rec->edid_bytes, "SYSFS");
               if (debug) {
                  if (businfo->edid)
                     report_parsed_edid(businfo->edid, false /* verbose */, 0);
                  else
                     DBGMSG("create_parsed_edid() returned NULL");
               }
               if (businfo->edid) {
                  businfo->flags |= I2C_BUS_ADDR_0X50;  // ???
                  businfo->flags |= I2C_BUS_SYSFS_EDID;
                  memcpy(businfo->edid->edid_source, "SYSFS", 6);
               }
            }

            if (debug) {
               GPtrArray * conflicts = collect_conflicting_drivers(busno, -1);
               // report_conflicting_drivers(conflicts);
               DBGMSG("Conflicting drivers: %s", conflicting_driver_names_string_t(conflicts));
               free_conflicting_drivers(conflicts);
            }
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
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
      i2c_check_bus(businfo);
      if (debug)
         i2c_dbgrpt_bus_info(businfo, 0);
   }

   DBGTRC_DONE(debug, DDCA_TRC_I2C, "busno=%d, returning: %p", busno, businfo);
   return businfo;
}


//
// Bus_Info retrieval
//

// Simple Bus_Info retrieval

/** Retrieves bus information by its index in the i2c_buses array
 *
 * @param   busndx
 *
 * @return  pointer to Bus_Info struct for the bus,\n
 *          NULL if invalid index
 */
I2C_Bus_Info * i2c_get_bus_info_by_index(uint busndx) {
   assert(i2c_buses);
   assert(busndx < i2c_buses->len);

   bool debug = false;
   DBGMSF(debug, "Starting.  busndx=%d", busndx );
   I2C_Bus_Info * bus_info = g_ptr_array_index(i2c_buses, busndx);
   // report_businfo(busInfo);
   if (debug) {
      char * s = interpret_i2c_bus_flags(bus_info->flags);
      DBGMSG("busno=%d, flags = 0x%04x = %s", bus_info->busno, bus_info->flags, s);
      free(s);
   }
   assert( bus_info->flags & I2C_BUS_PROBED );
   DBGMSF(debug, "busndx=%d, busno=%d, returning %p", busndx, bus_info->busno, bus_info );
   return bus_info;
}


/** Retrieves bus information by I2C bus number.
 *
 * If the bus information does not already exist in the #I2C_Bus_Info struct for the
 * bus, it is calculated by calling check_i2c_bus()
 *
 * @param   busno    bus number
 *
 * @return  pointer to Bus_Info struct for the bus,\n
 *          NULL if invalid bus number
 */
I2C_Bus_Info * i2c_find_bus_info_by_busno(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   assert(i2c_buses);   // fails if using temporary dref
   I2C_Bus_Info * result = NULL;
   for (int ndx = 0; ndx < i2c_buses->len; ndx++) {
      I2C_Bus_Info * cur_info = g_ptr_array_index(i2c_buses, ndx);
      if (cur_info->busno == busno) {
         result = cur_info;
         break;
      }
   }

   DBGMSF(debug, "Done.     Returning: %p", result);
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


/** Reports I2C buses.
 *
 * @param report_all    if false, only reports buses with monitors,\n
 *                      if true, reports all detected buses
 * @param depth         logical indentation depth
 *
 * @return count of reported buses
 *
 * @remark
 * Used by query-sysenv.c, always OL_VERBOSE
 */
int i2c_dbgrpt_buses(bool report_all, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "report_all=%s", sbool(report_all));

   assert(i2c_buses);
   int busct = i2c_buses->len;
   int reported_ct = 0;

   puts("");
   if (report_all)
      rpt_vstring(depth,"Detected %d non-ignorable I2C buses:", busct);
   else
      rpt_vstring(depth, "I2C buses with monitors detected at address 0x50:");

   for (int ndx = 0; ndx < busct; ndx++) {
      I2C_Bus_Info * busInfo = g_ptr_array_index(i2c_buses, ndx);
      if ( (busInfo->flags & I2C_BUS_ADDR_0X50) || report_all) {
         rpt_nl();
         i2c_dbgrpt_bus_info(busInfo, depth);
         reported_ct++;
      }
   }
   if (reported_ct == 0)
      rpt_vstring(depth, "   No buses\n");

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d", reported_ct);
   return reported_ct;
}


static void init_i2c_bus_core_func_name_table() {
   RTTI_ADD_FUNC(i2c_open_bus);
   RTTI_ADD_FUNC(i2c_close_bus);
   RTTI_ADD_FUNC(i2c_detect_buses);
   RTTI_ADD_FUNC(i2c_detect_single_bus);
   RTTI_ADD_FUNC(i2c_check_bus);
   RTTI_ADD_FUNC(i2c_detect_x37);
#ifdef OLD
   RTTI_ADD_FUNC(i2c_set_addr);
#endif
}


void init_i2c_bus_core() {
   init_i2c_bus_core_func_name_table();
   init_i2c_execute_func_name_table();
   init_i2c_strategy_func_name_table();
   open_failures_reported = EMPTY_BIT_SET_256;
}

