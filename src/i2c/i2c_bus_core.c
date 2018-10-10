// i2c_bus_core.c

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \file
 * I2C bus detection and inspection
 */

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
// #include <glib.h>
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
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_util.h"
#include "util/udev_i2c_util.h"
#include "util/utilrpt.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_do_io.h"
#include "i2c/wrap_i2c-dev.h"

#include "i2c/i2c_bus_core.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

/** All I2C buses.  GPtrArray of pointers to #I2C_Bus_Info - shared with i2c_bus_selector.c */
/* static */ GPtrArray * i2c_buses = NULL;

/** Global variable.  Controls whether function #i2c_set_addr() attempts retry
 *  after EBUSY error by changing ioctl op I2C_SLAVE to I2C_SLAVE_FORCE.
 */
bool i2c_force_slave_addr_flag = false;


//
// Basic I2C bus operations
//

/** Open an I2C bus device.
 *
 *  @param busno      bus number
 *  @param callopts  call option flags, controlling failure action
 *
 *  @retval >=0  file descriptor
 *  @retval -errno  negative Linux errno if open fails
 *
 *  Call options recognized
 *  - CALLOPT_ERR_MSG
 */
int i2c_open_bus(int busno, Byte callopts) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "busno=%d, callopts=0x%02x", busno, callopts);

   char filename[20];
   int  file;

   snprintf(filename, 19, "/dev/i2c-%d", busno);
   RECORD_IO_EVENT(
         IE_OPEN,
         ( file = open(filename, (callopts & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR) )
         );
   // per man open:
   // returns file descriptor if successful
   // -1 if error, and errno is set
   int errsv = errno;
   if (file < 0) {
#ifdef OLD
      if (callopts & CALLOPT_ERR_ABORT) {
         TERMINATE_EXECUTION_ON_ERROR("Open failed for %s. errno=%s\n",
                                      filename, linux_errno_desc(errsv));
      }
#endif
      if (callopts & CALLOPT_ERR_MSG) {
         f0printf(ferr(), "Open failed for %s: errno=%s\n",
                        filename, linux_errno_desc(errsv));
      }
      file = -errsv;
   }

   DBGTRC(debug, TRACE_GROUP, "Returning file descriptor: %d", file);
   return file;
}


/** Closes an open I2C bus device.
 *
 * @param  fd        file descriptor
 * @param  busno     bus number (for error messages), if -1, ignore
 * @param  callopts  call option flags, controlling failure action
 *
 * @retval 0  success
 * @retval <0 negative Linux errno value close*( fails and CALLOPT_ERR_ABORT not set in callopts
 */
Status_Errno i2c_close_bus(int fd, int busno, Call_Options callopts) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP,
          "Starting. fd=%d, busno=%d, callopts=%s",
          fd, busno,  interpret_call_options_t(callopts));

   Status_Errno result = 0;
   int rc = 0;

#ifdef ALTERNATIVE
   // get file name from descriptor instead of requiring busno parm
   char * i2c_fn;
   rc = filename_for_fd(fd, &i2c_fn);
   assert(rc == 0);
   DBGMSG("i2c_fn = %s", i2c_fn);
   free(i2c_fn);
#endif

   RECORD_IO_EVENT(IE_CLOSE, ( rc = close(fd) ) );
   assert( rc == 0 || rc == -1);   // per documentation
   int errsv = errno;
   if (rc < 0) {
      // EBADF (9)  fd isn't a valid open file descriptor
      // EINTR (4)  close() interrupted by a signal
      // EIO   (5)  I/O error
      char workbuf[80];
      if (busno >= 0)
         snprintf(workbuf, 80,
                  "Close failed for bus /dev/i2c-%d. errno=%s",
                  busno, linux_errno_desc(errsv));
      else
         snprintf(workbuf, 80,
                  "Bus device close failed. errno=%s",
                  linux_errno_desc(errsv));
#ifdef OLD
      if (callopts & CALLOPT_ERR_ABORT)
         TERMINATE_EXECUTION_ON_ERROR(workbuf);
#endif
      if (callopts & CALLOPT_ERR_MSG)
         f0printf(ferr(), "%s\n", workbuf);

      result = -errsv;
   }
   assert(result <= 0);
   DBGTRC(debug, TRACE_GROUP, "Returning: %d", result);
   return result;
}


/** Sets I2C slave address to be used on subsequent calls
 *
 * @param  file      file descriptor for open /dev/i2c-n
 * @param  addr      slave address
 * @param  callopts  call option flags, controlling failure action\n
 *                   if CALLOPT_FORCE set, use IOCTL op I2C_SLAVE_FORCE\n
 *                   to take control even if address is in use by another driver
 *
 * @retval  0 if success
 * @retval <0 negative Linux errno, if ioctl call fails and CALLOPT_ERR_ABORT not set in callopts
 *
 * \remark
 * Errors which are recovered are counted here using COUNT_STATUS_CODE().
 * The final status code is left for the caller to count
 */
Status_Errno i2c_set_addr(int file, int addr, Call_Options callopts) {
   bool debug = false;
#ifdef FOR_TESTING
   bool force_i2c_slave_failure = false;
#endif
   callopts |= CALLOPT_ERR_MSG;    // temporary
   DBGTRC(debug, TRACE_GROUP,
                 "file=%d, addr=0x%02x, i2c_force_slave_addr_flag=%s, callopts=%s",
                 file, addr,
                 bool_repr(i2c_force_slave_addr_flag),
                 interpret_call_options_t(callopts) );
   // FAILSIM;

   Status_Errno result = 0;
   int rc = 0;
   int errsv = 0;
   uint16_t op = I2C_SLAVE;

retry:
   errno = 0;
   RECORD_IO_EVENT( IE_OTHER, ( rc = ioctl(file, op, addr) ) );
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
#ifdef OLD
         report_ioctl_error(errsv, __func__, __LINE__-13, __FILE__,
                            /*fatal=*/ callopts&CALLOPT_ERR_ABORT);
      else if (callopts & CALLOPT_ERR_ABORT)
         DDC_ABORT(DDCL_INTERNAL_ERROR);
#endif

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
// I2C Bus Inspection - Functionality Flags
//


// Separate table for interpreting functionality flags.

Value_Name_Table functionality_flag_table = {
      VN(I2C_FUNC_I2C                    ),   //0x00000001
      VN(I2C_FUNC_10BIT_ADDR             ),   //0x00000002
      VN(I2C_FUNC_PROTOCOL_MANGLING      ),   //0x00000004 /* I2C_M_IGNORE_NAK etc. */
      VN(I2C_FUNC_SMBUS_PEC              ),   //0x00000008
   // VN(I2C_FUNC_NOSTART                ),   //0x00000010 /* I2C_M_NOSTART */  // i2c-tools 4.0
   // VN(I2C_FUNC_SLAVE                  ),   //0x00000020                      // i2c-tools 4.0
      {0x00000010, "I2C_FUNC_NOSTART", NULL  },
      {0x00000020, "I2C_FUNC_SLAVE",   NULL    },
      VN(I2C_FUNC_SMBUS_BLOCK_PROC_CALL  ),   //0x00008000 /* SMBus 2.0 */
      VN(I2C_FUNC_SMBUS_QUICK            ),   //0x00010000
      VN(I2C_FUNC_SMBUS_READ_BYTE        ),   //0x00020000
      VN(I2C_FUNC_SMBUS_WRITE_BYTE       ),   //0x00040000
      VN(I2C_FUNC_SMBUS_READ_BYTE_DATA   ),   //0x00080000
      VN(I2C_FUNC_SMBUS_WRITE_BYTE_DATA  ),   //0x00100000
      VN(I2C_FUNC_SMBUS_READ_WORD_DATA   ),   //0x00200000
      VN(I2C_FUNC_SMBUS_WRITE_WORD_DATA  ),   //0x00400000
      VN(I2C_FUNC_SMBUS_PROC_CALL        ),   //0x00800000
      VN(I2C_FUNC_SMBUS_READ_BLOCK_DATA  ),   //0x01000000
      VN(I2C_FUNC_SMBUS_WRITE_BLOCK_DATA ),   //0x02000000
      VN(I2C_FUNC_SMBUS_READ_I2C_BLOCK   ),   //0x04000000 /* I2C-like block xfer  */
      VN(I2C_FUNC_SMBUS_WRITE_I2C_BLOCK  ),   //0x08000000 /* w/ 1-byte reg. addr. */
   // VN(I2C_FUNC_SMBUS_HOST_NOTIFY      ),   //0x10000000               // i2c-tools 4.0
      {0x10000000, "I2C_FUNC_SMBUS_HOST_NOTIFY", NULL},
      VN_END
};


/** Gets the I2C functionality flags for an open I2C bus,
 *  specified by its file descriptor.
 *
 *  @param fd  file descriptor
 *  @return functionality flags
 */
unsigned long i2c_get_functionality_flags_by_fd(int fd) {
   bool debug = false;
   DBGMSF(debug, "Starting.", NULL);

   unsigned long funcs;
   int rc;

   RECORD_IO_EVENT(IE_OTHER, ( rc = ioctl(fd, I2C_FUNCS, &funcs) ) );
   // int errsv = errno;
   if (rc < 0) {
      REPORT_IOCTL_ERROR("I2C_FUNCS", errno);
      funcs = 0;
   }

   DBGMSF(debug, "Functionality for file descriptor %d: %lu, 0x%0lx", fd, funcs, funcs);
   return funcs;
}


/** Returns a string representation of functionality flags.
 *
 * @param functionality  long int of flags
 * @return string representation of flags
 */
char * i2c_interpret_functionality_flags(unsigned long functionality) {
   // HACK ALERT: There are 2 entries for bit I2C_FUNC_I2C in functionality_table,
   // one for function name ioctl_read and another for function name ioctl_write
   // These are at indexes 0 and 1.   For our purposes here we only want to check
   // each bit once, so we start at index 1 instead of 0.
   // return vnt_interpret_flags(functionality, functionality_table2+1, false, ", ");
   return vnt_interpret_flags(functionality, functionality_flag_table, false, ", ");
}


/** Reports functionality flags.
 *
 *  The output is multiline.
 *
 *  @param  functionality  flags to report
 *  @param  maxline        maximum length of 1 line
 *  @param  depth          logical indentation depth
 */
void i2c_report_functionality_flags(long functionality, int maxline, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting.  functionality=0x%016x, maxline=%d", functionality, maxline);

   char * buf0 = i2c_interpret_functionality_flags(functionality);
   DBGMSF(debug, "buf0=|%s|", buf0);

   char * header = "Functionality: ";
   int hdrlen = strlen(header);
   int maxpiece = maxline - ( rpt_get_indent(depth) + hdrlen);

   Null_Terminated_String_Array ntsa = strsplit_maxlength( buf0, maxpiece, " ");
   int ntsa_ndx = 0;
   while (true) {
      char * s = ntsa[ntsa_ndx++];
      if (!s)
         break;
      // printf("(%s) header=|%s|, s=|%s|\n", __func__, header, s);
      rpt_vstring(depth, "%-*s%s", hdrlen, header, s);
      // printf("(%s) s = %p\n", __func__, s);
      if (strlen(header) > 0)
         header = "";

   }
   free(buf0);
   ntsa_free(ntsa, /* free_strings */ true);

   DBGMSF(debug, "Done", NULL);
}


//
// I2C Bus Inspection - Slave Addresses
//


bool is_edp_device(int busno) {
   bool debug = false;
   // DBGMSF(debug, "Starting.  busno=%d", busno);
   bool result = false;

   char cmd[100];
   snprintf(cmd, 100, "ls -d /sys/class/drm/card*/card*/i2c-%d", busno);
   // DBGMSG("cmd: %s", cmd);

   GPtrArray * lines = execute_shell_cmd_collect(cmd);

   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * s = g_ptr_array_index(lines, ndx);
      // DBGMSG("s: %s", s);
      if (strstr(s, "-eDP-")) {
         // DBGMSG("Found");
         result = true;
         break;
      }
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
      i2c_set_addr(fd, addr, CALLOPT_ERR_ABORT || CALLOPT_ERR_MSG);
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
      i2c_close_bus(file, busno, CALLOPT_ERR_ABORT);
   }

   DBGMSF(debug, "Returning %p", addrmap);
   return addrmap;
}
#endif


/** Checks DDC related addresses on an I2C bus to see if the addresses are active.
 *  The bus device has already been opened.
 *
 * \param  fd       file descriptor for open i2c device
 * \param  presult  where to return result byte
 * \return status code, 0 if success
 *
 * Sets:
 *   Returns byte with flags possibly set:
 *    I2C_BUS_ADDR_0x30        true if addr x30 responds (EDID block selection)
 *    I2C_BUS_ADDR_0x50        true if addr x50 responds (EDID)
 *    I2C_BUS_ADDR_0x37        true if addr x37 responds (DDC commands)
 */
// static
Status_Errno_DDC i2c_detect_ddc_addrs_by_fd(int fd, Byte * presult) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d", fd);
   assert(fd >= 0);
   unsigned char result = 0x00;

   Byte    readbuf;  //  1 byte buffer
   // Byte    writebuf = {0x00};
   Status_Errno_DDC base_rc = 0;

#ifdef DISABLE
   // Causes screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   base_rc = i2c_set_addr(fd, 0x30, CALLOPT_NONE);
   if (base_rc < 0) {
      goto bye;
   }
   base_rc = invoke_i2c_writer(fd, 1, &writebuf);
   // DBGMSG("invoke_i2c_writer() returned %s", psc_desc(rc));
   if (base_rc == 0)
      result |= I2C_BUS_ADDR_0X30;
#else
   // DBGMSG("Skipping probe of slave address x30.");
#endif

   base_rc = i2c_set_addr(fd, 0x50, CALLOPT_NONE);
   if (base_rc < 0) {
      goto bye;
   }
   base_rc = invoke_i2c_reader(fd, 1, &readbuf);
   if (base_rc == 0)
      result |= I2C_BUS_ADDR_0X50;

#ifdef DISABLE
   // 10/2018: Causes temporary screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   base_rc = i2c_set_addr(fd, 0x37, CALLOPT_NONE);
   if (base_rc < 0) {
      goto bye;
   }
   // 7/2018: changed from invoke_i2c_reader() to invoke_i2c_writer()
   //         Dell P2715Q does not always respond to read of single byte
   base_rc = invoke_i2c_writer(fd, 1, &writebuf);
   DBGTRC(debug, TRACE_GROUP,"invoke_i2c_writer() for slave address x37 returned %s", psc_desc(base_rc));
   if (base_rc == 0) {
      base_rc = invoke_i2c_reader(fd, 1, &readbuf);
      DBGTRC(debug, TRACE_GROUP,"invoke_i2c_reader() for slave address x37 returned %s", psc_desc(base_rc));
   }

   if (base_rc == 0)
      result |= I2C_BUS_ADDR_0X37;
#else
   // DBGMSG("Skipping probe of slave address x37.");
#endif

   base_rc = 0;

bye:
   if (base_rc != 0)
      result = 0x00;

   *presult = result;

   DBGTRC(debug, TRACE_GROUP,
          "Done.  Returning base_rc=%s, *presult = 0x%02x", psc_desc(base_rc), *presult);
   return base_rc;
}


//
// I2C Bus Inspection - EDID Retrieval
//

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
   DBGTRC(debug, TRACE_GROUP, "Getting EDID for file %d", fd);

   bool conservative = true;

   assert(rawedid->buffer_size >= 128);
   Status_Errno_DDC rc;

   rc = i2c_set_addr(fd, 0x50, CALLOPT_ERR_MSG);
   if (rc < 0) {
      goto bye;
   }
   // 10/23/15, try disabling sleep before write
   if (conservative)
      sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "before write");

   Byte byte_to_write = 0x00;

   int max_tries = 3;
   for (int tryctr = 0; tryctr < max_tries; tryctr++) {
      rc = invoke_i2c_writer(fd, 1, &byte_to_write);
      if (rc == 0) {
         rc = invoke_i2c_reader(fd, 128, rawedid->bytes);
         assert(rc <= 0);
         if (rc == 0) {
            rawedid->len = 128;
            if (debug) {
               DBGMSG("call_read returned:");
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
         }
         if (rc == 0)
            break;
      }
      if (tryctr < max_tries)
         DBGTRC(debug, TRACE_GROUP, "Retrying EDID read.  tryctr=%d, max_tries=%d", tryctr, max_tries);
   }

bye:
   if (rc < 0)
      rawedid->len = 0;

   if (debug || IS_TRACING()) {
      DBGMSG("Returning %s.  edidbuf contents:", psc_desc(rc));
      buffer_dump(rawedid);
   }
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
            DBGMSG("create_parsed_edid() returned NULL", NULL);
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


/** Inspects an I2C bus.
 *
 *  Takes the number of the bus to be inspected from the #I2C_Bus_Info struct passed
 *  as an argument.
 *
 *  @param  bus_info  pointer to #I2C_Bus_Info struct in which information will be set
 */
static void i2c_check_bus(I2C_Bus_Info * bus_info) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. busno=%d, buf_info=%p", bus_info->busno, bus_info );

   assert(bus_info);
   assert( memcmp(bus_info->marker, I2C_BUS_INFO_MARKER, 4) == 0);

   int file = 0;

   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      DBGMSF(debug, "Probing", NULL);
      bus_info->flags |= I2C_BUS_PROBED;

      bool b = is_edp_device(bus_info->busno);
      if (b) {
         DBGMSG("eDP device detected");
         bus_info->flags |= I2C_BUS_EDP;
         // goto bye;
      }

      // unnecessary, bus_info is already filtered
      // probing hangs on PowerMac if i2c device is SMU
      // if (!is_ignorable_i2c_device(bus_info->busno)) {

         file = i2c_open_bus(bus_info->busno, CALLOPT_ERR_MSG);  // returns if failure

         if (file >= 0) {
            bus_info->flags |= I2C_BUS_ACCESSIBLE;
            Byte ddc_addr_flags = 0x00;
            Status_Errno_DDC psc = i2c_detect_ddc_addrs_by_fd(file, &ddc_addr_flags);
            if (psc != 0) {
               DBGMSF(debug, "detect_ddc_addrs_by_fd() returned %d", psc);
               f0printf(ferr(), "Failure detecting bus addresses for /dev/i2c-%d: status code=%s\n",
                              bus_info->busno, psc_desc(psc));
               goto bye;
            }
            bus_info->flags |= ddc_addr_flags;
            // DBGMSF(debug, "Calling i2c_get_functionality_flags_by_fd()...");
            bus_info->functionality = i2c_get_functionality_flags_by_fd(file);
            // DBGMSF(debug, "i2c_get_functionality_flags_by_fd() returned %lu", bus_info->functionality);
            if (bus_info->flags & I2C_BUS_ADDR_0X50) {
               // Have seen case of nouveau driver with Quadro card where
               // there's a bus that has no monitor but responds to the X50 probe
               // of detect_ddc_addrs_by_fd() and then returns a garbage EDID
               // when the bytes are read in i2c_get_parsed_edid_by_fd()
               // TODO: handle case of i2c_get_parsed_edid_by_fd() returning NULL
               // but should never fail if detect_ddc_addrs_by_fd() succeeds
               psc = i2c_get_parsed_edid_by_fd(file, &bus_info->edid);
               if (psc != 0) {
                  DBGMSF(debug, "i2c_get_parsed_edid_by_fd() returned %d", psc);
                  f0printf(ferr(), "Failure getting EDID for /dev/i2c-%d: status code=%s\n",
                                 bus_info->busno, psc_desc(psc));
                  goto bye;
               }
               // bus_info->flags |= I2C_BUS_EDID_CHECKED;
            }
   #ifdef NO
            // test is being made in ddc_displays.c
            if (bus_info->flags & I2C_BUS_ADDR_0X37) {
               // have seen case where laptop display reports addr 37 active, but
               // it doesn't respond to DDC
               // 8/2017: If DDC turned off on U3011 monitor, addr x37 still detected
               // TODO: sanity check for DDC goes here
               // or make this check at a higher level, since I2c doesn't understand DDC

            }
   #endif
         }
      }
   // }

bye:
   if (file >= 0)
      i2c_close_bus(file, bus_info->busno,  CALLOPT_ERR_MSG);

   // DBGTRC(debug, TRACE_GROUP, "Done. flags=0x%02x", bus_info->flags );
   if (debug || IS_TRACING() ) {
      DBGMSG("Done. flags=0x%02x", bus_info->flags );
      i2c_dbgrpt_bus_info(bus_info, 1);
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

// Why are there 2 functions?  Consolidate?
// only difference is that i2c_debugreport_bus_info() shows EDID

#ifdef UNUSED
void i2c_dbgreport_bus_info_flags(I2C_Bus_Info * bus_info, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting.  bus_info=%p");
   rpt_vstring(depth, "Bus /dev/i2c-%d found:    %s", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_EXISTS));
   rpt_vstring(depth, "Bus /dev/i2c-%d probed:   %s", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_PROBED ));
   if ( bus_info->flags & I2C_BUS_PROBED ) {
      rpt_vstring(depth, "Address 0x30 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X30));
      rpt_vstring(depth, "Address 0x37 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X50));
   }
   i2c_report_functionality_flags(bus_info->functionality, /* maxline */ 90, depth);
   DBGMSF(false, "Done");
}
#endif


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

   rpt_vstring(depth, "Bus /dev/i2c-%d found:    %s", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_EXISTS));
   rpt_vstring(depth, "Bus /dev/i2c-%d probed:   %s", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_PROBED ));
   if ( bus_info->flags & I2C_BUS_PROBED ) {
      rpt_vstring(depth, "Address 0x30 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X30));
      rpt_vstring(depth, "Address 0x37 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth, "Address 0x50 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X50));
      i2c_report_functionality_flags(bus_info->functionality, /* maxline */ 90, depth);
      if ( bus_info->flags & I2C_BUS_ADDR_0X50) {
         if (bus_info->edid) {
            report_parsed_edid(bus_info->edid, true /* verbose */, depth);
         }
      }
   }

   DBGMSF(debug, "Done", NULL);
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
   rpt_vstring(depth, "I2C bus:             /dev/i2c-%d", businfo->busno);

   // 08/2018 Disable.
   // Test for DDC communication is now done more sophisticatedly at the DDC level
   // The simple X37 test can have both false positives (DDC turned off in monitor but
   // X37 responsive), and false negatives (Dell P2715Q)
   // if (output_level >= DDCA_OL_NORMAL)
   // rpt_vstring(depth, "Supports DDC:        %s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level >= DDCA_OL_VERBOSE) {
      rpt_vstring(depth+1, "I2C address 0x30 (EDID block#)  present: %-5s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X30));
      rpt_vstring(depth+1, "I2C address 0x37 (DDC)          present: %-5s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth+1, "I2C address 0x50 (EDID)         present: %-5s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X50));

      char fn[PATH_MAX];     // yes, PATH_MAX is dangerous, but not as used here
      sprintf(fn, "/sys/bus/i2c/devices/i2c-%d/name", businfo->busno);
      char * sysattr_name = file_get_first_line(fn, /* verbose*/ false);
      rpt_vstring(depth+1, "%s: %s", fn, sysattr_name);
      free(sysattr_name);
   }

   if (businfo->edid) {
      switch(output_level) {
      case DDCA_OL_TERSE:
         rpt_vstring(depth, "Monitor:             %s:%s:%s",
                            businfo->edid->mfg_id,
                            businfo->edid->model_name,
                            businfo->edid->serial_ascii);
         break;
      case DDCA_OL_NORMAL:
         report_parsed_edid(businfo->edid, false, depth);
         break;
      case DDCA_OL_VERBOSE:
         report_parsed_edid(businfo->edid, true, depth);
         break;
      }
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
   sprintf(namebuf, "/dev/i2c-%d", busno);
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

    DBGMSF(debug, "busno=%d, returning %s", busno, bool_repr(result) );
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

int i2c_detect_buses() {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_I2C, "Starting.  i2c_buses = %p", i2c_buses);
   if (!i2c_buses) {
      Byte_Value_Array i2c_bus_bva = get_i2c_device_numbers_using_udev(false);
      // TODO: set free function
      i2c_buses = g_ptr_array_sized_new(bva_length(i2c_bus_bva));
      for (int ndx = 0; ndx < bva_length(i2c_bus_bva); ndx++) {
         int busno = bva_get(i2c_bus_bva, ndx);
         DBGMSF(debug, "Checking busno = %d", busno);
         I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
         businfo->flags = I2C_BUS_EXISTS;
         i2c_check_bus(businfo);
         // if (debug || IS_TRACING() )
         //    i2c_dbgrpt_bus_info(businfo, 0);
         g_ptr_array_add(i2c_buses, businfo);
      }
      bva_free(i2c_bus_bva);
   }
   int result = i2c_buses->len;
   DBGTRC(debug, DDCA_TRC_I2C, "Returning: %d", result);
   return result;
}


I2C_Bus_Info * detect_single_bus(int busno) {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_I2C, "Starting.  busno = %d", busno);
   I2C_Bus_Info * businfo = NULL;

   if (i2c_device_exists(busno) ) {
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS;
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
      DBGMSG("flags=0x%02x", bus_info->flags);
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

   DBGMSF(debug, "Returning %s", bool_repr(result));
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
   DBGTRC(debug, TRACE_GROUP, "Starting. report_all=%s\n", bool_repr(report_all));

   assert(i2c_buses);
   int busct = i2c_buses->len;
   int reported_ct = 0;

   puts("");
   if (report_all)
      rpt_vstring(depth,"Detected %d I2C buses:", busct);
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

