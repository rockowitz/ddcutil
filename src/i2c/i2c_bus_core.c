/** @file i2c_bus_core.c
 *
 * I2C bus detection and inspection
 */
// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/** \endcond */

#include "util/acl_util.h"
#include "util/dbus_util.h"
#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/edid.h"
#include "util/error_info.h"
#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_util.h"
#include "util/traced_function_stack.h"

#include "base/core.h"
#include "base/display_lock.h"
#include "base/execution_stats.h"
#include "base/flock.h"
#include "base/i2c_bus_base.h"
#include "base/linux_errno.h"
#include "base/monitor_model_key.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#include "sysfs/sysfs_base.h"
#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_i2c_info.h"
#include "sysfs/sysfs_sys_drm_connector.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif

#include "i2c/i2c_bus_sysfs.h"
#include "i2c/i2c_edid.h"
#include "i2c/i2c_strategy_dispatcher.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_bus_open_close.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

// Globals
bool try_get_edid_from_sysfs_first = true;    // enable-try-get-edid-from-sysfs, disable-try-get-edid-from-sysfs

int  pause_after_resume_ms = DEFAULT_PAUSE_AFTER_RESUME_MS;   // --pause-after-resume_ms 500
bool primitive_sysfs = false;                 // logic and --f23

// If true, i2c_edid_exists() does not open the device when the DRM connector
// for the bus reports status "disconnected".
bool edid_exists_checks_drm_status = true;    // --f35

// If true, i2c_edid_exists() does not open the device when no DRM connector
// names the bus, on a machine whose driver publishes that mapping.  --f38
bool edid_exists_skips_unmapped_bus = true;   // --f38


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
bool
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d - %s, driver=%s", fd, filename_for_fd_t(fd), driver);

   // Quirks
   // - i2c_set_addr() Causes screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   //   avoided by never calling this function for an eDP screen
   // - Dell P2715Q does not respond to single byte read, but does respond to
   //   a write (7/2018), so this function checks both
   Status_Errno_DDC rc = -1;
   int max_tries =  DETECT_X37_MAX_TRIES;  // 3;
   int poll_wait_millisec = DETECT_X37_NORMAL_RETRY_MS;
   if (streq(driver, "nvidia"))
      poll_wait_millisec = DETECT_X37_NVIDIA_RETRY_MS;
   int loopctr;
   for (loopctr = 0; loopctr < max_tries && rc != 0; loopctr++) {  // retries seem to give no benefit
      if (loopctr > 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "driver=%s, sleeping for %d millisec",
                                driver, poll_wait_millisec);
         SLEEP_MILLIS_WITH_SYSLOG(poll_wait_millisec, "Extra x37 sleep");
      }

      // regard either a successful write() or a read() as indication slave address is valid
      Byte writebuf = 0x00;
      rc = invoke_i2c_writer(fd, 0x37, 1, &writebuf);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_writer() for slave address x37 returned %s", psc_name_code(rc));
      if (rc != 0) {
         Byte    readbuf[4];  //  4 byte buffer
         rc = invoke_i2c_reader(fd, 0x37, false, 4, readbuf);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_reader() for slave address x37 returned %s", psc_name_code(rc));
      }

      if (rc == -EBUSY) {
         DUAL_MSGXV(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP, "X37 detection encountered EBUSY error");
         max_tries = DETECT_X37_MAX_TRIES + 2;
      }

   }

   if (rc == 0 && loopctr > 1) {
      DUAL_MSGXV(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP, "X37 detection succeeded on try %d", loopctr);
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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr(dh));
   Error_Info * err = NULL;
   if (dh->dref->disconnected) {
      err = ERRINFO_NEW(DDCRC_DISCONNECTED, "dh->dref-disconnected == true");
      goto bye;
   }

   I2C_Bus_Info * businfo = dh->dref->detail;
   if (!businfo || memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) != 0) {
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "Invalid businfo %p for %s", businfo, dh_repr(dh));
      err = ERRINFO_NEW(DDCRC_INTERNAL_ERROR, "Invalid businfo %p for %s", businfo, dh_repr(dh));
      goto bye;
   }
   if (!(businfo->flags & I2C_BUS_EXISTS) || !(businfo->flags & I2C_BUS_PROBED)) {
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "businfo for %s missing flag %s%s", dh_repr(dh),
            (businfo->flags & I2C_BUS_EXISTS) ? "" : "I2C_BUS_EXISTS ",
            (businfo->flags & I2C_BUS_PROBED) ? "" : "I2C_BUS_PROBED");
      err = ERRINFO_NEW(DDCRC_INTERNAL_ERROR, "businfo for %s missing flag %s%s", dh_repr(dh),
            (businfo->flags & I2C_BUS_EXISTS) ? "" : "I2C_BUS_EXISTS ",
            (businfo->flags & I2C_BUS_PROBED) ? "" : "I2C_BUS_PROBED");
      goto bye;
   }

#ifdef REDUNDANT
   if (current_traced_function_stack_size() > 0) {
      if (IS_DBGTRC(debug, TRACE_GROUP)) {
         DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "Traced function stack on entry to i2c_check_open_bus_alive","");
         // show_backtrace(0);   // all blank lines
         dbgrpt_current_traced_function_stack(false, true, 0);
      }
      syslog(LOG_DEBUG, "Traced function stack on entry to i2c_check_open_bus_alive()");
      TRACED_FUNCTION_STACK_TO_SYSLOG(DDCA_SYSLOG_DEBUG, TFS_MOST_RECENT_FIRST);
   }
#endif

   bool edid_exists = false;
   int tryctr = 0;
   for (; !edid_exists && tryctr < CHECK_OPEN_BUS_ALIVE_MAX_TRIES; tryctr++) {
      if (tryctr > 0)
         SLEEP_MILLIS_WITH_SYSLOG2(DDCA_SYSLOG_WARNING, CHECK_OPEN_BUS_ALIVE_RETRY_MILLISEC,
                          "Retrying i2c_check_edid_exists_by_dh() tryctr=%d, dh=%s",
                          tryctr, dh_repr(dh));

#ifdef SYSFS_PROBLEMATIC   // apparently not by driver vfd on Raspberry pi
      if (businfo->drm_connector_name) {
         edid_exists = GET_ATTR_EDID(NULL, "/sys/class/drm/", businfo->drm_connector_name, "edid");
         // edid_exists = i2c_check_bus_responsive_using_drm(businfo->drm_connector_name);  // fails for Nvidia
      }
      else {
         // read edid
         edid_exists = i2c_check_edid_exists_by_dh(dh);
      }
#else
      edid_exists = i2c_check_edid_exists_by_dh(dh);
#endif
   }

   if (!edid_exists) {
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "/dev/i2c-%d, Checking EDID failed after %d tries (B)",
            businfo->busno, tryctr);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "/dev/i2c-%d: Checking EDID failed (A)", businfo->busno);
      err = ERRINFO_NEW(DDCRC_DISCONNECTED, "Unable to read EDID for /dev/i2c-%d", businfo->busno);
      businfo->flags &= ~(I2C_BUS_HAS_EDID|I2C_BUS_ADDR_X37);   // ???
   }
   else {
      if (tryctr > 1) {
         DECORATED_SYSLOG(DDCA_SYSLOG_WARNING,
               "/dev/i2c-%d: Checking EDID succeeded after %d tries (G)",
               businfo->busno, tryctr);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
               "/dev/i2c-%d: Checking EDID succeeded after %d tries (H)",
               businfo->busno,tryctr);
      }
      char * driver = businfo->driver;
      int ddcrc = i2c_detect_x37(dh->fd, driver);
      if (ddcrc){
         // would DDCRC_DDC_DISABLED, DDCRC_DEAD, DDCRC_UNAVAILBLE be better?
         err = ERRINFO_NEW(DDCRC_DISCONNECTED,
               "/dev/i2c-%d: Slave address x37 unresponsive. io status = %s",
               businfo->busno, psc_desc(ddcrc));
         businfo->flags &= ~I2C_BUS_ADDR_X37;   // ???
      }
   }
   if (!err) {
      if (dpms_check_drm_asleep_by_businfo(businfo))
         err = ERRINFO_NEW(DDCRC_DPMS_ASLEEP,
               "/dev/i2c-%d", dh->dref->io_path.path.i2c_busno);
   }

bye:
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
   DBGTRC_RET_STRING(debug, TRACE_GROUP, bs256_to_string_t(result, "", ", "), "");
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
  *  @param  eacces_loc  if non-NULL, is set to true if the device could not be
  *                      opened due to EACCES; never set to false, so a caller
  *                      can accumulate over multiple buses
  *  @return true/false
  *
  *  @remark
  *  An EACCES open failure (e.g. the window after resume from sleep before
  *  udev has reapplied device ACLs) also returns false; check *eacces_loc to
  *  distinguish that case from a disconnected monitor.
  */
 bool i2c_edid_exists(int busno, bool * eacces_loc) {
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
    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "sysfs_name=|%s|, dev_name=|%s|", sysfs_name, dev_name);
    bool edid_exists = false;
    char * drm_connector_name = NULL;

    Error_Info *master_err = NULL;
    if (!i2c_device_exists(busno)) {
       goto bye;
    }

#ifdef OUT
    Error_Info * err = i2c_check_device_access(dev_name);
    if (err != NULL) {
       errinfo_free(err);   // for now
       goto bye;
    }
#endif

    if ( sysfs_is_ignorable_i2c_device(busno) ) {
       goto bye;
    }

    bool is_displaylink = is_displaylink_device(busno);

    bool drm_card_connector_directories_exist = directory_exists("/sys/class/drm");
    // *** Try to find the drm connector by bus number

    if (drm_card_connector_directories_exist) {
       // n. will fail for MST
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "Finding DRM connector name for bus %s using busno", dev_name);
       Found_Sys_Drm_Connector res = find_sys_drm_connector_by_busno_or_edid(busno, NULL);
       if (res.connector_name) {
          drm_connector_name = strdup(res.connector_name);
          free_found_sys_drm_connector_result_contents(res);
       }
       else {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "DRM connector not found by busno %d", busno);

          // No connector claims this bus.  On a machine whose driver publishes
          // the bus/connector mapping at all, that is positive evidence the
          // bus serves no connector and so can have no display -- the i915
          // gmbus channels for ports nothing is wired to, which otherwise cost
          // an I2C read apiece that takes tens of milliseconds to return
          // ENXIO, on every scan.
          //
          // Three conditions, each load bearing:
          //  - the driver is in known_reliable_driver(), so a connector's
          //    absence reflects hardware rather than a driver that does not
          //    maintain this part of sysfs;
          //  - some connector does report a bus number, proving this driver on
          //    this machine attaches DDC adapters.  Without this the rule would
          //    skip every bus under a driver that never publishes the mapping,
          //    nvidia being the standing example;
          //  - the bus is not DPMST.  A display behind an MST hub has no
          //    connector of its own, so its absence here says nothing.  That
          //    was issue #585.
          // DisplayLink is excluded for the same reason it is elsewhere: its
          // EDID is only ever readable from sysfs.
          if (edid_exists_skips_unmapped_bus && !is_displaylink) {
             char * busname = get_i2c_device_sysfs_name(busno);
             bool is_mst = streq(busname, "DPMST");     // streq() handles NULL
             free(busname);
             if (!is_mst &&
                 is_sysfs_reliable_for_busno(busno) &&
                 any_drm_connector_has_busno())
             {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                      "No DRM connector serves bus %d and this driver publishes the "
                      "mapping, not opening device", busno);
                goto bye;
             }
          }
       }

       // *** Possibly try to get the EDID from sysfs
       bool checked_connector_for_edid = false;
       if (drm_connector_name)  {   // i.e. DRM_CONNECTOR_FOUND_BY_BUSNO
          if ( (try_get_edid_from_sysfs_first &&
                is_sysfs_reliable_for_busno(busno) &&
                !primitive_sysfs
               ) ||
                is_displaylink)   // X50 can't be read for DisplayLink, must use sysfs
          {
             checked_connector_for_edid = true;
             Byte * edidbytes = get_connector_edid(drm_connector_name);
             if (edidbytes) {
                edid_exists = true;
                free(edidbytes);
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                      "Retrieved edid using DRM connector %s", drm_connector_name);
             }
             else {
                DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                      "Failed to get edid using DRM connector %s", drm_connector_name);
             }
          }
       }
       if (checked_connector_for_edid)
          goto bye;
    }

    // *** Possibly check the DRM connector status before resorting to opening the device

    // If the connector reports "disconnected" there is no monitor, hence no
    // EDID to read: skip opening the device, which is comparatively expensive
    // and, in the post-resume EACCES window, can block for seconds in open
    // retries.  Status "unknown", an unreadable attribute, and no connector
    // found for the bus all fall through to the device open.
    //
    // Which buses reach here.  Two conditions must both hold: the connector
    // was found by bus number just above, and the sysfs EDID shortcut was not
    // taken.  That excludes more than it may appear:
    //  - nvidia never arrives here.  The lookup above passes a NULL EDID, so
    //    it resolves by bus number alone, which is expected to fail for nvidia;
    //    the connector is instead matched by EDID later, on another path.
    //    drm_connector_name is therefore NULL and this block is skipped.  Do
    //    not read this code as covering the case where the sysfs EDID is
    //    untrusted because the driver is nvidia -- it does not.
    //  - The drivers in known_reliable_driver() normally take the shortcut and
    //    goto bye before reaching here, since try_get_edid_from_sysfs_first
    //    defaults true.  They arrive only when that is turned off, or under
    //    primitive_sysfs.
    // What is left is mainly a driver that populates the bus number linkage
    // but is absent from known_reliable_driver(), which is an allowlist: its
    // absence means unknown, not known bad.  Trusting status there is an
    // optimistic assumption.  It is a reasonable one -- populating that
    // linkage suggests an ordinary in-tree driver, whose hotplug path does
    // refresh connector->status -- but it is an assumption, not a verified
    // property, and it is the one worth revisiting if a display goes missing.
    //
    // Note that status_show() in the kernel returns the cached
    // connector->status and never calls the driver's detect hook, so this
    // reads whatever the last probe left behind.  The value is only as fresh
    // as that probe.
    if (edid_exists_checks_drm_status && drm_connector_name) {
       char * status = NULL;
       GET_ATTR_TEXT(&status, "/sys/class/drm", drm_connector_name, "status");
       bool disconnected = status && streq(status, "disconnected");
       free(status);
       if (disconnected) {
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                "DRM connector %s reports disconnected, not opening device", drm_connector_name);
          goto bye;
       }
    }

    // *** Open bus

    DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Calling i2c_open_bus for /dev/i2c-%d..", busno);
    int fd = -1;
    master_err = i2c_open_bus(busno, CALLOPT_WAIT, &fd);
 #ifdef ALT_LOCK_REC
    master_err = i2c_open_bus(businfo->busno, businfo->CALLOPT_WAIT, &fd);
 #endif
    if (master_err) {
       // Report EACCES to the caller: a transient permission failure must be
       // distinguishable from a disconnected monitor.
       if (eacces_loc) {
          if (master_err->status_code == -EACCES)
             *eacces_loc = true;
          else {
             for (int ndx = 0; ndx < master_err->cause_ct; ndx++) {
                if (master_err->causes[ndx]->status_code == -EACCES) {
                   *eacces_loc = true;
                   break;
                }
             }
          }
       }
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
    ERRINFO_FREE_WITH_REPORT(master_err,
                             IS_DBGTRC(debug, TRACE_GROUP) || is_report_ddc_errors_enabled() );
    DBGTRC_RET_BOOL(debug, TRACE_GROUP, edid_exists, "");
    return edid_exists;
 }


/** Sets the card-connector related fields in a #I2C_Bus_Info instance,
 *  by searching for the bus number in the user-supplied table
 *
 *  @param businfo pointer to I2C_Bus_Info instance
 *  @return true if found, false if not
 *
 *  @remark
 *  The connector name was validated when the table was built, i.e. when
 *  option --bus-drm-connector was processed.
 */
static bool set_connector_for_businfo_using_user_bus_connector_table(
      I2C_Bus_Info * businfo)
{
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
          "Finding DRM connector name for bus i2c-%d using busno_connector_table",
          businfo->busno);

   bool result = false;
   FREE(businfo->drm_connector_name);   // may hold a name from an earlier pass
   const char * connector_name = user_drm_connector_for_busno(businfo->busno);
   if (connector_name) {
      businfo->drm_connector_name = strdup(connector_name);
      businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_USER;
      int connector_id = 0;   // attribute is not set by all drivers
      if (GET_ATTR_INT(&connector_id, "/sys/class/drm",
                       businfo->drm_connector_name, "connector_id"))
         businfo->drm_connector_id = connector_id;
      result = true;
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Found connector name for /dev/i2c-%d using busno_connector_table: %s",
             businfo->busno, businfo->drm_connector_name);
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, result, "drm_connector_name=%s",
         businfo->drm_connector_name ? businfo->drm_connector_name : "NULL");
   return result;
}


 /** Sets the card-connector related fields in a #I2C_Bus_Info instance,
  *  by searching for the EDID value in the DRM card-connector directories
  *
  *  @param businfo pointer to I2C_Bus_Info instance
  *
  *  @remark
  *  Note that this function presumes that the video driver for the I2C bus
  *  supports DRM. This is not the case for older drivers, particularly Nvidia.
  *  @remark
  *  Writes to the system log and (possibly) to the terminal if the instance is
  *  not found.
  */
static void set_connector_for_businfo_using_edid(I2C_Bus_Info * businfo) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE,
          "Finding DRM connector name for bus i2c-%d using EDID, connector_directories_exist=%s",
          businfo->busno, SBOOL(sysfs_connector_directories_exist()));
   assert(businfo->edid);

   FREE(businfo->drm_connector_name);   // may hold a name from an earlier pass
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
      if (sysfs_connector_directories_exist()) {
         MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s", msg);
         // LOGABLE_MSG(DDCA_SYSLOG_ERROR,"%s", msg);
      }
      else {
         DECORATED_SYSLOG(DDCA_SYSLOG_INFO, "%s", msg);
         DECORATED_SYSLOG(DDCA_SYSLOG_INFO, "drm connector directories do not exist");
      }
      free(msg);
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE,"");
}


bool edp_always_laptop = true;   // edp-ambiguous

static bool is_laptop_for_businfo(I2C_Bus_Info * businfo) {
   bool debug  = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo=%p, busno=%d, edp_always_laptop=%s",
         businfo, businfo->busno, SBOOL(edp_always_laptop));

   bool is_laptop = false;
   if (businfo->drm_connector_name) {
      if ( is_laptop_drm_connector_name(businfo->drm_connector_name) ) {
         bool b = true;
         if (!edp_always_laptop) {
            // double check, eDP has been seen to be applied to external display, see:
            //   ddcutil issue #384
            //   freedesktop.org issue #10389, DRM connector for external monitor has name card1-eDP-1
            b = is_laptop_parsed_edid(businfo->edid);
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                   "connector name = %s, is_laptop_parsed_edid() returned %s",
                   businfo->drm_connector_name, SBOOL(b));
         }
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


static bool check_x37_for_businfo(int fd, I2C_Bus_Info * businfo) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "fd=%d, businfo=%p, use_x37_detection_table=%s",
         fd, businfo, SBOOL(use_x37_detection_table));

   bool first_x37_check = true;
   X37_Detection_State x37_detection_state = X37_Not_Recorded;
   if (use_x37_detection_table) {
      x37_detection_state = i2c_query_x37_detected(businfo->busno, businfo->edid->bytes);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Restored(1) %s", x37_detection_state_name(x37_detection_state));
      if (x37_detection_state == X37_Detected) {
         businfo->flags |= I2C_BUS_ADDR_X37;
         first_x37_check=false;
      }
   }
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "first_x37_check = %s", SBOOL(first_x37_check));
   if (x37_detection_state != X37_Detected) {
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "Calling i2c_detect_x37() for /dev/i2c-%d...", businfo->busno);
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
          DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                "Recording %s", x37_detection_state_name(x37_detection_state));
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
 *  @return NULL if success, Error_Info struct if error
 *  #retval Error_Info(-ENOENT) if but does not exist
 */
Error_Info * i2c_check_bus(I2C_Bus_Info * businfo, I2C_Check_Bus_Mode check_mode) {
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

#ifdef OUT
   master_err = i2c_check_device_access(dev_name);
   if (master_err != NULL) {
      // if (err->status_code != -ENOENT)
      businfo->open_errno = master_err->status_code;
      // errinfo_free(err);   // for now
      goto bye;
   }
#endif

   if (!primitive_sysfs) {
      if (!businfo->driver) {
         Sysfs_I2C_Info * driver_info = get_i2c_driver_info(businfo->busno, -1);
         businfo->driver = g_strdup(driver_info->driver);  // ** LEAKY
         // perhaps save businfo->driver_version
         // assert(driver_info->adapter_class);
         if (driver_info->adapter_class && 
             !is_adapter_class_display_controller(driver_info->adapter_class) ) 
         {
               master_err = ERRINFO_NEW(DDCRC_OTHER, "Display controller for bus %d has class %s",
                   businfo->busno, driver_info->adapter_class);
         }
         free_sysfs_i2c_info(driver_info);
         if (master_err) {
            goto bye;
         }
      }
   }

   businfo->flags |= I2C_BUS_EXISTS;
   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
         "initial flags = %s", i2c_interpret_bus_flags_t(businfo->flags));

   if (is_displaylink_device(businfo->busno))
      businfo->flags |= I2C_BUS_DISPLAYLINK;

   if (is_sysfs_reliable_for_busno(businfo->busno))
      businfo->flags |= I2C_BUS_SYSFS_KNOWN_RELIABLE;

   // *** Try to find the drm connector, first from the user supplied table,
   // *** then by bus number

   // i2c_reset_bus_info() frees and nulls drm_connector_name, so a recheck
   // arrives here with it unset and the connector is determined afresh.  A
   // display can move between connectors while its bus stays put, so the name
   // found on an earlier pass is not to be trusted.  The test still skips the
   // work when a caller has already established the name on this businfo.
   if (!businfo->drm_connector_name) {
      //assert(businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_CHECKED ||
      //       businfo->drm_connector_found_by == DRM_CONNECTOR_NOT_FOUND);
      businfo->drm_connector_found_by = DRM_CONNECTOR_NOT_CHECKED;
#ifdef NOT_HERE
      // A user supplied bus/connector association is an override, not a fallback.
      // It must be checked before the busno and EDID based searches, which can
      // silently choose the wrong connector when EDIDs are not unique.
      set_connector_for_businfo_using_user_bus_connector_table(businfo);
#endif

      if (drm_card_connector_directories_exist) {
         // n. will fail for MST
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
               "Finding DRM connector name for bus %s using busno", dev_name);
         Found_Sys_Drm_Connector res = find_sys_drm_connector_by_busno_or_edid(businfo->busno,NULL);
         if (res.connector_name) {
            businfo->drm_connector_name = strdup(res.connector_name);  // *** LEAKS ***
            businfo->drm_connector_found_by = DRM_CONNECTOR_FOUND_BY_BUSNO;
            businfo->drm_connector_id = res.connector_id;
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Found DRM connector name %s by busno, found_by=%s",
                  businfo->drm_connector_name,
                  drm_connector_found_by_name(businfo->drm_connector_found_by));
            free_found_sys_drm_connector_result_contents(res);
         }
         else {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "DRM connector not found by busno %d", businfo->busno);
         }
         
         if (!businfo->drm_connector_name) {
            set_connector_for_businfo_using_user_bus_connector_table(businfo);
         }   
      }
   }

   // *** Possibly try to get the EDID from sysfs
   bool checked_connector_for_edid = false;
   if (businfo->drm_connector_name)  {   // i.e. DRM_CONNECTOR_FOUND_BY_BUSNO or _BY_USER
      // The sysfs shortcut is taken only if the connector was found by busno.
      // If the association was supplied by the user, it is because sysfs does not
      // properly record it for this bus, so read the EDID from the bus itself.
      if ((try_get_edid_from_sysfs_first &&
            businfo->flags&I2C_BUS_SYSFS_KNOWN_RELIABLE &&
            businfo->drm_connector_found_by == DRM_CONNECTOR_FOUND_BY_BUSNO)  ||
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
   // based on a busno match or user busno-connector match, try EDID match

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
            int rc = i2c_detect_x37(fd, businfo->driver);
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


// Called by dw_hotplug_change_handler()
I2C_Bus_Info * i2c_get_and_check_bus_info(int busno, I2C_Check_Bus_Mode check_bus_mode) {
   bool debug  = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);

   bool new_info = false;
   I2C_Bus_Info* businfo =  i2c_get_bus_info(busno, &new_info);
   if (!new_info)
      i2c_reset_bus_info(businfo);
   Error_Info * err = i2c_check_bus(businfo, check_bus_mode);
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


//
// Initialization
//

static void init_i2c_bus_core_func_name_table() {
   RTTI_ADD_FUNC(i2c_check_edid_exists_by_dh);
   RTTI_ADD_FUNC(i2c_detect_x37);
   RTTI_ADD_FUNC(i2c_check_open_bus_alive);
   RTTI_ADD_FUNC(i2c_edid_exists);
   RTTI_ADD_FUNC(set_connector_for_businfo_using_user_bus_connector_table);
   RTTI_ADD_FUNC(set_connector_for_businfo_using_edid);
   RTTI_ADD_FUNC(is_laptop_for_businfo);
   RTTI_ADD_FUNC(check_x37_for_businfo);
   RTTI_ADD_FUNC(i2c_check_bus);
   RTTI_ADD_FUNC(i2c_get_and_check_bus_info);
   RTTI_ADD_FUNC(i2c_report_active_bus);
}


// void subinit_i2c_bus_core() {
//    // init_sysfs_drm_connector_names();
// }


void init_i2c_bus_core() {
   init_i2c_bus_core_func_name_table();
}

