/** \file i2c_bus_core.c
 *
 * I2C bus detection and inspection
 */
// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <i2c/i2c_strategy_dispatcher.h>
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
#include "util/sysfs_util.h"
#ifdef USE_UDEV
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
#include "i2c/i2c_sysfs.h"

#include "i2c/i2c_bus_core.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

/** All I2C buses.  GPtrArray of pointers to #I2C_Bus_Info - shared with i2c_bus_selector.c */
/* static */ GPtrArray * i2c_buses = NULL;

/** Global variable.  Controls whether function #i2c_set_addr() attempts retry
 *  after EBUSY error by changing ioctl op I2C_SLAVE to I2C_SLAVE_FORCE.
 */
bool i2c_force_slave_addr_flag = false;


// Another ugly global variable for testing purposes

bool i2c_force_bus = false;

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
   DBGTRC(debug, TRACE_GROUP, "Starting. busno=%d, callopts=0x%02x", busno, callopts);

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
      f0printf(ferr(), "Open failed for %s: errno=%s\n", filename, linux_errno_desc(errsv));
      fd = -errsv;
   }
   else {
      RECORD_IO_FINISH_NOW(fd, IE_OPEN);
      ptd_append_thread_description(filename);
   }

   DBGTRC(debug, TRACE_GROUP, "Done.     busno=%d, Returning file descriptor: %d", busno, fd);
   return fd;
}


/** Closes an open I2C bus device.
 *
 * @param  fd        Linux file descriptor
 * @param  callopts  call option flags, controlling failure action
 *
 * @retval 0  success
 * @retval <0 negative Linux errno value if close
 */
Status_Errno i2c_close_bus(int fd, Call_Options callopts) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting. fd=%d - %s, callopts=%s",
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
   DBGTRC(debug, TRACE_GROUP, "Done.     fd=%d, filename=%s, Returning: %s",
                              fd, filename_for_fd_t(fd), psc_desc(result));
   return result;
}


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
   callopts |= CALLOPT_ERR_MSG;    // temporary
   DBGTRC(debug, TRACE_GROUP,
                 "fd=%d, addr=0x%02x, i2c_force_slave_addr_flag=%s, callopts=%s",
                 fd, addr,
                 sbool(i2c_force_slave_addr_flag),
                 interpret_call_options_t(callopts) );
   // FAILSIM;

   Status_Errno result = 0;
   int rc = 0;
   int errsv = 0;
   uint16_t op = I2C_SLAVE;

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
      if ( callopts & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR( (op == I2C_SLAVE) ? "I2C_SLAVE" : "I2C_SLAVE_FORCE", errno);

      if (errsv == EBUSY && i2c_force_slave_addr_flag && op == I2C_SLAVE) {
         DBGMSG("Retrying using IOCTL op I2C_SLAVE_FORCE for address 0x%02x", addr );
         // normally errors counted at higher level, but in this case it would be lost because of retry
         COUNT_STATUS_CODE(-errsv);
         op = I2C_SLAVE_FORCE;
         debug = true;   // force final message for clarity
         goto retry;
      }

      result = -errsv;
   }

   DBGTRC((result || debug), TRACE_GROUP,
           "addr = 0x%02x. Returning %s", addr, psc_desc(result));
      // show_backtrace(1);

   assert(result <= 0);
   return result;
}


//
// I2C Bus Inspection - Slave Addresses
//

/** Checks whether a /dev/i2c-n device represents an dDP device,
 *  i.e. a laptop display.
 *
 *  @param  busno   i2c bus number
 *  @return true/false
 */
// Attempting to recode using opendir() and readdir() produced
// a complicated mess.  Using execute_shell_cmd_collect() is simple.
// Simplicity has its virtues.
static bool is_edp_device(int busno) {
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
         if (strstr(s, "-eDP-")) {
            result = true;
            break;
         }
      }
      g_ptr_array_free(lines, true);
   }

   DBGTRC(debug, TRACE_GROUP, "busno=%d, returning: %s", busno, sbool(result));
   return result;
}


#ifdef UNUSED
/* Checks each address on an I2C bus to see if a device exists.
 * The bus device has already been opened.
 *
 * Arguments:
 *   fd  file descriptor for open bus object
 *
 * Returns:
 *   128 byte array of booleans, byte n is true iff a device is
 *   detected at bus address n
 *
 * This "exploratory" function is not currently used but is
 * retained for diagnostic purposes.
 *
 * TODO: exclude reserved I2C bus addresses from check
 */
static
bool * i2c_detect_all_slave_addrs_by_fd(int fd) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d", fd);
   assert (fd >= 0);
   bool * addrmap = NULL;

   unsigned char byte_to_write = 0x00;
   int addr;
   addrmap = calloc(I2C_SLAVE_ADDR_MAX, sizeof(bool));
   //bool addrmap[128] = {0};

   for (addr = 3; addr < I2C_SLAVE_ADDR_MAX; addr++) {
      int rc;
      i2c_set_addr(fd, addr, CALLOPT_ERR_MSG);
      rc = invoke_i2c_reader(fd, 1, &byte_to_write);
      if (rc >= 0)
         addrmap[addr] = true;
   }

   DBGMSF(debug, "Returning %p", addrmap);
   return addrmap;
}


/* Examines all possible addresses on an I2C bus.
 *
 * Arguments:
 *    busno    bus number
 *
 * Returns:
 *   128 byte boolean array,
 *   NULL if unable to open I2C bus
 *
 * This "exploratory" function is not currently used but is
 * retained for diagnostic purposes.
 */
bool * i2c_detect_all_slave_addrs(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   int file = i2c_open_bus(busno, CALLOPT_ERR_MSG);  // return if failure
   bool * addrmap = NULL;

   if (file >= 0) {
      addrmap = i2c_detect_all_slave_addrs_by_fd(file);
      i2c_close_bus(file, busno, CALLOPT_NONE);
   }

   DBGMSF(debug, "Returning %p", addrmap);
   return addrmap;
}
#endif


//
// I2C Bus Inspection - EDID Retrieval
//

static Status_Errno_DDC
i2c_get_edid_bytes_directly(int fd, Buffer* rawedid, bool read_bytewise)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Getting EDID. File descriptor = %d, filename=%s, read_bytewise=%s",
                 fd, filename_for_fd_t(fd), sbool(read_bytewise));

   Byte byte_to_write = 0x00;
   int rc = 0;
   RECORD_IO_EVENTX(
          fd,
          IE_WRITE,
          ( rc = write(fd, &byte_to_write, 1) )
         );
   if (rc < 0) {
      rc = -errno;
      DBGMSF(debug, "write() failed.  rc = %s", psc_desc(rc));
   }
   else {
      rc = 0;
      if (read_bytewise) {
         int ndx = 0;
         for (; ndx < 128 && rc == 0; ndx++) {
            RECORD_IO_EVENTX(
                fd,
                IE_READ,
                ( rc = read(fd, &rawedid->bytes[ndx], 1) )
               );
            if (rc < 0)
               rc = -errno;
            else  {
               assert(rc == 1);
               rc = 0;
            }
          }
          DBGMSF(debug, "Final single byte read returned %d, ndx=%d", rc, ndx);
      }
      else {
         RECORD_IO_EVENTX(
             fd,
             IE_READ,
             ( rc = read(fd, rawedid->bytes, 128) )
            );
         if (rc >= 0) {
            assert(rc == 128);
            rc = 0;
         }
         else {
            rc = -errno;
         }
         DBGMSF(debug, "read() returned %s", psc_desc(rc) );
      }
   } // write succeeded

   DBGTRC(debug, TRACE_GROUP, "Returning: %s", psc_desc(rc));
   return rc;
}


static Status_Errno_DDC
i2c_get_edid_bytes_using_i2c_layer(int fd, Buffer* rawedid, bool read_bytewise)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Getting EDID. File descriptor=%d, filename=%s, read_bytewise=%s",
                 fd, filename_for_fd_t(fd), sbool(read_bytewise));

   Byte byte_to_write = 0x00;
   int rc = invoke_i2c_writer(fd, 0x50, 1, &byte_to_write);
   DBGMSF(debug, "invoke_i2c_writer returned %s", psc_desc(rc));
   if (rc == 0) {   // write succeeded
      if (read_bytewise) {
         int ndx = 0;
         for (; ndx < 128 && rc == 0; ndx++) {
            // DBGMSG("Before invoke_i2c_reader() call");
            rc = invoke_i2c_reader(fd, 0x50, false, 1, &rawedid->bytes[ndx] );
         }
         DBGMSF(debug, "Final single byte read returned %d, ndx=%d", rc, ndx);
      } // read_bytewise == true
      else {
         rc = invoke_i2c_reader(fd, 0x50, read_bytewise, 128, rawedid->bytes);
         DBGMSF(debug, "invoke_i2c_reader returned %s", psc_desc(rc));

      }
      if (rc == 0) {
         rawedid->len = 128;
      }
   }  // write succeeded
   DBGTRC(debug, TRACE_GROUP, "Returning: %s", psc_desc(rc));
   return rc;
}


/** Gets EDID bytes of a monitor on an open I2C device.
 *
 * @param  fd        file descriptor for open /dev/i2c-n
 * @param  rawedid   buffer in which to return first 128 bytes of EDID
 *
 * @retval  0        success
 * @retval  <0       error
 */
Status_Errno_DDC i2c_get_raw_edid_by_fd(int fd, Buffer * rawedid) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Getting EDID. File descriptor = %d, filename=%s",
                              fd, filename_for_fd_t(fd));
#ifdef OLD
   bool conservative = false;
#endif

   assert(rawedid->buffer_size >= 128);
   Status_Errno_DDC rc;

   rc = i2c_set_addr(fd, 0x50, CALLOPT_ERR_MSG);
   if (rc < 0) {
      goto bye;
   }

#ifdef OLD
   // need a different call since tuned_sleep_with_tracex() now takes Display_Handle *, not DDCA_IO_Type
   // 10/23/15, try disabling sleep before write
   if (conservative) {
      TUNED_SLEEP_WITH_TRACE(DDCA_IO_I2C, SE_PRE_EDID, "Before write");
   }
#endif

   int max_tries = 4;  // 2 each read_bytewise == true/false
   bool read_bytewise = EDID_Read_Bytewise;
   rc = -1;
   DBGMSF(debug, "EDID read performed using %s,read_bytewise=%s",
                 (EDID_Read_Uses_I2C_Layer) ? "I2C layer" : "local io", sbool(read_bytewise));

   for (int tryctr = 0; tryctr < max_tries && rc != 0; tryctr++) {
      DBGMSF(debug, "Trying EDID read, tryctr=%d, max_tries=%d, read_bytewise=%s",
                    tryctr, max_tries, sbool(read_bytewise));

      if (EDID_Read_Uses_I2C_Layer) {
         rc = i2c_get_edid_bytes_using_i2c_layer(fd, rawedid, read_bytewise);
      }
      else {
         rc = i2c_get_edid_bytes_directly(fd, rawedid, read_bytewise);
      }
      if (rc == -ENXIO || rc == -EIO) {
         // DBGMSG("breaking");
         break;
      }
      assert(rc <= 0);
      if (rc == 0) {
         rawedid->len = 128;
         if (debug || IS_TRACING() ) {
            DBGMSG("get bytes returned:");
            dbgrpt_buffer(rawedid, 1);
            DBGMSG("edid checksum = %d", edid_checksum(rawedid->bytes) );
         }
         Byte checksum = edid_checksum(rawedid->bytes);
         if (checksum != 0) {
            // possible if successfully read bytes from i2c bus with no monitor
            // attached - the bytes will be junk.
            // e.g. nouveau driver, Quadro card, on blackrock
            DBGTRC(debug, TRACE_GROUP, "Invalid EDID checksum %d, expected 0.", checksum);
            rawedid->len = 0;
            rc = DDCRC_INVALID_EDID;    // was DDCRC_EDID
         }
      }  // get bytes succeeded

      // try the alternative
      // read_bytewise = !read_bytewise;
   }

bye:
   if (rc < 0)
      rawedid->len = 0;

   DBGTRC(debug, TRACE_GROUP, "Returning %s", psc_desc(rc));
   return rc;
}


/** Returns a parsed EDID record for the monitor on an I2C bus.
 *
 * @param fd      file descriptor for open /dev/i2c-n
 * @param edid_ptr_loc where to return pointer to newly allocated #Parsed_Edid,
 *                     or NULL if error
 *
 * @return status code
 */
Status_Errno_DDC i2c_get_parsed_edid_by_fd(int fd, Parsed_Edid ** edid_ptr_loc) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d", fd);
   Parsed_Edid * edid = NULL;
   Buffer * rawedidbuf = buffer_new(128, NULL);

   Status_Errno_DDC rc = i2c_get_raw_edid_by_fd(fd, rawedidbuf);
   if (rc == 0) {
      edid = create_parsed_edid(rawedidbuf->bytes);
      if (debug) {
         if (edid)
            report_parsed_edid(edid, false /* dump hex */, 0);
         else
            DBGMSG("create_parsed_edid() returned NULL");
      }
      if (!edid)
         rc = DDCRC_INVALID_EDID;
   }
   else {        // if (rc == DDCRC_EDID) {
      DBGTRC(debug, TRACE_GROUP, "i2c_get_raw_edid_by_fd() returned %s", psc_desc(rc));
   }

   buffer_free(rawedidbuf, NULL);
   DBGTRC(debug, TRACE_GROUP, "Returning %p", edid);
   *edid_ptr_loc = edid;
   return rc;
}


//
// I2C Bus Inspection - Fill in and report Bus_Info

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


bool i2c_detect_x37(int fd) {
   bool debug = false;
   bool result = false;

   // Quirks
   // - i2c_set_addr() Causes screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   //   avoided by never calling this function for an eDP screen
   // - Dell P2715Q does not respond to single byte read, but does respond to
   //   a write (7/2018), so this function checks both
   Status_Errno_DDC rc = i2c_set_addr(fd, 0x37, CALLOPT_NONE);
   if (rc == 0)  {
      // regard either a successful write() or a read() as indication slave address is valid
      Byte    writebuf = {0x00};
      rc = write(fd, &writebuf, 1);
      DBGTRC(debug, TRACE_GROUP,"write() for slave address x37 returned %s", psc_desc(rc));
      if (rc == 1)
         result = true;

      Byte    readbuf[4];  //  4 byte buffer
      rc = read(fd, readbuf, 4);
      DBGTRC(debug, TRACE_GROUP,"read() for slave address x37 returned %s", psc_desc(rc));
      // test doesn't work, buffer contains random bytes (but same random bytes for every
      // display in a single call to i2cdetect)
      // Byte ddc_null_msg[4] = {0x6f, 0x6e, 0x80, 0xbe};
      // if (rc == 4) {
      //    DBGMSG("read x37 returned: 0x%08x", readbuf);
      //    result = (memcmp( readbuf, ddc_null_msg, 4) == 0);
      // }
      if (rc >= 0)
         result = true;
   }

   DBGMSF(debug, "Returning %s", SBOOL(result));
   return result;
}


// Factored out of i2c_check_bus().  Not needed, since i2c_check_bus() is called
// only when the bus name is valid
void i2c_bus_check_valid_name(I2C_Bus_Info * bus_info) {
  assert(bus_info && ( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0) );

  if ( !(bus_info->flags & I2C_BUS_VALID_NAME_CHECKED) ) {
      bus_info->flags |= I2C_BUS_VALID_NAME_CHECKED;
      if ( !sysfs_is_ignorable_i2c_device(bus_info->busno) )
         bus_info->flags |= I2C_BUS_HAS_VALID_NAME;
   }

   bus_info->flags |= I2C_BUS_HAS_VALID_NAME;
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
   DBGTRC(debug, TRACE_GROUP, "Starting. busno=%d, buf_info=%p", bus_info->busno, bus_info );

   assert(bus_info && ( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0) );

   // void i2c_bus_check_valid_name(bus_info);  // unnecessary
   assert( (bus_info->flags & I2C_BUS_EXISTS) &&
           (bus_info->flags & I2C_BUS_VALID_NAME_CHECKED) &&
           (bus_info->flags & I2C_BUS_HAS_VALID_NAME)
         );

   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      DBGMSF(debug, "Probing");
      bus_info->flags |= I2C_BUS_PROBED;
      int fd = i2c_open_bus(bus_info->busno, CALLOPT_ERR_MSG);
      if (fd >= 0) {
          DBGMSF(debug, "Opened bus /dev/i2c-%d", bus_info->busno);
          bus_info->flags |= I2C_BUS_ACCESSIBLE;

          bus_info->functionality = i2c_get_functionality_flags_by_fd(fd);

          DDCA_Status ddcrc = i2c_get_parsed_edid_by_fd(fd, &bus_info->edid);
          DBGMSF(debug, "i2c_get_parsed_edid_by_fd() returned %d", ddcrc);
          if (ddcrc == 0) {
             bus_info->flags |= I2C_BUS_ADDR_0X50;

             if ( is_edp_device(bus_info->busno) ) {
                DBGMSF(debug, "eDP device detected");
                bus_info->flags |= I2C_BUS_EDP;
             }
             else {
                if ( i2c_detect_x37(fd) )
                   bus_info->flags |= I2C_BUS_ADDR_0X37;
             }
          }
          i2c_close_bus(fd, CALLOPT_ERR_MSG);
      }
   }   // probing complete

   // DBGTRC(debug, TRACE_GROUP, "Done. flags=0x%02x", bus_info->flags );
   if (debug || IS_TRACING() ) {
      DBGMSG("Done. flags=0x%04x, bus info:", bus_info->flags );
      i2c_dbgrpt_bus_info(bus_info, 2);
   }
}


void i2c_free_bus_info(I2C_Bus_Info * bus_info) {
   if (bus_info) {
      if ( memcmp(bus_info->marker, "BINx", 4) != 0) {   // just ignore if already freed
         assert( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0);
         if (bus_info->edid)
            free_parsed_edid(bus_info->edid);
         bus_info->marker[3] = 'x';
         free(bus_info);
      }
   }
}


//
// Bus Reports
//

/** Reports on a single I2C bus.
 *
 * \param   bus_info    pointer to Bus_Info structure describing bus
 * \param   depth       logical indentation depth
 *
 * \remark
 * The format of the output as well as its extent is controlled by get_output_level(). - no longer!
 */
// used by dbgreport_display_ref() in ddc_displays.c, always OL_VERBOSE
// used by debug code within this file
// used by i2c_report_buses() in this file, which is called by query_i2c_buses() in query_sysenv.c, always OL_VERBOSE
void i2c_dbgrpt_bus_info(I2C_Bus_Info * bus_info, int depth) {
   bool debug = false;
   DBGMSF(debug, "bus_info=%p", bus_info);
   assert(bus_info);

   rpt_vstring(depth, "Bus /dev/i2c-%d found:   %s", bus_info->busno, sbool(bus_info->flags&I2C_BUS_EXISTS));
   rpt_vstring(depth, "Bus /dev/i2c-%d probed:  %s", bus_info->busno, sbool(bus_info->flags&I2C_BUS_PROBED ));
   if ( bus_info->flags & I2C_BUS_PROBED ) {
      rpt_vstring(depth, "Bus accessible:          %s", sbool(bus_info->flags&I2C_BUS_ACCESSIBLE ));
      rpt_vstring(depth, "Bus is eDP:              %s", sbool(bus_info->flags&I2C_BUS_EDP ));
      rpt_vstring(depth, "Valid bus name checked:  %s", sbool(bus_info->flags & I2C_BUS_VALID_NAME_CHECKED));
      rpt_vstring(depth, "I2C bus has valid name:  %s", sbool(bus_info->flags & I2C_BUS_HAS_VALID_NAME));
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(depth, "Address 0x30 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X30));
#endif
      rpt_vstring(depth, "Address 0x37 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", sbool(bus_info->flags & I2C_BUS_ADDR_0X50));
      // not useful and clutters the output
      // i2c_report_functionality_flags(bus_info->functionality, /* maxline */ 90, depth);
      if ( bus_info->flags & I2C_BUS_ADDR_0X50) {
         if (bus_info->edid) {
            report_parsed_edid(bus_info->edid, true /* verbose */, depth);
         }
      }
   }

   DBGMSF(debug, "Done");
}


/** Reports a single active display.
 *
 * Output is written to the current report destination.
 *
 * @param   businfo     bus record
 * @param   depth       logical indentation depth
 */
// used by detect, interrogate commands, C API
void i2c_report_active_display(I2C_Bus_Info * businfo, int depth) {
   DDCA_Output_Level output_level = get_output_level();
   rpt_vstring(depth, "I2C bus:             /dev/"I2C"-%d", businfo->busno);

   // 08/2018 Disable.
   // Test for DDC communication is now done more sophisticatedly at the DDC level
   // The simple X37 test can have both false positives (DDC turned off in monitor but
   // X37 responsive), and false negatives (Dell P2715Q)
   // if (output_level >= DDCA_OL_NORMAL)
   // rpt_vstring(depth, "Supports DDC:        %s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level >= DDCA_OL_VERBOSE) {
#ifdef DETECT_SLAVE_ADDRS
      rpt_vstring(depth+1, "I2C address 0x30 (EDID block#)  present: %-5s", srepr(businfo->flags & I2C_BUS_ADDR_0X30));
      rpt_vstring(depth+1, "I2C address 0x37 (DDC)          present: %-5s", srepr(businfo->flags & I2C_BUS_ADDR_0X37));
#endif
      rpt_vstring(depth+1, "I2C address 0x50 (EDID) responsive: %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X50));
      rpt_vstring(depth+1, "Is eDP device:                      %-5s", sbool(businfo->flags & I2C_BUS_EDP));
      if (!(businfo->flags & I2C_BUS_EDP))
      rpt_vstring(depth+1, "I2C address 0x37 (DDC) responsive:  %-5s", sbool(businfo->flags & I2C_BUS_ADDR_0X37));

      char fn[PATH_MAX];     // yes, PATH_MAX is dangerous, but not as used here
      sprintf(fn, "/sys/bus/i2c/devices/i2c-%d/name", businfo->busno);
      char * sysattr_name = file_get_first_line(fn, /* verbose*/ false);
      rpt_vstring(depth+1, "%s:   %s", fn, sysattr_name);
      free(sysattr_name);

#ifndef TARGET_BSD
      if (output_level >= DDCA_OL_VV) {
         I2C_Sys_Info * info = get_i2c_sys_info(businfo->busno, -1);
         report_i2c_sys_info(info, depth);
         free_i2c_sys_info(info);
      }
#endif
   }

   if (businfo->edid) {
      if (output_level == DDCA_OL_TERSE)
         rpt_vstring(depth, "Monitor:             %s:%s:%s",
                            businfo->edid->mfg_id,
                            businfo->edid->model_name,
                            businfo->edid->serial_ascii);
      else
         report_parsed_edid(businfo->edid, false, depth);
   }
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
   DBGTRC(debug, TRACE_GROUP, "Returning %d", busct );
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
   DBGTRC(debug, DDCA_TRC_I2C, "Starting.  i2c_buses = %p", i2c_buses);
   if (!i2c_buses) {
      // only returns buses with valid name (arg=false)
#ifdef USE_UDEV
      Byte_Value_Array i2c_bus_bva = get_i2c_device_numbers_using_udev(false);
#else
      Byte_Value_Array i2c_bus_bva = get_i2c_devices_by_existence_test();
#endif
      // TODO: set free function
      i2c_buses = g_ptr_array_sized_new(bva_length(i2c_bus_bva));
      for (int ndx = 0; ndx < bva_length(i2c_bus_bva); ndx++) {
         int busno = bva_get(i2c_bus_bva, ndx);
         DBGMSF(debug, "Checking busno = %d", busno);
         I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
         businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
         i2c_check_bus(businfo);
         // if (debug || IS_TRACING() )
         //    i2c_dbgrpt_bus_info(businfo, 0);
         DBGMSF(debug, "Valid bus: /dev/"I2C"-%d", busno);
         g_ptr_array_add(i2c_buses, businfo);
      }
      bva_free(i2c_bus_bva);
   }
   int result = i2c_buses->len;
   DBGTRC(debug, DDCA_TRC_I2C, "Returning: %d", result);
   return result;
}


void i2c_discard_buses() {
   i2c_buses= NULL;
}

I2C_Bus_Info * i2c_detect_single_bus(int busno) {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_I2C, "Starting.  busno = %d", busno);
   I2C_Bus_Info * businfo = NULL;

   if (i2c_device_exists(busno) ) {
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS | I2C_BUS_VALID_NAME_CHECKED | I2C_BUS_HAS_VALID_NAME;
      i2c_check_bus(businfo);
      if (debug)
         i2c_dbgrpt_bus_info(businfo, 0);
   }

   DBGTRC(debug, DDCA_TRC_I2C, "Done.  busno=%d, returning: %p", busno, businfo);
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
I2C_Bus_Info * i2c_get_bus_info_by_index(int busndx) {
   assert(busndx >= 0);
   assert(i2c_buses);

   bool debug = false;
   DBGMSF(debug, "Starting.  busndx=%d", busndx );

   I2C_Bus_Info * bus_info = NULL;
   int busct = i2c_buses->len;
   assert(busndx < busct);
   bus_info = g_ptr_array_index(i2c_buses, busndx);
   // report_businfo(busInfo);
   if (debug) {
      DBGMSG("flags=0x%04x", bus_info->flags);
      DBGMSG("flags & I2C_BUS_PROBED = 0x%02x", (bus_info->flags & I2C_BUS_PROBED) );
   }
   assert( bus_info->flags & I2C_BUS_PROBED );
#ifdef OLD
   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      // DBGMSG("Calling check_i2c_bus()");
      i2c_check_bus(bus_info);
   }
#endif
   DBGMSF(debug, "busndx=%d, returning %p", busndx, bus_info );
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

   DBGMSF(debug, "Done. Returning: %p", result);
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
int i2c_report_buses(bool report_all, int depth) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. report_all=%s\n", sbool(report_all));

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

   DBGTRC(debug, TRACE_GROUP, "Done. Returning %d\n", reported_ct);
   return reported_ct;
}


#ifdef UNUSED
bool is_probably_laptop_display(I2C_Bus_Info * businfo) {
   assert(businfo);
   bool result = (businfo->flags & I2C_BUS_EDP) || is_embedded_parsed_edid(businfo->edid);
   return result;
}
#endif


static void init_i2c_bus_core_func_name_table() {
#define ADD_FUNC(_NAME) rtti_func_name_table_add(_NAME, #_NAME);
   ADD_FUNC(i2c_open_bus);
   ADD_FUNC(i2c_close_bus);
   ADD_FUNC(i2c_get_edid_bytes_using_i2c_layer);
   ADD_FUNC(i2c_get_edid_bytes_directly);
   ADD_FUNC(i2c_detect_buses);
   ADD_FUNC(i2c_detect_single_bus);
   ADD_FUNC(i2c_get_raw_edid_by_fd);
#undef ADD_FUNC
}


void init_i2c_bus_core() {
   init_i2c_bus_core_func_name_table();
}

