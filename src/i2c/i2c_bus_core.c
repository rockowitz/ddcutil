/* i2c_bus_core.c
 *
 * Created on: Jun 13, 2014
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
// On Fedora, i2c-dev.h is minimal.  i2c.h is required for struct i2c_msg and
// other stuff.  On Ubuntu and SuSE, including both causes redefinition errors.
// If I2C_FUNC_I2C is not defined, the definition is present in the full version
// of i2c-dev.h but not in the abbreviated version, so i2c.h must be included.
#include <linux/i2c-dev.h>
#ifndef I2C_FUNC_I2C
#include <linux/i2c.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <util/debug_util.h>
#include <util/report_util.h>
#include <util/string_util.h>

#include <base/ddc_errno.h>
#include <base/common.h>
#include <base/displays.h>
#include <base/edid.h>
#include <base/linux_errno.h>
#include <base/msg_control.h>
#include <base/parms.h>
#include <base/status_code_mgt.h>
#include <base/util.h>

#include <i2c/i2c_do_io.h>

#include <i2c/i2c_bus_core.h>


// maximum number of i2c buses this code supports,
// i.e. it only looks for /dev/i2c-0 .. /dev/i2c-31
#define I2C_BUS_MAX 32

// Addresses on an I2C bus are 7 bits in size
#define BUS_ADDR_MAX 128


// Trace class for this file
static Trace_Group TRACE_GROUP = TRC_I2C;
// static TraceControl bus_core_trace_level = NEVER;   // old way of controlling tracing


//
// Bus inventory - retrieve and inspect bus information
//

static int _busct = -1;                // number of i2c buses found, -1 if not yet checked
typedef Bus_Info Bus_Info_Array[];     // type for an array of Bus_Info
static Bus_Info_Array *  _bus_infos;   // pointer to array of Bus_Info


// Data structure allocation

/* Returns the Bus_Info structure for a bus.
 *
 * Arguments:  busno   bus number (must be valid)
 *
 * Returns:    Bus_Info structure for the bus
 */
static Bus_Info * _get_allocated_Bus_Info(int busno) {
   bool debug = false;
   if (debug)
      DBGMSG("busno=%d, _busct=%d", busno, _busct );
   assert(_bus_infos != NULL && _busct >= 0);   // Check initialized
   assert(busno >= 0 && busno < _busct);

   Bus_Info_Array * bia = _bus_infos;
   Bus_Info * bus_info  = (void *)bia + busno*sizeof(Bus_Info);

   if (debug)
       DBGMSG("Returning %p", bus_info );
   return bus_info;
}


/* Returns the number of I2C buses on the system, by looking for
 * devices named /dev/i2c-n.
 *
 * Note that no attempt is made to open the devices.
 *
 * Note also the assumption that all buses are numbered
 * consecutively starting from 0.
 */
static int _get_i2c_busct() {
   bool debug = false;
   int  errsv;
   int  busno = 0;
   char namebuf[20];
   struct stat statbuf;
   int  rc = 0;

   for (busno=0; busno < I2C_BUS_MAX && rc==0; busno++) {
      sprintf(namebuf, "/dev/i2c-%d", busno);
      errno = 0;
      rc = stat(namebuf, &statbuf);
      errsv = errno;
      if (debug) {
         if (rc == 0) {
            DBGMSG("Found %s", namebuf);
         }
         else {
            DBGMSG("stat(%s) returned %d, errno=%s", namebuf, rc, linux_errno_desc(errsv) );
         }
      }
   }
   int result = busno-1;
   if (debug)
      DBGMSG("Returning %d", result );
   return result;
}


/* Allocates an array of Bus_Info and initializes each entry
 *
 * Arguments:
 *   ct   number of entries
 *
 * Returns:
 *   pointer to Bus_Info array
 *
 * Side effects:
 *   _bus_infos = address of allocated Bus_Info array
 */
static Bus_Info_Array * _allocate_Bus_Info_Array(int ct) {
   bool debug = false;
   if (debug) 
      DBGMSG("Starting. ct=%d", ct );
   Bus_Info_Array * bia = (Bus_Info_Array*) call_calloc(ct, sizeof(Bus_Info), "_allocate_Bus_Info_Array");
   if (debug) DBGMSG("&bia=%p, bia=%p ", &bia, bia);
   _bus_infos = bia;
   int busno = 0;
   for (; busno < ct; busno++) {
      Bus_Info * bus_info = _get_allocated_Bus_Info(busno);
      // if (debug) DBGMSG("Putting marker in Bus_Info at %p", bus_info );
      memcpy(bus_info->marker, "BINF", 4);
      bus_info->busno = busno;
      // I2C_BUS_PRESENT currently always set.  Might not be set if it turns out that
      // I2C bus numbers can be non-consecutive, and the same Bus_Info_Array is used
      bus_info->flags = I2C_BUS_EXISTS;
   }
   if (debug) 
      DBGMSG("Returning %p", bia);
   return bia;
}


/* Determines the number of I2C buses and initializes the Bus_Info array
 *
 * This function should be called exactly once.
 *
 * Arguments:   none
 *
 * Returns:     nothing
 *
 * Side effects:
 *   _busct = number of I2C buses
 *   _bus_infos = address of allocated Bus_Info array
 */
static void _init_bus_infos_and_busct() {
   // DBGMSG("Starting" );
   assert( _busct < 0 && _bus_infos == NULL);  // check not yet initialized
   _busct = _get_i2c_busct();
   _allocate_Bus_Info_Array(_busct);
   // DBGMSG("Done" );
}


// Ensures that global variables _busct and _bus_infos are initialized
//
// Allows for lazy initialization.
// Useless optimization, should eliminate.
static void _ensure_bus_infos_and_busct_initialized() {
   // DBGMSG("Starting" );
   assert( (_busct < 0 && _bus_infos == NULL) || (_busct >= 0 && _bus_infos != NULL));
   if (_busct < 0)
      _init_bus_infos_and_busct();
   assert( _busct >= 0 && _bus_infos);
   // DBGMSG("Done" );
}


/* Returns the number of /dev/i2c-n devices found on the system.
 *
 * As a side effect, data structures for storing information about
 * the devices are initialized if not already initialized.
 */
int i2c_get_busct() {
   bool debug = false;
   assert( (_busct < 0 && _bus_infos == NULL) || (_busct >= 0 && _bus_infos != NULL));

   _ensure_bus_infos_and_busct_initialized();

   DBGMSF(debug, "Returning %d", _busct);
   assert(_busct >= 0 && _bus_infos != NULL);
   return _busct;
}




// I2C device inspection

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
 * This "exploratory" function is not currently used.
 *
 * TODO: excluded reserved I2C bus addresses from check
 */
bool * detect_all_addrs_by_fd(int fd) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting. fd=%d", fd);
   assert (fd >= 0);
   bool * addrmap = NULL;

   unsigned char byte_to_write = 0x00;
   int addr;
   addrmap = call_calloc(BUS_ADDR_MAX, sizeof(bool), "detect_all_addrs" );
   //bool addrmap[128] = {0};

   for (addr = 3; addr < BUS_ADDR_MAX; addr++) {
      int rc;
      i2c_set_addr(fd, addr);
      rc = invoke_i2c_reader(fd, 1, &byte_to_write);
      if (rc >= 0)
         addrmap[addr] = true;
   }

   if (debug)
      DBGMSG("Returning %p", addrmap);
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
 * This "exploratory" function is not currently used.
 */
bool * detect_all_addrs(int busno) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting. busno=%d", busno);
   int file = i2c_open_bus(busno, RETURN_ERROR_IF_FAILURE);
   bool * addrmap = NULL;

   if (file >= 0) {
      addrmap = detect_all_addrs_by_fd(file);
      i2c_close_bus(file, busno, EXIT_IF_FAILURE);
   }

   if (debug)
      DBGMSG("Returning %p", addrmap);
   return addrmap;
}


/* Checks DDC related addresses on an I2C bus to see if the address is active.
 * The bus device has already been opened.
 *
 * Arguments:
 *   file  file descriptor for open bus object
 *
 * Returns:
 *   Returns byte with flags possibly set:
 *    I2C_BUS_ADDR_0x50        true if addr x50 responds (EDID)
 *    I2C_BUS_ADDR_0x37        true if addr x37 responds (DDC commands)
 */
Byte detect_ddc_addrs_by_fd(int file) {
   bool debug = false;
   DBGMSF(debug, "Starting.  busno=%d", file);
   assert(file >= 0);
   unsigned char result = 0x00;

   // result |= I2C_BUS_PRESENT;   // file >= 0 => bus exists

   Byte    readbuf;  //  1 byte buffer
   int rc;

   i2c_set_addr(file, 0x50);
   rc = invoke_i2c_reader(file, 1, &readbuf);
   if (rc >= 0)
      result |= I2C_BUS_ADDR_0X50;

   i2c_set_addr(file, 0x37);
   rc = invoke_i2c_reader(file, 1, &readbuf);
   // DBGMSG("call_read() returned %d", rc);
   if (rc >= 0 || rc == DDCRC_READ_ALL_ZERO)   // 11/2015: DDCRC_READ_ALL_ZERO currently set only in ddc_packet_io.c
      result |= I2C_BUS_ADDR_0X37;

   // result |= I2C_BUS_ADDRS_CHECKED;

   DBGMSGF(debug, "Done.  Returning 0x%02x", result);
   return result;
}

#ifdef UNUSED
/* Checks DDC related addresses on an I2C bus.
 *
 * Arguments:
 *   busno   bus number
 *
 * Returns:
 *   I2C_BUS_ flags byte (see detect_addrs_by_fd() for details)
 *   0x00 if bus cannot be opened
 */
Byte detect_ddc_addrs_by_busno(int busno) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting.  busno=%d", busno);

   unsigned char result = 0x00;
   int file = i2c_open_bus(busno, RETURN_ERROR_IF_FAILURE);
   if (file >= 0) {
      result = detect_ddc_addrs_by_fd(file);
      i2c_close_bus(file, busno, EXIT_IF_FAILURE);
   }

   if (debug)
      DBGMSG("Done.  Returning 0x%02x", result);
   return result;
}
#endif

/* Calculates bus information for an I2C bus.
 *
 * Arguments:
 *    bus_info  pointer to Bus_Info struct in which information will be set
 *
 * Returns:
 *    bus_info value passed as argument
 */
Bus_Info * i2c_check_bus(Bus_Info * bus_info) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d, buf_info=%p", bus_info->busno, bus_info );

   assert(bus_info != NULL);
   char * marker = bus_info->marker;  // mcmcmp(bus_info->marker... causes compile error
   assert( memcmp(marker,"BINF",4) == 0);

   if (!(bus_info->flags & I2C_BUS_PROBED)) {
      bus_info->flags |= I2C_BUS_PROBED;
      int file = i2c_open_bus(bus_info->busno, RETURN_ERROR_IF_FAILURE);

      if (file >= 0) {
         bus_info->flags |= I2C_BUS_ACCESSIBLE;
         bus_info->flags |= detect_ddc_addrs_by_fd(file);
         bus_info->functionality = i2c_get_functionality_flags_by_fd(file);
         if (bus_info->flags & I2C_BUS_ADDR_0X50) {
            // Have seen case of nouveau driver with Quadro card where
            // there's a bus that has no monitor but responds to the X50 probe
            // of detect_ddc_addrs_by_fd() and then returns a garbage EDID
            // when the bytes are read in i2c_get_parsed_edid_by_fd()
            bus_info->edid = i2c_get_parsed_edid_by_fd(file);
            // bus_info->flags |= I2C_BUS_EDID_CHECKED;
         }
         i2c_close_bus(file, bus_info->busno,  EXIT_IF_FAILURE);
      }
   }

   DBGMSF(debug, "Returning %p, flags=0x%02x", bus_info, bus_info->flags );
   return bus_info;
}


/* Retrieves bus information by I2C bus number.
 *
 * If the bus information does not already exist in the Bus_Info struct for the
 * bus, it is calculated by calling check_i2c_bus()
 *
 * Arguments:
 *    busno    bus number
 *
 * Returns:
 *    pointer to Bus_Info struct for the bus,
 *    NULL if busno is greater than the highest bus number
 */
Bus_Info * i2c_get_bus_info(int busno) {
   assert(busno >= 0);
   // bool debug = adjust_debug_level(false, bus_core_trace_level);
   bool debug = false;
   DBGMSF(debug, "Starting.  busno=%d", busno );

   Bus_Info * bus_info = NULL;

   int busct = i2c_get_busct();   // forces initialization of Bus_Info data structs if necessary
   if (busno < busct) {
      bus_info = _get_allocated_Bus_Info(busno);
      // report_businfo(busInfo);
      if (debug) {
         DBGMSG("flags=0x%02x", bus_info->flags);
         DBGMSG("flags & I2C_BUS_PROBED = 0x%02x", (bus_info->flags & I2C_BUS_PROBED) );
      }
      if (!(bus_info->flags & I2C_BUS_PROBED)) {
         // DBGMSG("Calling check_i2c_bus()");
         i2c_check_bus(bus_info);
      }
   }
   DBGMSF(debug, "Returning %p", bus_info );
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
Bus_Info * i2c_find_bus_info_by_model_sn(const char * model, const char * sn) {
   // DBGMSG("Starting. mode=%s, sn=%s", model, sn );
   Bus_Info * result = NULL;
   int busct = i2c_get_busct();
   int busno;
   for (busno=0; busno<busct; busno++) {
      Bus_Info * curinfo = i2c_get_bus_info(busno);  // ensures probed
      // report_businfo(curinfo);
      // Edid * pEdid = curinfo->edid;
      Parsed_Edid * edid = curinfo->edid;
      if (edid) {        // if there's a monitor on the bus
         // report_edid_summary(pEdid, false);
         if (streq(edid->model_name, model) && streq(edid->serial_ascii, sn)) {
            result = curinfo;
           break;
         }
      }
   }
   // DBGMSG("Returning: %p", result );
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
Bus_Info * i2c_find_bus_info_by_edid(const Byte * pEdidBytes) {
   // DBGMSG("Starting. mode=%s, sn=%s", model, sn );
  Bus_Info * result = NULL;
  int busct = i2c_get_busct();
  int busno;
  for (busno=0; busno<busct; busno++) {
     Bus_Info * curinfo = i2c_get_bus_info(busno);
     // report_businfo(curinfo);
     // Edid * pEdid = curinfo->edid;
     Parsed_Edid * pEdid = curinfo->edid;
     if (pEdid) {        // if there's a monitor on the bus
        // report_edid_summary(pEdid, false);
        if ( memcmp(pEdid->bytes, pEdidBytes, 128) == 0)  {
           result = curinfo;
          break;
        }
     }
  }

  // DBGMSG("Returning: %p", result );
  return result;

}


/* Checks whether an I2C bus supports DDC.
 *
 * Issues messages if not.
 *
 * Arguments:
 *    busno      I2C bus number
 *
 * Returns:
 *    true or false
 */
bool i2c_is_valid_bus(int busno, bool emit_error_msg) {
   bool result = false;
   char * complaint = NULL;

   Bus_Info * businfo = i2c_get_bus_info(busno);
   // if (businfo)
   //    report_businfo(businfo);
   if (!businfo)
      complaint = "I2C bus not found:";
   else if (!(businfo->flags & I2C_BUS_EXISTS))
      complaint = "I2C bus not found: /dev/i2c-%d\n";
   else if (!(businfo->flags & I2C_BUS_ACCESSIBLE))
      complaint = "Inaccessible I2C bus:";
   else if (!(businfo->flags & I2C_BUS_ADDR_0X50))
      complaint = "No monitor found on bus";
   else if (!(businfo->flags & I2C_BUS_ADDR_0X37))
      complaint = "Cannot communicate DDC on bus address 0x37 for I2C bus";
   else
      result = true;

   if (complaint && emit_error_msg) {
      fprintf(stderr, "%s /dev/i2c-%d\n", complaint, busno);
   }
   // DBGMSG("returning %d", result);
   return result;
}


//
// Bus Reports
//

#ifdef DEPRECATED
// use get_ParsedEdid_by_busno
DisplayIdInfo* get_bus_display_id_info(int busno) {
   DisplayIdInfo * pIdInfo = NULL;

      Parsed_Edid* edid = i2c_get_parsed_edid_by_busno(busno);
      if (edid) {
         pIdInfo = calloc(1, sizeof(DisplayIdInfo));
         memcpy(pIdInfo->mfg_id,       edid->mfg_id,       sizeof(pIdInfo->mfg_id)      );
         memcpy(pIdInfo->model_name,   edid->model_name,   sizeof(pIdInfo->model_name)  );
         memcpy(pIdInfo->serial_ascii, edid->serial_ascii, sizeof(pIdInfo->serial_ascii));
         memcpy(pIdInfo->edid_bytes,   edid->bytes,        128                          );
      }

   return pIdInfo;
}
#endif

Parsed_Edid * i2c_get_parsed_edid_by_busno(int busno) {
   Parsed_Edid * edid = NULL;

   Bus_Info * pbus_info = i2c_get_bus_info(busno);
   if (pbus_info)
      edid = pbus_info->edid;

   return edid;
}


// TODO: convert to use report functions

/* Reports on a single I2C bus.
 *
 * Arguments:
 *    bus_info    pointer to Bus_Info structure describing bus
 *    fp          output file pointer - where output will be written
 *
 * Returns:  nothing
 *
 * The format of the output is controlled by a call to getOutputFormat().
 * fp ignored unless getOutputFormat() is OUTPUT_PROG_VCP or OUTPUT_PROG_BUSINFO
 *
 * The extent of information reported (as opposed to its format) is affected
 * by getGlobalMessageLevel().
 */
static void report_businfo(Bus_Info * bus_info, int depth) {
   // TODO: implement depth
   // bool debug = adjust_debug_level(false, bus_core_trace_level);
   bool debug = false;
   Output_Level output_level = get_output_level();
   if (debug)
      DBGMSG("bus_info=%p, output_level=%s", bus_info, output_level_name(output_level));
   assert(bus_info);

   Buffer * buf0 = buffer_new(1000, "report_businfo");

   // bool showAll = false;

   switch (output_level) {

      case OL_PROGRAM:
         if ( bus_info->flags & I2C_BUS_ADDR_0X50 ) {
            printf(
                    "%d:%s:%s:%s\n",
                    bus_info->busno,
                    bus_info->edid->mfg_id,
                    bus_info->edid->model_name,
                    bus_info->edid->serial_ascii);

         }
         break;

      case OL_VERBOSE:
         printf("\nBus /dev/i2c-%d found:    %s\n", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_EXISTS));
         printf(  "Bus /dev/i2c-%d probed:   %s\n", bus_info->busno, bool_repr(bus_info->flags&I2C_BUS_PROBED ));
         if ( bus_info->flags & I2C_BUS_PROBED ) {
            printf("Address 0x37 present:    %s\n", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X37));
            printf("Address 0x50 present:    %s\n", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X50));
            i2c_interpret_functionality_into_buffer(bus_info->functionality, buf0);
            printf("Bus functionality:    %.*s\n",  buf0->len, buf0->bytes /* buf */);
            if ( bus_info->flags & I2C_BUS_ADDR_0X50) {
               if (bus_info->edid) {
                  report_parsed_edid(bus_info->edid, true /* verbose */, depth);
               }
            }
         }
         break;

      case OL_NORMAL:
         printf("\nBus:              /dev/i2c-%d\n", bus_info->busno);
         printf(  "Supports DDC:     %s\n", bool_repr(bus_info->flags & I2C_BUS_ADDR_0X37));
         if ( (bus_info->flags & I2C_BUS_ADDR_0X50) && bus_info->edid) {
            report_parsed_edid(bus_info->edid, false /* verbose */, depth);
         }
         break;

      default:    // OL_TERSE
         assert (output_level == OL_TERSE);
         printf("\nBus:                     /dev/i2c-%d\n", bus_info->busno);
         if ( (bus_info->flags & I2C_BUS_PROBED)     &&
              (bus_info->flags & I2C_BUS_ADDR_0X37)  &&
              (bus_info->flags & I2C_BUS_ADDR_0X50)  &&
              (bus_info->edid)
            )
         {
            Parsed_Edid * edid = bus_info->edid;
            // what if edid->mfg_id, edid->model_name, or edid->serial_ascii are NULL ??
            printf("Monitor:                 %s:%s:%s\n",
                   edid->mfg_id, edid->model_name, edid->serial_ascii);
         }
         break;
      }  // switch

   buffer_free(buf0, "report_businfo");
   if (debug)
      DBGMSG("Done");
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
   Output_Level output_level = get_output_level();
   rpt_printf(depth, "Bus:                 /dev/i2c-%d", businfo->busno);

   if (output_level >= OL_VERBOSE)
   rpt_printf(depth, "Supports DDC:        %s", bool_repr(businfo->flags & I2C_BUS_ADDR_0X37));

   if (output_level == OL_TERSE || output_level == OL_PROGRAM)
   rpt_printf(depth, "Monitor:             %s:%s:%s",  businfo->edid->mfg_id,
                                               businfo->edid->model_name,
                                               businfo->edid->serial_ascii);
   if (output_level >= OL_NORMAL) {
      bool dump_edid = (output_level >= OL_VERBOSE && businfo->edid);
      report_parsed_edid(businfo->edid, dump_edid /* verbose */, depth);
   }
}


/* Reports a single active display.
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
   Bus_Info * curinfo = i2c_get_bus_info(busno);
   assert(curinfo);
   i2c_report_active_display(curinfo, depth);
}


/* Reports on a single I2C bus.
 *
 * Arguments:
 *    busno       bus number
 *    fp          output file pointer - where output will be written
 *
 * Returns:  nothing
 *
 * The format of the output is determined by a call to getOutputFormat().
 */
void i2c_report_bus(int busno) {
   // bool debug = adjust_debug_level(false, bus_core_trace_level);
   bool debug = false;
   if (debug)
      DBGMSG("Starting. busno=%d", busno );
   assert(busno >= 0);

  int busct = i2c_get_busct();
  if (busno >= busct)
     fprintf(stderr, "Invalid I2C bus number: %d\n", busno);
  else {
     Bus_Info * busInfo = i2c_get_bus_info(busno);
     report_businfo(busInfo, 0);
  }

  if (debug)
     DBGMSG("Done");
}


/* Reports I2C buses.
 *
 * Arguments:
 *    report_all    if false, only reports buses with monitors
 *                  if true, reports all detected buses
 *
 * Returns:
 *    count of reported buses
 *
 * The format of the output is determined by a call to getOutputFormat().
 */
int i2c_report_buses(bool report_all) {
   bool debug = false;
   Trace_Group tg = TRACE_GROUP;
   if (debug) tg = 0xff;
   TRCMSGTG(tg, "Starting. report_all=%s\n", bool_repr(report_all));

   Output_Level output_level = get_output_level();
   int busct = i2c_get_busct();
   int reported_ct = 0;
   if (output_level != OL_PROGRAM) {
      if (report_all)
         printf("\nDetected I2C buses:\n");
      else
         printf("\nI2C buses with monitors detected at address 0x50:\n");
   }
   int busno = 0;
   for (busno=0; busno < busct; busno++) {
      Bus_Info * busInfo = i2c_get_bus_info(busno);
      if ( (busInfo->flags & I2C_BUS_ADDR_0X50) || report_all) {
         report_businfo(busInfo, 0);
         reported_ct++;
      }
   }
   if (reported_ct == 0)
      printf("   No buses\n");

   TRCMSGTG(tg, "Done. Returning %d\n", reported_ct);
   return reported_ct;
}

Display_Info_List i2c_get_valid_displays() {
   Display_Info_List info_list = {0,NULL};
   Display_Info info_recs[256];
   int busct = i2c_get_busct();
   int cur_display = 0;
   int busno = 0;
   for (busno=0; busno < busct; busno++) {
      Bus_Info * businfo = i2c_get_bus_info(busno);
      if ( (businfo->flags & I2C_BUS_ADDR_0X50) ) {
         Display_Info * pcur = &info_recs[cur_display];
         pcur->dref   = create_bus_display_ref(businfo->busno);
         pcur->edid = businfo->edid;
         cur_display++;
      }
   }
   info_list.info_recs = calloc(cur_display,sizeof(Display_Info));
   memcpy(info_list.info_recs, info_recs, cur_display*sizeof(Display_Info));
   info_list.ct = cur_display;
   // DBGMSG("Done. Returning:");
   // report_display_info_list(&info_list, 0);
   return info_list;
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


//
// Basic I2C bus operations
//

/* Open an I2C bus
 *
 * Arguments:
 *   busno            I2C bus number
 *   failure_action   exit if failure?
 *
 * Returns:
 *   file descriptor ( > 0) if success
 *   -errno if failure and failure_action == RETURN_ERROR_IF_FAILURE
 *
 */
int i2c_open_bus(int busno, Failure_Action failure_action) {
   bool debug = false;
   if (debug)
      DBGMSG("busno=%d", busno);
   char filename[20];
   int  file;

   snprintf(filename, 19, "/dev/i2c-%d", busno);
   RECORD_IO_EVENT(
         IE_OPEN,
         ( file = open(filename, O_RDWR) )
         );
   // per man open:
   // returns file descriptor if successful
   // -1 if error, and errno is set
   int errsv = errno;
   if (file < 0) {
      if (failure_action == EXIT_IF_FAILURE) {
         TERMINATE_EXECUTION_ON_ERROR("Open failed. errno=%s\n", linux_errno_desc(errsv));
      }
      fprintf(stderr, "Open failed for %s: errno=%s\n", filename, linux_errno_desc(errsv));
      file = -errno;
   }
   return file;
}


/* Closes an open I2C bus device.
 *
 * Arguments:
 *   fd     file descriptor
 *   busno  bus number (for error messages)
 *          if -1, ignore
 *   failure_action  if true, exit if close fails
 *
 * Returns:
 *    0 if success
 *    -errno if close fails and exit on failure was not specified
 */
int i2c_close_bus(int fd, int busno, Failure_Action failure_action) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting. fd=%d", fd);
   errno = 0;
   int rc = 0;
   RECORD_IO_EVENT(IE_CLOSE, ( rc = close(fd) ) );
   int errsv = errno;
   if (rc < 0) {
      // EBADF  fd isn't a valid open file descriptor
      // EINTR  close() interrupted by a signal
      // EIO    I/O error
      char workbuf[80];
      if (busno >= 0)
         snprintf(workbuf, 80,
                  "Close failed for bus /dev/i2c-%d. errno=%s",
                  busno, linux_errno_desc(errsv));
      else
         snprintf(workbuf, 80,
                  "Bus device close failed. errno=%s",
                  linux_errno_desc(errsv));

      if (failure_action == EXIT_IF_FAILURE)
         TERMINATE_EXECUTION_ON_ERROR(workbuf);

      fprintf(stderr, "%s\n", workbuf);

      rc = errsv;
   }
   return rc;
}


void i2c_set_addr(int file, int addr) {
   int rc = 0;
   RECORD_IO_EVENT(
         IE_OTHER,
         ( rc = ioctl(file, I2C_SLAVE, addr) )
        );
   if (rc < 0){
      report_ioctl_error(errno, __func__, __LINE__-2, __FILE__, true /*fatal*/);
   }
}


//
// Bus functionality
//

typedef
struct {
        unsigned long bit;
        char *        name;
        char *        function_name;
} I2C_Func_Table_Entry;

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
         // printf("Unrecognized function name: %s\n", funcname);
         // exit(1);
      }
      if (busno < 0 || busno >= i2c_get_busct() ) {
         TERMINATE_EXECUTION_ON_ERROR("Invalid bus: /dev/i2c-%d\n", busno);
         // printf("Invalid bus: /dev/i2c-%d\n", busno);
         // exit(1);
      }

      // DBGMSG("functionality=0x%lx, func_table_entry->bit=-0x%lx", bus_infos[busno].functionality, func_table_entry->bit);
      Bus_Info * bus_info = i2c_get_bus_info(busno);
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

   // long start_time = 0;
   // if (gather_timing_stats)
   //    start_time = cur_realtime_nanosec();
   // int rc = ioctl(fd, I2C_FUNCS, &funcs);
   // int errsv = errno;
   // if (gather_timing_stats) {
   //    pTimingStats->total_call_nanosecs += (cur_realtime_nanosec()-start_time);
   //    pTimingStats->total_call_ct++;
   // }
   RECORD_IO_EVENT(IE_OTHER, ( rc = ioctl(fd, I2C_FUNCS, &funcs) ) );
   int errsv = errno;
   if (rc < 0)
      report_ioctl_error( errsv, __func__, (__LINE__-3), __FILE__, true /*fatal*/);

   // DBGMSG("Functionality for file %d: %lu, 0x%lx", file, funcs, funcs);
   return funcs;
}

#ifdef UNUSED
unsigned long get_i2c_functionality_flags_by_busno(int busno) {
   unsigned long funcs;
   int           file;

   file = i2c_open_bus(busno, EXIT_IF_FAILURE);
   funcs = i2c_get_functionality_flags_by_fd(file);
   i2c_close_bus(file, busno, EXIT_IF_FAILURE);

   // DBGMSG("Functionality for bus %d: %lu, 0x%lx", busno, funcs, funcs);
   return funcs;
}
#endif



char * i2c_interpret_functionality_into_buffer(unsigned long functionality, Buffer * buf) {
   char * result = "--";

   buf->len = 0;
   int ndx = 0;
   for (ndx =0; ndx < bit_name_ct; ndx++) {
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


#ifdef OLD
void show_functionality(int busno) {
   unsigned long funcs = get_i2c_functionality_flags_by_busno(busno);
   char * buf = interpret_functionality(funcs);
   printf("Functionality for bus /dev/i2c-%d: %s\n\n", busno, buf);
   call_free(buf, "show_functionality");
}
#endif


//
// EDID
//

/* retrieve edid with i2c calls known to work
 *
 * Arguments:
 *   fd       file descriptor for open bus
 *   rawedid  buffer in which to return 128 byte edid
 *
 * Returns:
 *   0        success
 *   <0       error
 */
Global_Status_Code i2c_get_raw_edid_by_fd(int fd, Buffer * rawedid) {
   bool debug = false;
   DBGMSF(debug, "Getting EDID for file %d", fd);

   bool conservative = false;

   assert(rawedid->buffer_size >= 128);
   Global_Status_Code gsc;

   i2c_set_addr(fd, 0x50);
   // 10/23/15, try disabling sleep before write
   if (conservative)
      sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "before write");

   Byte byte_to_write = 0x00;

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
            DBGMSF(debug, "Invalid EDID checksum %d, expected 0.\n", checksum);
            rawedid->len = 0;
            gsc = DDCRC_EDID;
         }
      }
   }

   if (gsc < 0)
      rawedid->len = 0;

   if (debug) {
      DBGMSG("Returning %d.  edidbuf contents:", gsc);
      buffer_dump(rawedid);
   }
   return gsc;
}


/* Returns the EDID bytes for the monitor on an I2C bus.
 *
 * Arguments:
 *   fd          file descriptor for open /dev/i2c-n
 *   rawedidbuf  pointer to Buffer in which bytes are returned
 *
 * Returns:
 *   Parsed_Edid, NULL if get_raw_edid_by_fd() fails
 *
 * Terminates execution if open or close of bus fails
 */
Parsed_Edid * i2c_get_parsed_edid_by_fd(int fd) {
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d\n", fd);
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
      DBGMSF(debug, "i2c_get_raw_edid_by_fd() returned %s", gsc_desc(rc));

   }
   buffer_free(rawedidbuf, NULL);
   DBGMSF(debug, "Returning %p", edid);
   return edid;
}
