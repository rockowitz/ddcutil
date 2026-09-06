/** @file i2c_bus_collections.c
 *
 *  Operations on multiple i2c devices
 */

// Copyright (C) 2018-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/linux_basic_util.h"
#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/traced_function_stack.h"
#ifdef ENABLE_UDEV
#include "util/udev_i2c_util.h"
#endif

#include "base/core.h"
#include "base/i2c_bus_base.h"
#include "base/parms.h"
#include "base/rtti.h"

#include "sysfs/sysfs_base.h"
#include "sysfs/sysfs_i2c_info.h"
#include "sysfs/sysfs_sys_drm_connector.h"
#include "sysfs/sysfs_conflicting_drivers.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_bus_sysfs.h"
#include "i2c/i2c_bus_open_close.h"

#include "i2c/i2c_bus_collections.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

/** If at least this many buses found to check, perform checks asynchronously */
int  i2c_businfo_async_threshold = DEFAULT_BUS_CHECK_ASYNC_THRESHOLD;

bool force_failure_i2c_all_relevant_i2c_buses_rw = false;     // for testing
bool force_failure_i2c_all_edids_readable_using_i2c = false;  // for testing

/** Gets a list of all /dev/i2c devices by checking the file system
 *  if devices named /dev/i2c-N exist.
 *
 *  @param  include_ignorable_devices  if true, include devices that are
 *                                     unusable for DDC communication
 *  @return Byte_Value_Array containing the valid bus numbers
 */
Byte_Value_Array
i2c_get_devices_by_existence_test(bool include_ignorable_devices) {
   Byte_Value_Array bva = bva_create();
   for (int busno=0; busno < I2C_BUS_MAX; busno++) {
      if (i2c_device_exists(busno)) {
         if (include_ignorable_devices || !sysfs_is_ignorable_i2c_device(busno))
            bva_append(bva, busno);
      }
   }
   return bva;
}


/** Checks that all /dev/i2c buses that might be associated with a display
 *  are in group i2c and have the group RW permission bits set.
 *
 *  @return true if all pass the check, false otherwise
 *
 *  The bus numbers are obtained by checking for existence.
 */
bool i2c_all_relevant_buses_group_i2c_rw() {
   bool debug = false;
   Byte_Value_Array bva = i2c_get_devices_by_existence_test(false);
   bool result = true;
   for (int ndx = 0; ndx < bva_length(bva); ndx++) {
      uint8_t busno = bva_get(bva, ndx);
      char buf[20];
      snprintf(buf, 20, "/dev/i2c-%d",busno);
      if (!is_file_group_i2c(buf)) {
         result = false;
         break;
      }
      if (!is_file_group_perm_rw(buf)) {
         result = false;
         break;
      }
   }
   bva_free(bva);
   DBGMSF(debug, "Returning: %s", sbool(result));
   return result;
}


/** Checks that all buses that could possibly be used for DDDC communication
 *  are in group i2c, that the group permission bits are RW, and the current
 *  user is a member of that group.
 *
 *  @return true/false
 *
 *  The bus numbers are obtained by checking for existence.
 */
bool i2c_all_relevant_buses_rw_by_inode()  {
   bool debug = false;
   bool result =  cur_user_in_group_i2c() &&
                  i2c_all_relevant_buses_group_i2c_rw();
   DBGMSF(debug, "Returning %s", sbool(result));
   return result;
}


/** Checks that all /dev/i2c buses that might possibly be used for DDC
 *  communication can be read and written. The check is executed
 *  by attempting an open()/close().
 *
 *  The bus numbers are obtained from udev.
 *
 *  @return Error_Info struct if one or more buses are inaccessible,
 *          NULL if no problem
 */
Error_Info *
i2c_all_relevant_i2c_buses_rw() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_i2c_buses=%p", all_i2c_buses);
   GPtrArray * err_accumulator = NULL;
   Error_Info * final_result = NULL;

   if (force_failure_i2c_all_relevant_i2c_buses_rw) {
      DBGMSG("Forcing dummy failure");
      final_result = ERRINFO_NEW(-EACCES, "Dummy failure");
      goto bye;
   }

#ifdef WRONG  //buses_bitset_from_businfo_array() has already read the buses
   BS256 attached_buses = i2c_buses_bitset_from_businfo_array(all_i2c_buses, /*only_connected*/ false);
   Bit_Set_256_Iterator iter = bs256_iter_new(attached_buses);
   int busno = -1;
   while ( (busno = bs256_iter_next(iter)) >= 0) {
      Error_Info * err = i2c_simple_rw_test(busno);
      if (err) {
         if (!err_accumulator)
            err_accumulator = g_ptr_array_new_with_free_func((void*)errinfo_free);
         g_ptr_array_add(err_accumulator,err);
      }
   }
   bs256_iter_free(iter);
#endif

   Byte_Value_Array bva =
   i2c_get_device_numbers_using_udev(/*include_ignorable_devices*/ false);
   for (int ndx=0; ndx<bva_length(bva); ndx++) {
      int busno = bva_get(bva, ndx);
      Error_Info * err = i2c_simple_rw_test(busno);
      if (err) {
         if (!err_accumulator)
            err_accumulator = g_ptr_array_new_with_free_func((void*)errinfo_free);
         g_ptr_array_add(err_accumulator,err);
      }
   }
   bva_free(bva);

   if (err_accumulator) {
      final_result = errinfo_new_with_causes_gptr(DDCRC_INVALID_OPERATION, err_accumulator, __func__,
            "libddcutil requires RW access to all /dev/i2c devices that might be used for DDC.");
      g_ptr_array_free(err_accumulator, true);
   }

   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Before check all EACCES, final_result=%s", errinfo_summary(final_result));
   if (final_result) {
      bool all_eaccess = true;
      for (int ndx = 0; ndx < final_result->cause_ct; ndx++) {
         if (final_result->causes[ndx]->status_code != -EACCES) {
            all_eaccess = false;
            break;
         }
      }
      if (all_eaccess)
         final_result->status_code = -EACCES;
   }

bye:
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, final_result, "");
   return final_result;
}


#ifdef UNUSED
/** Checks that all EDIDS for Display_Refs of type I2C are actually
 *  readable using I2C. There are some cases, e.g. DisplayLink devices,
 *  where the EDID can be read only from /sys.
 *
 * @return NULL if all readable, struct Error_Info if not
 */
Error_Info *
i2c_all_edids_readable_using_i2c() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   Error_Info * errs = NULL;
   if (force_failure_i2c_all_edids_readable_using_i2c) {
      errs = ERRINFO_NEW(-EACCES, "Dummy failure");
   }
   else {
      errs = i2c_all_relevant_i2c_buses_rw();
      if (errs) {
         syslog(LOG_WARNING, "%s", errs->detail);
         for (int ndx = 0; ndx < errs->cause_ct; ndx++) {
            syslog(LOG_WARNING, "   %s", errs->causes[ndx]->detail);
         }
      }
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, errs, "");
   return errs;
}
#endif


/** Used when i2c_check_bus() is called asynchronously.
 *
 *  @param data pointer to I2C_Bus_Info struct of bus to check.
 */
STATIC void *
i2c_threaded_initial_checks_by_businfo(gpointer data) {
   bool debug = false;

   I2C_Bus_Info * businfo = data;
   TRACED_ASSERT(memcmp(businfo->marker, I2C_BUS_INFO_MARKER, 4) == 0 );
   DBGTRC_STARTING(debug, TRACE_GROUP, "bus = /dev/i2c-%d", businfo->busno );

   Error_Info * err = i2c_check_bus(businfo, EDID_STATUS_UNKNOWN);
   // g_thread_exit(NULL);

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "bus=/dev/i2c-%d", businfo->busno );
   free_current_traced_function_stack();
   return err;
}



/** Spawns threads to perform initial checks and waits for them all to complete.
 *
 *  @param i2c_buses   #GPtrArray of pointers to #I2C_Bus_Info
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
      ERRINFO_FREE_WITH_REPORT(err,IS_DBGTRC(debug, TRACE_GROUP) || is_report_ddc_errors_enabled());
   }
   DBGMSF(debug, "Threads joined");
   g_ptr_array_free(threads, true);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


/** Loops through a list of I2C_Bus_Info, performing initial checks on each.
 *
 *  @param i2c_buses #GPtrArray of pointers to #I2C_Bus_Info
 */
static void
i2c_non_async_scan(GPtrArray * i2c_buses) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "checking %d buses", i2c_buses->len);
   Error_Info * err = NULL;

   for (int ndx = 0; ndx < i2c_buses->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(i2c_buses, ndx);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
            "Calling i2c_check_bus() synchronously for bus %d", businfo->busno);
      err = i2c_check_bus(businfo, EDID_STATUS_UNKNOWN);
      ERRINFO_FREE_WITH_REPORT(err,IS_DBGTRC(debug, TRACE_GROUP) || is_report_ddc_errors_enabled());
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


//
// Attached buses
//

#ifdef ENABLE_UDEV
/** Gets the numbers of I2C devices
 *
 *  \param  include_ignorable_devices  if true, do not exclude SMBus and other ignorable devices
 *  \return sorted #Byte_Value_Array of I2C device numbers, caller is responsible for freeing
 */
Byte_Value_Array
i2c_get_device_numbers_using_udev(bool include_ignorable_devices) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "include_ignorable_devices=%s", SBOOL(include_ignorable_devices));

   Byte_Value_Array bva = bva_create();
   GPtrArray * summaries = get_i2c_devices_using_udev();
   if (summaries) {
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         int busno = udev_i2c_device_summary_busno(summary);
         assert(busno >= 0);
         assert(busno <= 127);
         if ( include_ignorable_devices || !sysfs_is_ignorable_i2c_device(busno) )
            bva_append(bva, busno);
      }
      free_udev_device_summaries(summaries);
   }

   char * s = bva_as_string(bva, /*as_hex*/ false, ",");
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning I2C bus numbers: %s", s);
   free(s);

   return bva;
}
#endif


/** Returns the bus numbers for /dev/i2c buses that could possibly be
 *  connected to a monitor. Buees that the user had indicated should
 *  be ignored (option --ignore-bus) are not included.
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

   // filter out buses that user has indicated should be excluded using
   // option --ignore_bus
   Byte_Value_Array bva = bva_filter(bva0, i2c_bus_is_not_excluded);
   bva_free(bva0);

   char * s = bva_as_string(bva,  false,  ", ");
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "possible i2c device bus numbers: %s", s);
   free(s);
   return bva;
}


/** Returns the bus numbers for /dev/i2c buses that could possibly be
 *  connected to a monitor as a bitset.  Does not include buses
 *  that the user has requested be ignored.
 *
 *  @return bitset of bus numbers
 */
Bit_Set_256 i2c_detect_attached_buses_as_bitset() {
   Byte_Value_Array bva = i2c_detect_attached_buses();
   Bit_Set_256  cur_buses = bs256_from_bva(bva);
   bva_free(bva);
   return cur_buses;
}


/** Filters a set of bus numbers to those whose bus has an EDID.
 *
 *  @param  bs_all_buses  buses to check
 *  @param  eacces_loc    if non-NULL, set to true if any device could not be
 *                        opened due to EACCES (see i2c_edid_exists())
 *  @return buses with an EDID
 */
Bit_Set_256 i2c_filter_buses_w_edid_as_bitset(BS256 bs_all_buses, bool * eacces_loc) {
   BS256 bs_buses_w_edid = EMPTY_BIT_SET_256;
   Bit_Set_256_Iterator iter =  bs256_iter_new(bs_all_buses);
   int bitno = bs256_iter_next(iter);
   while (bitno >= 0) {
      if (i2c_edid_exists(bitno, eacces_loc))
         bs_buses_w_edid = bs256_insert(bs_buses_w_edid, bitno);
      bitno = bs256_iter_next(iter);
   }
   bs256_iter_free(iter);
   return bs_buses_w_edid;
}


/** Returns a bitset indicating the I2C buses with an EDID.
 *
 * @return bitset of buses with an EDID
 */
Bit_Set_256 i2c_buses_w_edid_as_bitset() {
   BS256 bs_all_buses = i2c_detect_attached_buses_as_bitset();
   return i2c_filter_buses_w_edid_as_bitset(bs_all_buses, NULL);
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
   // One snapshot of the DRM connectors for the lookups below.  Why it is
   // taken here and not after the scan, and why nothing needs to maintain it,
   // are in take_connector_snapshot().
   take_connector_snapshot();

   if (buses->len < i2c_businfo_async_threshold) {
      i2c_non_async_scan(buses);
   }
   else {
      i2c_async_scan(buses);
   }

   discard_connector_snapshot();

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


#ifdef MAINTAINED_CONNECTOR_ARRAY
/* Parked.  These maintain a Sys_Drm_Connector array that outlives display
 * detection: update_sys_drm_connector_by_edid() fills in a connector's bus
 * number when the watch attaches a display, extended_bus_detection() does the
 * same in bulk at startup for connectors the driver left unresolved.  Both
 * exist so that a persistent array can answer a lookup by bus number.
 *
 * Superseded by the snapshot, which is discarded after detection and so has
 * nothing to keep current.  Retained for reference: the deduction is the part
 * that would be wanted again if a persistent array ever were.
 */
/** Assigns a bus number to the DRM connector showing the same EDID as a bus,
 *  for use when the display watch has just attached one.
 *
 *  Matches #I2C_Bus_Info.edid against #Sys_Drm_Connector.edid_bytes and, on a
 *  match, sets that connector's i2c_busno.  Connectors that already have a bus
 *  number are left alone: that mapping came from the driver.
 *
 *  @param  businfo  newly attached bus, EDID already read
 *  @return true if a connector was updated
 *
 *  @remark
 *  Tries the cached connector array first and rescans /sys/class/drm only if
 *  that fails.  The rescan matters: nothing in the watch path refreshes the
 *  array, so for a display attached since the last scan the cached connector
 *  has no EDID yet and the first attempt cannot succeed.  Trying cached-first
 *  keeps a batch of several added buses from rescanning once per bus, which is
 *  worth avoiding -- reading connector attributes is itself slow on some
 *  drivers.
 */
bool update_sys_drm_connector_by_edid(I2C_Bus_Info * businfo) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "busno=%d", (businfo) ? businfo->busno : -1);

   bool updated = false;
   if (businfo && businfo->edid) {
      // Held across the rescan and the write: the array must not be rebuilt
      // between get_sys_drm_connectors() handing it over and cur->i2c_busno
      // being set, and no other thread may see a half-updated array.
      g_rec_mutex_lock(&sys_drm_connectors_mutex);
      for (int attempt = 0; attempt < 2 && !updated; attempt++) {
         // attempt 0 uses whatever is cached, attempt 1 rescans
         GPtrArray * connectors = get_sys_drm_connectors(/*rescan=*/ attempt > 0);
         if (!connectors)
            break;
         for (int ndx = 0; ndx < connectors->len; ndx++) {
            Sys_Drm_Connector * cur = g_ptr_array_index(connectors, ndx);
            if (cur->i2c_busno >= 0)
               continue;
            if (!cur->edid_bytes || cur->edid_size < 128)
               continue;
            if (memcmp(cur->edid_bytes, businfo->edid->bytes, 128) == 0) {
               DBGTRC_NOPREFIX(debug, DDCA_TRC_I2C,
                     "Connector %s: setting i2c_busno = %d by EDID match%s",
                     cur->connector_name, businfo->busno,
                     (attempt > 0) ? " (after rescan)" : "");
               cur->i2c_busno = businfo->busno;
               updated = true;
               break;
            }
         }
      }
      g_rec_mutex_unlock(&sys_drm_connectors_mutex);
   }

   DBGTRC_RET_BOOL(debug, DDCA_TRC_I2C, updated, "busno=%d", (businfo) ? businfo->busno : -1);
   return updated;
}


/** Fills in the I2C bus number of DRM connectors that have an EDID but whose
 *  bus number sysfs did not report, by matching their EDID against the buses
 *  in #all_i2c_buses.
 *
 *  Some drivers, nvidia among them, do not publish the connector to bus
 *  mapping, so #Sys_Drm_Connector.i2c_busno stays -1 even for a connector with
 *  a display attached.  The EDID is the only thing the two views share, so it
 *  is what the match is made on.
 *
 *  Only connectors that have an EDID and lack a bus number are considered; a
 *  connector whose bus number sysfs did report is left alone, since that
 *  mapping came from the driver and is better evidence than a guess.
 *
 *  Two sources are consulted, in order of authority.  First the table the user
 *  supplied with --bus-drm-connector: that is an explicit statement about this
 *  machine and beats anything inferred.  Only if the connector is absent from
 *  it are the buses searched for a matching EDID.
 *
 *  @remark
 *  The first bus whose EDID matches wins.  Two connectors showing the same
 *  EDID is possible in principle -- the same monitor reachable by two cables --
 *  and there is nothing here to tell them apart, so the arbitrary choice is
 *  deliberate rather than overlooked.
 */
void extended_bus_detection() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "");

   int         from_table = 0;   // resolved from --bus-drm-connector
   int         from_edid  = 0;   // resolved by matching against all_i2c_buses

   /* Both arrays are read and one is written, so both locks are held for the
    * duration.  Order is all_i2c_buses_mutex then sys_drm_connectors_mutex,
    * and it has to stay that way: nothing under sysfs/ calls the i2c_*bus*
    * functions that take all_i2c_buses_mutex, so no path acquires them in the
    * opposite order today.
    *
    * user_busno_for_drm_connector() reads the --bus-drm-connector table, which
    * add_busno_connector() fills during initialization and nothing writes
    * afterwards, so it needs no lock of its own.
    */
   g_mutex_lock(&all_i2c_buses_mutex);
   g_rec_mutex_lock(&sys_drm_connectors_mutex);
   GPtrArray * connectors = get_sys_drm_connectors(/*rescan=*/ false);

   if (connectors && all_i2c_buses) {
      for (int cndx = 0; cndx < connectors->len; cndx++) {
         Sys_Drm_Connector * cur = g_ptr_array_index(connectors, cndx);
         if (cur->i2c_busno >= 0)
            continue;
         if (!cur->edid_bytes || cur->edid_size < 128)
            continue;

         int user_busno = user_busno_for_drm_connector(cur->connector_name);
         if (user_busno >= 0) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_I2C,
                  "Connector %s: setting i2c_busno = %d from --bus-drm-connector",
                  cur->connector_name, user_busno);
            cur->i2c_busno = user_busno;
            from_table++;
            continue;
         }

         for (int bndx = 0; bndx < all_i2c_buses->len; bndx++) {
            I2C_Bus_Info * businfo = g_ptr_array_index(all_i2c_buses, bndx);
            if (!businfo->edid)
               continue;
            if (memcmp(businfo->edid->bytes, cur->edid_bytes, 128) == 0) {
               DBGTRC_NOPREFIX(debug, DDCA_TRC_I2C,
                     "Connector %s: setting i2c_busno = %d by EDID match",
                     cur->connector_name, businfo->busno);
               cur->i2c_busno = businfo->busno;
               from_edid++;
               break;
            }
         }
      }
   }
   g_rec_mutex_unlock(&sys_drm_connectors_mutex);
   g_mutex_unlock(&all_i2c_buses_mutex);

   DBGTRC_DONE(debug, DDCA_TRC_I2C,
         "Resolved %d connector(s) from --bus-drm-connector, %d by EDID match",
         from_table, from_edid);
}


/** Detect buses if not already detected.
 *
 *  Stores the result in global array all_i2c_buses and also
 *  the bitset connected_buses.
 *
 *  @return number of i2c buses
 */
#endif


int i2c_detect_buses() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_I2C, "all_i2c_buses = %p", all_i2c_buses);

   if (!all_i2c_buses) {
      all_i2c_buses = i2c_detect_buses0();
      // g_ptr_array_set_free_func(all_i2c_buses, (GDestroyNotify) i2c_free_bus_info);

      // The bus view is complete, so this is the first point at which a
      // connector left without an i2c_busno by the driver can be resolved
      // against it.  Only inside this branch: on a driver that publishes the
      // mapping there is nothing to resolve, and repeating the search on the
      // calls that find all_i2c_buses already built would find nothing either.
      //
#ifdef MAINTAINED_CONNECTOR_ARRAY
      // Belongs to the persistent array: the deduced bus numbers exist so that
      // its lookup can answer by bus number.  The snapshot has no use for them,
      // populating the businfo records during detection and then discarding the
      // array, so there is nothing left for a deduced number to serve.
      extended_bus_detection();
#endif
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
         // g_ptr_array_set_free_func(all_i2c_buses, (GDestroyNotify) i2c_free_bus_info);
      }
      businfo = i2c_new_bus_info(busno);
      businfo->flags = I2C_BUS_EXISTS;
      Error_Info * err = i2c_check_bus(businfo, EDID_STATUS_UNKNOWN);
      ERRINFO_FREE_WITH_REPORT(err, IS_DBGTRC(debug,DDCA_TRC_I2C) || is_report_ddc_errors_enabled());
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
 *  @param  businfo_array   array of I2C_Bus_Info
 *  @param  only_connected if true, only include buses having EDID
 *  @return bit set
 */
Bit_Set_256 i2c_buses_bitset_from_businfo_array(GPtrArray * businfo_array, bool only_connected) {
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


#ifdef UNUSED
Bit_Set_256 i2c_nonlaptop_buses_bitset_from_businfo_array(
                GPtrArray * businfo_array,
                bool        only_connected)
{
   bool debug = false;
   assert(businfo_array);
   DBGTRC_STARTING(debug, TRACE_GROUP, "businfo_array=%p, len=%d, only_connected=%s",
         businfo_array, businfo_array->len, SBOOL(only_connected));

   Bit_Set_256 result = EMPTY_BIT_SET_256;
   for (int ndx = 0; ndx < businfo_array->len; ndx++) {
      I2C_Bus_Info * businfo = g_ptr_array_index(businfo_array, ndx);
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "businfo=%p", businfo);
      if ( (!only_connected || businfo->edid) && !(businfo->flags&I2C_BUS_LAPTOP)  ) {
         // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "EDID exists");
         result = bs256_insert(result, businfo->busno);
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s", bs256_to_string_decimal_t(result, "", ", "));
   return result;
}
#endif


void init_i2c_bus_collections(void) {
   RTTI_ADD_FUNC(i2c_all_relevant_i2c_buses_rw);
   RTTI_ADD_FUNC(i2c_threaded_initial_checks_by_businfo);
   RTTI_ADD_FUNC(i2c_async_scan);
   RTTI_ADD_FUNC(i2c_non_async_scan);
#ifdef ENABLE_UDEV
   RTTI_ADD_FUNC(i2c_get_device_numbers_using_udev);
#endif
   RTTI_ADD_FUNC(i2c_detect_attached_buses);
   RTTI_ADD_FUNC(i2c_detect_buses0);
#ifdef MAINTAINED_CONNECTOR_ARRAY
   RTTI_ADD_FUNC(extended_bus_detection);
   RTTI_ADD_FUNC(update_sys_drm_connector_by_edid);
#endif
   RTTI_ADD_FUNC(i2c_detect_buses);
   RTTI_ADD_FUNC(i2c_detect_single_bus);
   RTTI_ADD_FUNC(i2c_buses_bitset_from_businfo_array);
}
