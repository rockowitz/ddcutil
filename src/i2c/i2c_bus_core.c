/* i2c_bus_core.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/debug_util.h"
#include "util/failsim.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"

#include "i2c/i2c_do_io.h"
#include "i2c/wrap_i2c-dev.h"

#include "i2c/i2c_bus_core.h"


// maximum number of i2c buses this code supports
#define I2C_BUS_MAX 32

// Addresses on an I2C bus are 7 bits in size
#define BUS_ADDR_MAX 128

// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_I2C;

// forward declarations
void report_businfo(Bus_Info * bus_info, int depth);


//
// Basic I2C bus operations
//

/* Open an I2C bus device.
 *
 * Arguments:
 *   busno      bus number
 *   callopts  call option flags, controlling failure action
 *
 * Returns:
 *    0 if success
 *    -errno if close fails and CALLOPT_ERR_ABORT not set in callopts
 */
int i2c_open_bus(int busno, Byte callopts) {
   bool debug = false;
   DBGMSF(debug, "busno=%d, callopts=0x%02x", busno, callopts);

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
      if (callopts & CALLOPT_ERR_ABORT) {
         TERMINATE_EXECUTION_ON_ERROR("Open failed for %s. errno=%s\n",
                                      filename, linux_errno_desc(errsv));
      }
      if (callopts & CALLOPT_ERR_MSG) {
         f0printf(FERR, "Open failed for %s: errno=%s\n",
                        filename, linux_errno_desc(errsv));
      }
      file = -errsv;
   }

   DBGMSF(debug, "Returning file descriptor: %d", file);
   return file;
}


/* Closes an open I2C bus device.
 *
 * Arguments:
 *   fd         file descriptor
 *   busno      bus number (for error messages), if -1, ignore
 *   callopts  call option flags, controlling failure action
 *
 * Returns:
 *    0 if success
 *    -errno if close fails and CALLOPT_ERR_ABORT not set in callopts
 */
int i2c_close_bus(int fd, int busno, Byte callopts) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, callopts=0x%02x", fd, callopts);

   errno = 0;
   int rc = 0;
   RECORD_IO_EVENT(IE_CLOSE, ( rc = close(fd) ) );
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

      if (callopts & CALLOPT_ERR_ABORT)
         TERMINATE_EXECUTION_ON_ERROR(workbuf);

      if (callopts & CALLOPT_ERR_MSG)
         f0printf(FERR, "%s\n", workbuf);

      rc = errsv;
   }
   return rc;
}


bool i2c_force_slave_addr_flag = false;


/* Sets I2C slave address to be used on subsequent calls
 *
 * Arguments:
 *   fd    file descriptor for open /dev/i2c-n
 *   addr  slave address
 *   callopts  call option flags, controlling failure action
 *         if CALLOPT_FORCE set, use IOCTL op I2C_SLAVE_FORCE
 *         to take control even if address is in use by another driver
 *
 * Returns:
 *    0 if success
 *    -errno if ioctl call fails and CALLOPT_ERR_ABORT not set in callopts
 */
int i2c_set_addr(int file, int addr, Call_Options callopts) {
   bool debug = false;
   DBGMSF(debug, "file=%d, addr=0x%02x, callopts=%s", file, addr, interpret_call_options(callopts));
   // FAILSIM_EXT( ( show_backtrace(1) ) )
   FAILSIM;

   int rc = 0;
   int errsv = 0;
   uint16_t op = I2C_SLAVE;
   // if (callopts & CALLOPT_FORCE_SLAVE) {
   if (i2c_force_slave_addr_flag) {
      DBGMSG("Using IOCTL op I2C_SLAVE_FORCE for address 0x%02x", addr );
      op = I2C_SLAVE_FORCE;
   }

   RECORD_IO_EVENT(
         IE_OTHER,
         ( rc = ioctl(file, op, addr) )
        );

   if (rc < 0) {
      errsv = errno;
      if ( callopts & CALLOPT_ERR_MSG)
         report_ioctl_error(errsv, __func__, __LINE__-9, __FILE__,
                            /*fatal=*/ callopts&CALLOPT_ERR_ABORT);
      else if (callopts & CALLOPT_ERR_ABORT)
         DDC_ABORT(DDCL_INTERNAL_ERROR);
      errsv = -errsv;
   }

   if (errsv || debug) {
      printf("(%s) addr = 0x%02x. Returning %d\n", __func__, addr, errsv);
      // show_backtrace(1);
   }

   return errsv;
}


//
// I2C Bus Inspection
//

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
bool * detect_all_addrs_by_fd(int fd) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d", fd);
   assert (fd >= 0);
   bool * addrmap = NULL;

   unsigned char byte_to_write = 0x00;
   int addr;
   addrmap = calloc(BUS_ADDR_MAX, sizeof(bool));
   //bool addrmap[128] = {0};

   for (addr = 3; addr < BUS_ADDR_MAX; addr++) {
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
bool * detect_all_addrs(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   int file = i2c_open_bus(busno, CALLOPT_ERR_MSG);  // return if failure
   bool * addrmap = NULL;

   if (file >= 0) {
      addrmap = detect_all_addrs_by_fd(file);
      i2c_close_bus(file, busno, CALLOPT_ERR_ABORT);
   }

   DBGMSF(debug, "Returning %p", addrmap);
   return addrmap;
}


/* Checks DDC related addresses on an I2C bus to see if the address is active.
 * The bus device has already been opened.
 *
 * Arguments:
 *   fd   file descriptor for open i2c device
 *   presult
 *
 * Returns:
 *   Returns byte with flags possibly set:
 *    I2C_BUS_ADDR_0x50        true if addr x50 responds (EDID)
 *    I2C_BUS_ADDR_0x37        true if addr x37 responds (DDC commands)
 *
 * Returns:
 *    if < 0, modulated status code from i2c_set_addr()
 */
// static
Global_Status_Code detect_ddc_addrs_by_fd(int fd, Byte * presult) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d", fd);
   assert(fd >= 0);
   unsigned char result = 0x00;

   Byte    readbuf;  //  1 byte buffer
   int base_rc = 0;
   Global_Status_Code gsc = 0;

   base_rc = i2c_set_addr(fd, 0x50, CALLOPT_ERR_MSG);   // CALLOPT_ERR_MSG temporary
   if (base_rc < 0) {
      gsc = modulate_rc(base_rc, RR_ERRNO);
      goto bye;
   }
   gsc = invoke_i2c_reader(fd, 1, &readbuf);
   if (gsc >= 0)
      result |= I2C_BUS_ADDR_0X50;

   base_rc = i2c_set_addr(fd, 0x37, CALLOPT_ERR_MSG);   // CALLOPT_ERR_MSG temporary
   if (base_rc < 0) {
      gsc = modulate_rc(base_rc,RR_ERRNO);
      goto bye;
   }
   gsc = invoke_i2c_reader(fd, 1, &readbuf);
   // DBGMSG("call_read() returned %d", rc);
   // 11/2015: DDCRC_READ_ALL_ZERO currently set only in ddc_packet_io.c:
   if (gsc >= 0 || base_rc == DDCRC_READ_ALL_ZERO)
      result |= I2C_BUS_ADDR_0X37;
   gsc = 0;

bye:
   if (gsc != 0)
      result = 0x00;

   *presult = result;
   DBGMSF(debug, "Done.  Returning gsc=%d, *presult = 0x%02x", *presult);
   return gsc;
}


//
// Bus functionality
//

// Functions and data structures for interpreting the I2C bus functionality flags.
// They are overly complex for production use.  They were created during development
// to facilitate exploratory programming.

typedef
struct {
        unsigned long bit;
        char *        name;
        char *        function_name;
} I2C_Func_Table_Entry;

// Note 2 entries for I2C_FUNC_I2C.  Usage must take this into account.
I2C_Func_Table_Entry functionality_table[] = {
//  bit value of flag                 flag name                          i2c function name
// {I2C_FUNC_I2C                    , "I2C_FUNC_I2C",                    NULL},
   {I2C_FUNC_I2C                    , "I2C_FUNC_I2C",                    "ioctl_write"},
   {I2C_FUNC_I2C                    , "I2C_FUNC_I2C",                    "ioctl_read"},
   {I2C_FUNC_10BIT_ADDR             , "I2C_FUNC_10BIT_ADDR",             NULL},
   {I2C_FUNC_PROTOCOL_MANGLING      , "I2C_FUNC_PROTOCOL_MANGLING",      NULL},
   {I2C_FUNC_SMBUS_PEC              , "I2C_FUNC_SMBUS_PEC",              "i2c_smbus_pec"},
   {I2C_FUNC_SMBUS_BLOCK_PROC_CALL  , "I2C_FUNC_SMBUS_BLOCK_PROC_CALL",  "i2c_smbus_block_proc_call"},
   {I2C_FUNC_SMBUS_QUICK            , "I2C_FUNC_SMBUS_QUICK",            "i2c_smbus_quick"},
   {I2C_FUNC_SMBUS_READ_BYTE        , "I2C_FUNC_SMBUS_READ_BYTE",        "i2c_smbus_read_byte"},
   {I2C_FUNC_SMBUS_WRITE_BYTE       , "I2C_FUNC_SMBUS_WRITE_BYTE",       "i2c_smbus_write_byte"},
   {I2C_FUNC_SMBUS_READ_BYTE_DATA   , "I2C_FUNC_SMBUS_READ_BYTE_DATA",   "i2c_smbus_read_byte_data"},
   {I2C_FUNC_SMBUS_WRITE_BYTE_DATA  , "I2C_FUNC_SMBUS_WRITE_BYTE_DATA",  "i2c_smbus_write_byte_data"},
   {I2C_FUNC_SMBUS_READ_WORD_DATA   , "I2C_FUNC_SMBUS_READ_WORD_DATA",   "i2c_smbus_read_word_data"},
   {I2C_FUNC_SMBUS_WRITE_WORD_DATA  , "I2C_FUNC_SMBUS_WRITE_WORD_DATA",  "i2c_smbus_write_word_data"},
   {I2C_FUNC_SMBUS_PROC_CALL        , "I2C_FUNC_SMBUS_PROC_CALL",        "i2c_smbus_proc_call"},
   {I2C_FUNC_SMBUS_READ_BLOCK_DATA  , "I2C_FUNC_SMBUS_READ_BLOCK_DATA",  "i2c_smbus_read_block_data"},
   {I2C_FUNC_SMBUS_WRITE_BLOCK_DATA , "I2C_FUNC_SMBUS_WRITE_BLOCK_DATA", "i2c_smbus_write_block_data"},
   {I2C_FUNC_SMBUS_READ_I2C_BLOCK   , "I2C_FUNC_SMBUS_READ_I2C_BLOCK",   "i2c_smbus_read_i2c_block_data"},
   {I2C_FUNC_SMBUS_WRITE_I2C_BLOCK  , "I2C_FUNC_SMBUS_WRITE_I2C_BLOCK",  "i2c_smbus_write_i2c_block_data"}
};
int bit_name_ct = sizeof(functionality_table) / sizeof(I2C_Func_Table_Entry);


static I2C_Func_Table_Entry * find_func_table_entry_by_funcname(char * funcname) {
   // DBGMSG("Starting.  funcname=%s", funcname);
   int ndx = 0;
   I2C_Func_Table_Entry * result = NULL;
   for (ndx = 0; ndx < bit_name_ct; ndx++) {
      // printf("ndx=%d, bit_name_ct=%d\n", ndx, bit_name_ct);
      // printf("--%s--\n", funcname);
      // printf("--%s--\n", functionality_table[ndx].function_name);
      if ( streq( functionality_table[ndx].function_name, funcname)) {
         result = &functionality_table[ndx];
         break;
      }
   }
   // DBGMSG("funcname=%s, returning %s", funcname, (result) ? result->name : "NULL");
   return result;
}


static bool is_function_supported(int busno, char * funcname) {
   // DBGMSG("Starting. busno=%d, funcname=%s", busno, funcname);
   bool result = true;
   if ( !streq(funcname, "read") &&  !streq(funcname, "write") ) {
      I2C_Func_Table_Entry * func_table_entry = find_func_table_entry_by_funcname(funcname);
      if (!func_table_entry) {
         TERMINATE_EXECUTION_ON_ERROR("Unrecognized function name: %s", funcname);
      }
      assert(func_table_entry);   // suppresses clang analyzer warning re dereference of possibly null func_table_entry
      if (busno < 0 || busno >= i2c_get_busct() ) {
         TERMINATE_EXECUTION_ON_ERROR("Invalid bus: /dev/i2c-%d\n", busno);
      }

      // DBGMSG("functionality=0x%lx, func_table_entry->bit=-0x%lx", bus_infos[busno].functionality, func_table_entry->bit);
      Bus_Info * bus_info = i2c_get_bus_info(busno, DISPSEL_NONE);
      result = (bus_info->functionality & func_table_entry->bit) != 0;
   }
   // DBGMSG("busno=%d, funcname=%s, returning %d", busno, funcname, result);
   return result;
}


bool i2c_verify_functions_supported(int busno, char * write_func_name, char * read_func_name) {
   // printf("(%s) Starting. busno=%d, write_func_name=%s, read_func_name=%s\n",
   //        __func__, busno, write_func_name, read_func_name);
   bool write_supported = is_function_supported(busno, write_func_name);
   bool read_supported  = is_function_supported(busno, read_func_name);

   if (!write_supported)
      printf("Unsupported write function: %s\n", write_func_name );
   if (!read_supported)
      printf("Unsupported read function: %s\n", read_func_name );

   bool result =write_supported && read_supported;
   // DBGMSG("returning %d", result);
   return result;
}


unsigned long i2c_get_functionality_flags_by_fd(int fd) {
   unsigned long funcs;
   int rc;

   RECORD_IO_EVENT(IE_OTHER, ( rc = ioctl(fd, I2C_FUNCS, &funcs) ) );
   int errsv = errno;
   if (rc < 0)
      report_ioctl_error( errsv, __func__, (__LINE__-3), __FILE__, true /*fatal*/);

   // DBGMSG("Functionality for file %d: %lu, 0x%lx", file, funcs, funcs);
   return funcs;
}


char * i2c_interpret_functionality_into_buffer(unsigned long functionality, Buffer * buf) {
   char * result = "--";

   buf->len = 0;
   int ndx;

   // HACK ALERT: There are 2 entries for bit I2C_FUNC_I2C in functionality_table,
   // one for function name ioctl_read and another for function name ioctl_write
   // These are at indexes 0 and 1.   For our purposes here we only want to check
   // each bit once, so we start at index 1 instead of 0.
   for (ndx =1; ndx < bit_name_ct; ndx++) {
     if (functionality_table[ndx].bit & functionality) {
        // DBGMSG("found bit, ndx=%d", ndx);
        if (buf->len > 0)
           buffer_append(buf, (Byte *) ", ", 2);
        buffer_append(buf, (Byte *) functionality_table[ndx].name, strlen(functionality_table[ndx].name));
     }
   }
   unsigned char terminator = 0x00;
   buffer_append(buf, &terminator, 1);
   result = (char *) buf->bytes;
   return result;
}


//
// EDID Retrieval
//

/* Gets EDID bytes of a monitor on an open I2C device.
 *
 * Arguments:
 *   fd        file descriptor for open /dev/i2c-n
 *   rawedid   buffer in which to return first 128 bytes of EDID
 *
 * Returns:
 *   0        success
 *   <0       error
 */
Global_Status_Code i2c_get_raw_edid_by_fd(int fd, Buffer * rawedid) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Getting EDID for file %d", fd);

   bool conservative = true;

   assert(rawedid->buffer_size >= 128);
   Global_Status_Code gsc;
   int rc;

   rc = i2c_set_addr(fd, 0x50, CALLOPT_ERR_MSG);
   if (rc < 0) {
      gsc = modulate_rc(rc, RR_ERRNO);
      goto bye;
   }
   // 10/23/15, try disabling sleep before write
   if (conservative)
      sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "before write");

   Byte byte_to_write = 0x00;

   int max_tries = 3;
   for (int tryctr = 0; tryctr < max_tries; tryctr++) {
      gsc = invoke_i2c_writer(fd, 1, &byte_to_write);
      if (gsc == 0) {
         gsc = invoke_i2c_reader(fd, 128, rawedid->bytes);
         assert(gsc <= 0);
         if (gsc == 0) {
            rawedid->len = 128;
            if (debug) {
               DBGMSG("call_read returned:");
               buffer_dump(rawedid);
               DBGMSG("edid checksum = %d", edid_checksum(rawedid->bytes) );
            }
            Byte checksum = edid_checksum(rawedid->bytes);
            if (checksum != 0) {
               // possible if successfully read bytes from i2c bus with no monitor
               // attached - the bytes will be junk.
               // e.g. nouveau driver, Quadro card, on blackrock
               DBGTRC(debug, TRACE_GROUP, "Invalid EDID checksum %d, expected 0.", checksum);
               rawedid->len = 0;
               gsc = DDCRC_EDID;
            }
         }
         if (gsc == 0)
            break;
      }
      if (tryctr < max_tries)
         DBGTRC(debug, TRACE_GROUP, "Retrying EDID read.  tryctr=%d, max_tries=%d", tryctr, max_tries);
   }

bye:
   if (gsc < 0)
      rawedid->len = 0;

   DBGTRC(debug, TRACE_GROUP, "Returning %s.  edidbuf contents:", gsc_desc(gsc));
   if (debug || IS_TRACING()) {
      buffer_dump(rawedid);
   }
   return gsc;
}


/* Returns a parsed EDID record for the monitor on an I2C bus.
 *
 * Arguments:
 *   fd          file descriptor for open /dev/i2c-n
 *
 * Returns:
 *   Parsed_Edid, NULL if get_raw_edid_by_fd() fails
 */
Parsed_Edid * i2c_get_parsed_edid_by_fd(int fd) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d\n", fd);
   Parsed_Edid * edid = NULL;
   Buffer * rawedidbuf = buffer_new(128, NULL);

   int rc = i2c_get_raw_edid_by_fd(fd, rawedidbuf);
   if (rc == 0) {
      edid = create_parsed_edid(rawedidbuf->bytes);
      if (debug) {
         if (edid)
            report_parsed_edid(edid, false /* dump hex */, 0);
         else
            DBGMSG("create_parsed_edid() returned NULL");
      }
   }
   else if (rc == DDCRC_EDID) {
      DBGTRC(debug, TRACE_GROUP, "i2c_get_raw_edid_by_fd() returned %s", gsc_desc(rc));

   }
   buffer_free(rawedidbuf, NULL);
   DBGTRC(debug, TRACE_GROUP, "Returning %p", edid);
   return edid;
}


//
// I2C Bus Inspection
//

/* Inspects an I2C bus.
 *
 * Arguments:
 *    bus_info  pointer to Bus_Info struct in which information will be set
 *
 * Returns:
 *    bus_info value passed as argument
 */
Bus_Info * i2c_check_bus(Bus_Info * bus_info) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. busno=%d, buf_info=%p", bus_info->busno, bus_info );

   assert(bus_info != NULL);
   char * marker = bus_info->marker;  // mcmcmp(bus_info->marker... causes compile error
   assert( memcmp(marker,"BINF",4) == 0);
   int file = 0;

   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      bus_info->flags |= I2C_BUS_PROBED;
      file = i2c_open_bus(bus_info->busno, CALLOPT_ERR_MSG);  // returns if failure

      if (file >= 0) {
         bus_info->flags |= I2C_BUS_ACCESSIBLE;
         Byte ddc_addr_flags = 0x00;
         Global_Status_Code gsc = detect_ddc_addrs_by_fd(file, &ddc_addr_flags);
         if (gsc != 0) {
            DBGMSF(debug, "detect_ddc_addrs_by_fd() returned %d", gsc);
            f0printf(FERR, "Failure detecting bus addresses for /dev/i2c-%d: status code=%s\n",
                           bus_info->busno, gsc_desc(gsc));
            goto bye;
         }
         bus_info->flags |= ddc_addr_flags;
         bus_info->functionality = i2c_get_functionality_flags_by_fd(file);
         if (bus_info->flags & I2C_BUS_ADDR_0X50) {
            // Have seen case of nouveau driver with Quadro card where
            // there's a bus that has no monitor but responds to the X50 probe
            // of detect_ddc_addrs_by_fd() and then returns a garbage EDID
            // when the bytes are read in i2c_get_parsed_edid_by_fd()
            // TODO: handle case of i2c_get_parsed_edid_by_fd() returning NULL
            // but should never fail if detect_ddc_addrs_by_fd() succeeds
            bus_info->edid = i2c_get_parsed_edid_by_fd(file);
            // bus_info->flags |= I2C_BUS_EDID_CHECKED;
         }
#ifdef NO
         // test is being made in ddc_displays.c
         if (bus_info->flags & I2C_BUS_ADDR_0X37) {
            // have seen case where laptop display reports addr 37 active, but
            // it doesn't responsd to DDC
            // TODO: sanity check for DDC goes here
            // or make this check at a higher level, since I2c doesn't understand DDC

         }
#endif

      }
   }

bye:
   if (file)
      i2c_close_bus(file, bus_info->busno,  CALLOPT_ERR_ABORT);

   DBGTRC(debug, TRACE_GROUP, "Returning %p, flags=0x%02x", bus_info, bus_info->flags );
   return bus_info;
}


//
// Bus inventory
//

static int _busct = -1;                // number of i2c buses found, -1 if not yet checked
static Bus_Info * _bus_infos = NULL;


/* Checks if an I2C bus with a given number exists.
 *
 * Arguments:
 *    busno     bus number
 *
 * Returns:     true/false
 */
bool i2c_bus_exists(int busno) {
   bool result = false;
   bool debug = false;
   int  errsv;
   char namebuf[20];
   struct stat statbuf;
   int  rc = 0;

#ifdef MOCK_DATA
   if (busno == 0 || busno == 3) {
      DBGMSG("Inserting mock data.  Returning false for bus %d", busno);
      return false;
   }
#endif

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


/* Returns the number of I2C buses on the system, by looking for
 * devices named /dev/i2c-n.
 *
 * Note that no attempt is made to open the devices.
 */
static int _count_i2c_devices() {
   bool debug = false;
   int  busct = 0;

   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_bus_exists(busno))
         busct++;
   }
   DBGTRC(debug, TRACE_GROUP, "Returning %d", busct );
   return busct;
}


/* Determines the number of I2C buses, initializes the _bus_infos array,
 * and probes each of the I2C buses.
 *
 * This function should be called exactly once. However, if it is called
 * more than once it just does nothing.
 *
 * Arguments:   none
 *
 * Returns:     nothing
 *
 * Side effects:
 *   _busct = number of I2C buses
 *   _bus_infos = address of allocated Bus_Info array
 */
static void init_i2c_bus_information() {
   bool debug = false;
   DBGMSF(debug, "Starting" );
   assert( (_busct < 0 && _bus_infos == NULL) || (_busct >= 0 && _bus_infos != NULL));

   if (_busct < 0) {
      _busct = _count_i2c_devices();
      _bus_infos = calloc(_busct, sizeof(Bus_Info));
      DBGMSF(debug, "_bus_infos=%p, _busct=%d", _bus_infos, _busct);

      int busndx = 0;
      int busno = 0;

      for (busno=0; busno < I2C_BUS_MAX; busno++) {
         if (i2c_bus_exists(busno)) {
            // Bus_Info * bus_info = _get_allocated_bus_info(busndx);
            Bus_Info * bus_info = _bus_infos + busndx;
            DBGMSF(debug, "Initializing Bus_Info at %p, busno=%d, busndx=%d", bus_info, busno, busndx);
            memcpy(bus_info->marker, "BINF", 4);
            bus_info->busno = busno;
            bus_info->flags = I2C_BUS_EXISTS;
            i2c_check_bus(bus_info);
            busndx++;
         }
      }
   }
   DBGMSF(debug, "Done");
}


/* Returns the number of /dev/i2c-n devices found on the system.
 *
 * As a side effect, data structures for storing information about
 * the devices are initialized if not already initialized.
 */
int i2c_get_busct() {
   bool debug = false;

   init_i2c_bus_information();

   DBGMSF(debug, "Returning %d", _busct);
   return _busct;
}


//
// Bus_Info retrieval
//

Bus_Info * i2c_get_bus_info_by_index(int busndx) {
   assert(busndx >= 0);
   bool debug = false;
   DBGMSF(debug, "Starting.  busndx=%d", busndx );

   Bus_Info * bus_info = NULL;

   int busct = i2c_get_busct();   // forces initialization of Bus_Info data structs if necessary
   assert(busndx < busct);
   bus_info = (Bus_Info *) _bus_infos + busndx;
   // report_businfo(busInfo);
   if (debug) {
      DBGMSG("flags=0x%02x", bus_info->flags);
      DBGMSG("flags & I2C_BUS_PROBED = 0x%02x", (bus_info->flags & I2C_BUS_PROBED) );
   }
   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      // DBGMSG("Calling check_i2c_bus()");
      i2c_check_bus(bus_info);
   }
   DBGMSF(debug, "busndx=%d, returning %p", busndx, bus_info );
   return bus_info;
}


typedef struct {
   int           busno;
   const char *  mfg_id;
   const char *  model_name;
   const char *  serial_ascii;
   const Byte *  edidbytes;
   Byte          options;
} I2C_Bus_Selector;


void report_i2c_bus_selector(I2C_Bus_Selector * sel, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("I2C_Bus_Selector", sel, depth);
   rpt_int("busno",        NULL, sel->busno, d1);
   rpt_str("mfg_id",       NULL, sel->mfg_id, d1);
   rpt_str("model_name",   NULL, sel->model_name, d1);
   rpt_str("serial_ascii", NULL, sel->serial_ascii, d1);
   rpt_structure_loc("edidbytes", sel->edidbytes, d1);
   if (sel->edidbytes)
      rpt_hex_dump(sel->edidbytes, 128, d2);
}


void init_i2c_bus_selector(I2C_Bus_Selector* sel) {
   assert(sel);
   memset(sel, 0, sizeof(I2C_Bus_Selector));
   sel->busno = -1;
}

// Note: No need for free_i2c_bus_selector() function since strings and memory
// pointed to by selector are always in other data structures


/* Tests if a bus_info table entry matches the criteria of a selector.
 *
 * Arguments:
 *   bus_info    pointer to Bus_Info instance to test
 *   sel         selection criteria
 *
 * Returns:      true/false
 */
bool bus_info_matches_selector(Bus_Info * bus_info, I2C_Bus_Selector * sel) {
   bool debug = false;
   if (debug) {
      DBGMSG("Starting");
      report_businfo(bus_info, 1);
   }

   assert( bus_info && sel);
   assert( sel->busno >= 0   ||
           sel->mfg_id       ||
           sel->model_name   ||
           sel->serial_ascii ||
           sel->edidbytes);

   bool result = false;
   // does the bus represent a valid display?
   if (sel->options & DISPSEL_VALID_ONLY) {
      if (!(bus_info->flags & I2C_BUS_ADDR_0X37))
         goto bye;
   }
   bool some_test_passed = false;

   if (sel->busno >= 0) {
      DBGMSF(debug, "bus_info->busno = %d", bus_info->busno);
      if (sel->busno != bus_info->busno)  {
         result = false;
         goto bye;
      }
      DBGMSF(debug, "busno test passed");
      some_test_passed = true;
   }

   Parsed_Edid * edid = bus_info->edid;  // will be NULL for I2C bus with no monitor

   if (sel->mfg_id && strlen(sel->mfg_id) > 0) {
      if ((!edid) || strlen(edid->mfg_id) == 0 || !streq(sel->mfg_id, edid->mfg_id) ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (sel->model_name && strlen(sel->model_name) > 0) {
      if ((!edid) || strlen(edid->model_name) == 0 || !streq(sel->model_name, edid->model_name) ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (sel->serial_ascii && strlen(sel->serial_ascii) > 0) {
      if ((!edid) || strlen(edid->serial_ascii) == 0 || !streq(sel->serial_ascii, edid->serial_ascii) ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (sel->edidbytes) {
      if ((!edid) || !memcmp(sel->edidbytes, edid->bytes, 128) != 0  ) {
         result = false;
         goto bye;
      }
      some_test_passed = true;
   }
   if (some_test_passed)
      result = true;

bye:
   DBGMSF(debug, "Returning: %s", bool_repr(result));
   return result;
}


/* Finds the first Bus_Info instance that matches a selector
 *
 * Arguments:
 *   sel       pointer to selection criteria
 *
 * Returns:    pointer to Bus_Info instance if found, NULL if not
 */
Bus_Info * find_bus_info_by_selector(I2C_Bus_Selector * sel) {
   assert(sel);
   bool debug = false;
   if (debug) {
      DBGMSG("Starting.");
      report_i2c_bus_selector(sel, 1);
   }

   Bus_Info * bus_info = NULL;
   int busct = i2c_get_busct();   // forces initialization of Bus_Info data structs if necessary
   int busndx = 0;
   for (busndx=0; busndx < busct; busndx++) {
      bus_info = (Bus_Info *) _bus_infos + busndx;
      if (bus_info_matches_selector(bus_info, sel))
         break;
   }
   // DBGMSF(debug, "After loop: busndx=%d, busct=%d", busndx, busct);
   if (busndx >= busct)
      bus_info = NULL;

    DBGMSF(debug, "returning %p", bus_info );
    if (debug && bus_info) {
       report_businfo(bus_info, 1);
    }
    return bus_info;
 }


/* Retrieves bus information by I2C bus number.
 *
 * If the bus information does not already exist in the Bus_Info struct for the
 * bus, it is calculated by calling check_i2c_bus()
 *
 * Arguments:
 *    busno    bus number
 *    findopts
 *
 * Returns:
 *    pointer to Bus_Info struct for the bus,
 *    NULL if busno is greater than the highest bus number
 */
Bus_Info * i2c_get_bus_info(int busno, Byte findopts) {
   bool debug = false;
   DBGMSF(debug, "Starting.  busno=%d, findopts=0x%02x", busno, findopts );
   assert(busno >= 0);

   I2C_Bus_Selector sel;
   init_i2c_bus_selector(&sel);
   sel.busno   = busno;
   sel.options = findopts;
   Bus_Info * bus_info = find_bus_info_by_selector(&sel);

#ifdef OLD   // keep for reuse in general selection code moved to ddc level
   int busct = i2c_get_busct();   // forces initialization of Bus_Info data structs if necessary
   int busndx = 0;
   for (busndx=0; busndx < busct; busndx++) {
      // bus_info = _get_allocated_bus_info(busndx);
      bus_info = (Bus_Info *) _bus_infos + busndx;
      if (busno == bus_info->busno) {
         // report_businfo(busInfo);
         if (debug) {
            DBGMSG("flags=0x%02x", bus_info->flags);
            DBGMSG("flags & I2C_BUS_PROBED = 0x%02x", (bus_info->flags & I2C_BUS_PROBED) );
         }
         if (!(bus_info->flags & I2C_BUS_PROBED)) {
            // DBGMSG("Calling check_i2c_bus()");
            i2c_check_bus(bus_info);
         }
         break;
      }
   }
#endif
   DBGMSF(debug, "busno=%d, returning %p", busno, bus_info );
   return bus_info;
}


/* Retrieves bus information by model name and serial number
 * for the monitor.
 *
 * Arguments:
 *    model     monitor model (as listed in the EDID)
 *    sn        monitor ascii serial number (as listed in the EDID)
 *
 * Returns:
 *    pointer to Bus_Info struct for the bus,
 *    NULL if not found
 */
Bus_Info *
i2c_find_bus_info_by_mfg_model_sn(
      const char * mfg_id,
      const char * model,
      const char * sn,
      Byte findopts)
{
   bool debug = false;
   DBGMSF(debug, "Starting. mfg_id=|%s|, model=|%s|, sn=|%s|", mfg_id, model, sn );
   assert(mfg_id || model || sn);    // loosen the requirements

   I2C_Bus_Selector sel;
   init_i2c_bus_selector(&sel);
   sel.mfg_id       = mfg_id;
   sel.model_name   = model;
   sel.serial_ascii = sn;
   sel.options      = findopts;
   Bus_Info * result = find_bus_info_by_selector(&sel);

   DBGMSF(debug, "Returning: %p", result );
   return result;
}


/* Retrieves bus information using the 128 byte EDID of the monitor on the bus.
 *
 * Arguments:
 *    pEdidBytes  pointer to 128 byte EDID
 *
 * Returns:
 *    pointer to Bus_Info struct for the bus,
 *    NULL if not found
 */
Bus_Info * i2c_find_bus_info_by_edid(const Byte * edidbytes, Byte findopts) {
   bool debug = false;
   DBGMSF(debug, "Starting. edidbytes=%p, findopts=0x%02x", edidbytes, findopts);
   assert(edidbytes);

   I2C_Bus_Selector sel;
   init_i2c_bus_selector(&sel);
   sel.edidbytes   = edidbytes;
   sel.options = findopts;
   Bus_Info * result = find_bus_info_by_selector(&sel);

   DBGMSF(debug, "Returning: %p", result );
   return result;
}


//
// I2C Bus Inquiry
//

/* Checks whether an I2C bus supports DDC.
 *
 * Issues messages if not.
 *
 * Arguments:
 *    busno      I2C bus number
 *    emit_error_msg  if true, write message if error
 *
 * Returns:
 *    true or false
 */
bool i2c_is_valid_bus(int busno, Call_Options callopts) {
   bool emit_error_msg = callopts & CALLOPT_ERR_MSG;
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d, callopts=%s", busno, interpret_call_options(callopts) );
   bool result = false;
   char * complaint = NULL;

   Bus_Info * businfo = i2c_get_bus_info(busno, DISPSEL_NONE);
   if (debug && businfo)
      report_businfo(businfo, 1);

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
      f0printf(FERR, "%s /dev/i2c-%d\n", complaint, busno);
   }
   if (complaint && overridable && (callopts & CALLOPT_FORCE)) {
      f0printf(FERR, "Continuing.  --force option was specified.\n");
      result = true;
   }

   DBGMSF(debug, "Returning %s", bool_repr(result));
   return result;
}


/* Gets the parsed EDID record for the monitor on an I2C bus
 * specified by its bus number.
 *
 * Arguments:
 *   busno        I2C bus number
 *
 * Returns:       Parsed_Edid record, NULL if not found
 */
Parsed_Edid * i2c_get_parsed_edid_by_busno(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);
   Parsed_Edid * edid = NULL;

   Bus_Info * pbus_info = i2c_get_bus_info(busno, DISPSEL_NONE);
   if (pbus_info)
      edid = pbus_info->edid;

   DBGMSF(debug, "Returning: %p", edid);
   return edid;
}


#ifdef REFERENCE
typedef struct {
   Display_Ref * dref;
   Parsed_Edid * edid;
} Display_Info;

typedef struct {
   int ct;
   Display_Info * info_recs;
} Display_Info_List;

#endif


/* Gets list of I2C connected displays in the form expected by
 * higher levels of the program.
 *
 * Note this list may contain displays that do not support DDC.
 *
 * Arguments:   none
 *
 * Returns:     list of displays
 */
Display_Info_List i2c_get_displays() {
   Display_Info_List info_list = {0,NULL};
   Display_Info info_recs[256];
   int busct = i2c_get_busct();
   int cur_display = 0;
   int busndx = 0;
   for (busndx=0; busndx < busct; busndx++) {
      Bus_Info * businfo = i2c_get_bus_info_by_index(busndx);
      if ( (businfo->flags & I2C_BUS_ADDR_0X50) ) {
         Display_Info * pcur = &info_recs[cur_display];
         pcur->dref = create_bus_display_ref(businfo->busno);
         pcur->edid = businfo->edid;
         memcpy(pcur->marker, DISPLAY_INFO_MARKER, 4);
         cur_display++;
      }
   }
   if (cur_display > 0) {
      info_list.info_recs = calloc(cur_display,sizeof(Display_Info));
     memcpy(info_list.info_recs, info_recs, cur_display*sizeof(Display_Info));
     info_list.ct = cur_display;
   }
   // DBGMSG("Done. Returning:");
   // report_display_info_list(&info_list, 0);
   return info_list;
}


//
// Bus Reports
//

/* Reports on a single I2C bus.
 *
 * Arguments:
 *    bus_info    pointer to Bus_Info structure describing bus
 *    depth       logical indentation depth
 *
 * Returns:  nothing
 *
 * The format of the output as well as its extent is controlled by
 * getGlobalMessageLevel().
 */
void report_businfo(Bus_Info * bus_info, int depth) {
   bool debug = false;
   DDCA_Output_Level output_level = get_output_level();
   DBGMSF(debug, "bus_info=%p, output_level=%s", bus_info, output_level_name(output_level));
   assert(bus_info);

   Buffer * buf0 = buffer_new(1000, "report_businfo");

   switch (output_level) {

#ifdef OLD
      case OL_PROGRAM:
         if ( bus_info->flags & I2C_BUS_ADDR_0X50 ) {
            rpt_vstring(
                    depth,
                    "%d:%s:%s:%s",
                    bus_info->busno,
                    bus_info->edid->mfg_id,
                    bus_info->edid->model_name,
                    bus_info->edid->serial_ascii);
         }
         break;
#endif

      case OL_VERBOSE:
         puts("");
         rpt_vstring(depth, "Bus /dev/i2c-%d found:    %s", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_EXISTS));
         rpt_vstring(depth, "Bus /dev/i2c-%d probed:   %s", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_PROBED ));
         if ( bus_info->flags & I2C_BUS_PROBED ) {
            rpt_vstring(depth, "Address 0x37 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X37));
            rpt_vstring(depth, "Address 0x50 present:    %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X50));
            i2c_interpret_functionality_into_buffer(bus_info->functionality, buf0);
            rpt_vstring(depth, "Bus functionality:    %.*s",  buf0->len, buf0->bytes /* buf */);
            if ( bus_info->flags & I2C_BUS_ADDR_0X50) {
               if (bus_info->edid) {
                  report_parsed_edid(bus_info->edid, true /* verbose */, depth);
               }
            }
         }
         break;

      case OL_NORMAL:
         puts("");
         rpt_vstring(depth, "Bus:              /dev/i2c-%d", bus_info->busno);
         rpt_vstring(depth, "Supports DDC:     %s", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X37));
         if ( (bus_info->flags & I2C_BUS_ADDR_0X50) && bus_info->edid) {
            report_parsed_edid(bus_info->edid, false /* verbose */, depth);
         }
         break;

      default:    // OL_TERSE
         assert (output_level == OL_TERSE);
         puts("");
         rpt_vstring(depth, "Bus:                     /dev/i2c-%d\n", bus_info->busno);
         if ( (bus_info->flags & I2C_BUS_PROBED)     &&
              (bus_info->flags & I2C_BUS_ADDR_0X37)  &&
              (bus_info->flags & I2C_BUS_ADDR_0X50)  &&
              (bus_info->edid)
            )
         {
            Parsed_Edid * edid = bus_info->edid;
            // what if edid->mfg_id, edid->model_name, or edid->serial_ascii are NULL ??
            rpt_vstring(depth, "Monitor:                 %s:%s:%s",
                   edid->mfg_id, edid->model_name, edid->serial_ascii);
         }
         break;
      }  // switch

   buffer_free(buf0, "report_businfo");
   DBGMSF(debug, "Done");
}


/* Reports a single active display.
 *
 * Output is written to the current report destination.
 *
 * Arguments:
 *    businfo     bus record
 *    depth       logical indentation depth
 *
 * Returns: nothing
 */
void i2c_report_active_display(Bus_Info * businfo, int depth) {
   DDCA_Output_Level output_level = get_output_level();
   rpt_vstring(depth, "I2C bus:             /dev/i2c-%d", businfo->busno);

   if (output_level >= OL_NORMAL)
   rpt_vstring(depth, "Supports DDC:        %s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level >= OL_VERBOSE) {
      rpt_vstring(depth+1, "I2C address 0x37 (DDC)  present: %-5s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X37));
      rpt_vstring(depth+1, "I2C address 0x50 (EDID) present: %-5s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X50));
   }

#ifdef OLD
   if (output_level == OL_TERSE || output_level == OL_PROGRAM)
#else
   if (output_level == OL_TERSE)
#endif
   rpt_vstring(depth, "Monitor:             %s:%s:%s",  businfo->edid->mfg_id,
                                               businfo->edid->model_name,
                                               businfo->edid->serial_ascii);
   if (output_level >= OL_NORMAL && businfo->edid) {
      bool verbose = (output_level >= OL_VERBOSE);
      report_parsed_edid(businfo->edid, verbose, depth);
   }
}


/* Reports a single active display, specified by its bus number.
 *
 * Output is written to the current report destination.
 *
 * Arguments:
 *    busno       bus number (must be valid)
 *    depth       logical indentation depth
 *
 * Returns: nothing
 */
void i2c_report_active_display_by_busno(int busno, int depth) {
   Bus_Info * curinfo = i2c_get_bus_info(busno, DISPSEL_NONE);
   assert(curinfo);
   i2c_report_active_display(curinfo, depth);
}


/* Reports on a single I2C bus.
 *
 * Arguments:
 *    busno       bus number
 *
 * Returns:  nothing
 *
 * The format of the output is determined by a call to getOutputFormat().
 */
void i2c_report_bus(int busno) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno );
   assert(busno >= 0);

  int busct = i2c_get_busct();
  if (busno >= busct)
     fprintf(stderr, "Invalid I2C bus number: %d\n", busno);
  else {
     Bus_Info * busInfo = i2c_get_bus_info(busno, DISPSEL_NONE);
     report_businfo(busInfo, 0);
  }

  DBGMSF(debug, "Done");
}


/* Reports I2C buses.
 *
 * Arguments:
 *    report_all    if false, only reports buses with monitors
 *                  if true, reports all detected buses
 *    depth         logical indentation depth
 *
 * Returns:
 *    count of reported buses
 *
 * Used by query-sysenv.c
 */
int i2c_report_buses(bool report_all, int depth) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. report_all=%s\n", bool_repr(report_all));

   int busct = i2c_get_busct();
   int reported_ct = 0;

   puts("");
   if (report_all)
      rpt_vstring(depth,"Detected I2C buses:");
   else
      rpt_vstring(depth, "I2C buses with monitors detected at address 0x50:");

   int busno = 0;
   for (busno=0; busno < busct; busno++) {
      Bus_Info * busInfo = i2c_get_bus_info(busno, DISPSEL_NONE);
      if ( (busInfo->flags & I2C_BUS_ADDR_0X50) || report_all) {
         report_businfo(busInfo, depth);
         reported_ct++;
      }
   }
   if (reported_ct == 0)
      rpt_vstring(depth, "   No buses\n");

   DBGTRC(debug, TRACE_GROUP, "Done. Returning %d\n", reported_ct);
   return reported_ct;
}

