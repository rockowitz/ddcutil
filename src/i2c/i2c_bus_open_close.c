/** @file i2c_bus_open_close.c
 *
 * Opening and closing /dev/i2c devices
 *
 *  Functions related to getting a file descriptor for a bus and giving it back,
 *  including the EACCES retry episode that a resume from sleep can provoke
 *  while udev has yet to reapply the device ACLs.
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
#include "sysfs/sysfs_sys_drm_connector.h"

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif

#include "i2c/i2c_bus_sysfs.h"
#include "i2c/i2c_edid.h"
#include "i2c/i2c_strategy_dispatcher.h"

#include "i2c/i2c_bus_open_close.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

// Globals, consumed only by the open/close path.  Set from options in
// ddc_common_init.c; declared in i2c_bus_open_close.h with the rest of this
// subsystem's externs.
bool force_failure_i2c_open = false;                     // --f17
int  max_eacces_retry_ms = DEFAULT_MAX_EACCES_RETRY_MS;  // --i11
int  max_eacces_retry_ct = DEFAULT_MAX_EACCES_RETRY_CT;  // --i12

// The expensive EACCES diagnostics in i2c_open_bus_basic() -- traced function
// stack dump, open failure diagnosis -- are emitted at most once per interval
// instead of once per open call.  --i13
int rate_limit_eacces_diagnostics_interval_sec = DEFAULT_EACCES_DIAGNOSTIC_INTERVAL_SEC;


#ifdef OUT
// Timestamp of the first EACCES open failure in the current cycle.
// Used to share the post-resume sleep across all devices: the first
// failing device sleeps once; subsequent devices in the same window
// retry immediately without adding another sleep.
static _Atomic uint64_t first_eacces_open_ns = 0;
#endif


/** Tests that a /dev/i2c bus can be opened for reading and writing.
 *
 *  @param  busno   i2c bus number
 *  @return NULL if success, Error_Info struct if failure
 */
Error_Info *
i2c_simple_rw_test(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);

   int fd;
   Error_Info * err = i2c_open_bus_basic_by_busno(busno, CALLOPT_NONE, &fd);
   if (!err) {
      i2c_close_bus_basic(busno, fd, CALLOPT_NONE);
   }
   else {
      // MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Error opening /dev/i2c-%d: %s", busno, errinfo_summary(err));
   }

   DBGTRC_RET_ERRINFO(debug, DDCA_TRC_NONE, err, "busno=%d", busno);
   return err;
}


//
// Bus open and close
//

static GMutex  open_failures_mutex;
static Bit_Set_256 open_failures_reported;


#ifdef DETERMINED_UNUSED
/** Adds a set of bus numbers to the set of bus numbers
 *  whose open failure has already been reported.
 *
 *  @param failures   set of bus numbers
 */
void
i2c_add_open_failures_reported(Bit_Set_256 failures) {
   g_mutex_lock(&open_failures_mutex);
   open_failures_reported = bs256_or(open_failures_reported, failures);
   g_mutex_unlock(&open_failures_mutex);
}
#endif


/** Adds a single bus number to the set of open failures already reported.
 *
 *  @param  busno     /dev/i2c-N bus number
 */
void
i2c_include_open_failures_reported(int busno) {
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


#ifdef UNUSED
static bool cur_user_has_group_i2c_perms(const char * filename) {
   bool has_group_perms = false;
   if (!group_i2c_exists()) {
      BASIC_STD_SYSLOG(LOG_WARNING, "Group i2c does not exist");
   }
   else {
      if (!is_file_group_i2c(filename)) {
         SIMPLE_STD_SYSLOG(LOG_WARNING, "Device %s not in group i2c", filename);
      }
      else {
         if (!cur_user_in_group_i2c()) {
            BASIC_STD_SYSLOG(LOG_WARNING, "Current user not in group i2c");
         }
         else {
            if (!is_file_group_acl_rw(filename)) {
               SIMPLE_STD_SYSLOG(LOG_WARNING, "Group permissions on %s not RW",filename);
            }
            else {
               has_group_perms = true;
            }
         }
      }
   }
   return has_group_perms;
}
#endif


// Process-wide EACCES episode state, shared across all opens.  The full
// retry ladder is charged once per episode, not once per open: the first
// open to fail with EACCES starts an episode and retries for up to
// max_eacces_retry_ms; an open failing later in the episode retries only
// for the remainder of that budget, which is soon nothing.  Without this,
// each of the N inaccessible buses of a scan paid the full ladder, turning
// a fast failure for a user with no permissions at all into N x 3 seconds.
// A failure occurring after EACCES_NEW_EPISODE_QUIET_MS with no EACCES
// failure in between starts a new episode with a fresh budget, so the
// transient window after the next resume is again waited out in full.  An
// open that succeeds after retrying ends the episode: the condition was
// transient and has cleared.  Plain atomics, no mutex.  The reset is two
// separate stores, so a concurrent failure can see the episode start already
// cleared while last seen is not; that is why a zero episode start begins a
// new episode below rather than being differenced, which would deny the open
// its retries entirely.  The remaining interleavings cost at worst one extra
// full ladder.
//
// Known limitation.  The budget is shared by every bus, so whichever bus
// fails first spends it.  If a permanently inaccessible bus and a
// transiently inaccessible one fail within the same episode, the permanent
// one can consume the whole window and the transient one, scanned later,
// gets no retry at all and its display is missed for that scan.  Not
// reachable in the ordinary case: SMBus and other non-display nodes are
// excluded by sysfs_is_ignorable_i2c_device() before any open is attempted,
// and the shipped udev rule grants uaccess to exactly the class 0x03 buses
// scanned here, so those buses share one permission fate.  It needs an
// unusual layout, a class 0x0a docking station adapter or multi-seat ACL
// ownership, and the watch path's EACCES rescan plus the episode reset on a
// retried success recover most of what is lost.  Were it to need fixing, the
// cheap form is to record which busno opened the episode and let a bus that
// has not itself failed during it take at least one retry.

static _Atomic uint64_t eacces_episode_start_ns = 0;   // 0 = no episode
static _Atomic uint64_t eacces_last_seen_ns     = 0;   // last EACCES failure

/** Opens a I2C device specified by its file name, without further checks
 *
 *  @param  filename   name of file to open
 *  @param  callopts   if bit CALLOPT_RDONLY set, open RO, otherwise open RW
 *  @param  fd_loc     address which to return file descriptor, -1 if failure
 *  @return Error_Info struct if error, NULL if success
 *
 *  @remark
 *  Common error codes: -ENOENT, -EACCES
 */
Error_Info *
i2c_open_bus_basic(const char * filename,  Byte callopts, int* fd_loc) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "filename=%s, callopts=0x%02x, fd_loc=%p, force_i2c_open_failure=%s",
         filename, callopts, fd_loc, sbool(force_failure_i2c_open));

   // Previously, a pause was taken before the first open. The transient
   // condition this function contends with, udev not yet having reapplied the
   // /dev/i2c uaccess ACL after a resume from sleep or at login, announces
   // itself as EACCES, and is waited out by the retry loop below.
   // Pausing beforehand instead charged every open for a condition most of
   // them do not encounter, and does not shorten the wait for the ones that do.

   Error_Info * err = NULL;
   int eacces_retry_ct = 0;
   int total_eacces_retry_ms = 0;
   int eacces_retry_interval_ms = EACCES_RETRY_INITIAL_INTERVAL_MS;
   int eacces_budget_ms = max_eacces_retry_ms;   // reduced below by episode state

retry:
   RECORD_IO_EVENT(
         -1,
         IE_OPEN,
         ( *fd_loc = open(filename, (callopts & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR) )
         );
   // if successful, returns file descriptor; if fail, returns -1 and errno is set

   if (*fd_loc >= 0 && force_failure_i2c_open)  { // for testing
      close(*fd_loc);
      *fd_loc = -1;
      errno = EACCES;
   }

   if (*fd_loc < 0) {
      int errsv = -errno;
      char * msg = g_strdup_printf("open(%s) failed. errno=%s", filename, psc_desc(errsv));
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s", msg);
      free(msg);
      err = ERRINFO_NEW(errsv,  "Open failed for %s, errno=%s in file %s near line %d",
               filename, psc_desc(errsv), __FILE__, __LINE__);

      if (err->status_code == -EACCES) {
         DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "%s", err->detail);
         // CLOCK_BOOTTIME, not the CLOCK_REALTIME used for the timings
         // elsewhere in the tree.  This is control flow, not measurement: a
         // forward step of the wall clock by 3 to 10 seconds inside a live
         // episode would make the budget look spent while leaving the quiet
         // gap below unreached, denying every retry until 10 seconds pass
         // without a failure.  CLOCK_MONOTONIC is not a substitute: it does
         // not advance across suspend, so an episode opened before a long
         // suspend would still look live on resume and the first open after
         // it would inherit that stale budget, which is exactly the case
         // this retry ladder exists to serve.
         uint64_t now_ns = cur_boot_time_nanosec();
         uint64_t prior_last_seen_ns = eacces_last_seen_ns;
         eacces_last_seen_ns = now_ns;
         if (eacces_retry_ct == 0) {
            // Establish this call's retry budget from the process-wide episode.
            // Read the episode start once, into a local: a concurrent open
            // that succeeded on retry may clear it at any point, and a zero
            // read must start a new episode rather than be differenced, which
            // would yield an epoch-sized elapsed time and a budget of zero.
            uint64_t episode_start_ns = eacces_episode_start_ns;
            if (prior_last_seen_ns == 0 || episode_start_ns == 0 ||
                now_ns - prior_last_seen_ns > MILLIS2NANOS(EACCES_NEW_EPISODE_QUIET_MS))
            {
               episode_start_ns = now_ns;           // new episode
               eacces_episode_start_ns = now_ns;
            }
            uint64_t episode_elapsed_ms = NANOS2MILLIS(now_ns - episode_start_ns);
            eacces_budget_ms = (episode_elapsed_ms >= (uint64_t) max_eacces_retry_ms)
                  ? 0
                  : max_eacces_retry_ms - (int) episode_elapsed_ms;
            if (eacces_budget_ms == 0)
               DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE,
                     "Retry budget for the current EACCES episode already exhausted, not retrying");

            // During the post-resume EACCES window every bus open fails, and
            // stabilization rescans multiply the failures.  Emit the expensive
            // diagnostics (traced function stack dump, open failure diagnosis,
            // which forks getfacl and lsof) at most once per
            // rate_limit_eacces_diagnostics_interval_sec (option --i13),
            // process wide, not once per failing open: a single rescan of
            // this machine's buses would otherwise produce one full dump per
            // inaccessible bus.  An interval of 0 disables the limit.
            bool emit_diagnostics = true;
            if (rate_limit_eacces_diagnostics_interval_sec > 0) {
               // 0 means not yet emitted, and must be tested explicitly.
               // Differencing against it worked only while this used
               // CLOCK_REALTIME, whose magnitude always exceeds the interval.
               // Under CLOCK_BOOTTIME, chosen for the same reason as above,
               // the first 10 seconds after boot fall inside the interval,
               // which would suppress the diagnostics for the EACCES window
               // at login that they are most wanted for.
               static _Atomic uint64_t last_eacces_diagnostics_ns = 0;
               const uint64_t interval_ns =
                     SECS2NANOS(rate_limit_eacces_diagnostics_interval_sec);
               uint64_t diag_now_ns = cur_boot_time_nanosec();
               uint64_t prior_diag_ns = last_eacces_diagnostics_ns;
               emit_diagnostics = (prior_diag_ns == 0) ||
                                  (diag_now_ns - prior_diag_ns > interval_ns);
               if (emit_diagnostics)
                  last_eacces_diagnostics_ns = diag_now_ns;
            }
            if (emit_diagnostics) {
               TRACED_FUNCTION_STACK_TO_SYSLOG(DDCA_SYSLOG_ERROR, TFS_MOST_RECENT_LAST);
               diagnose_open_failure_to_syslog(filename, err->detail);
            }

#ifdef REDUNDANT_WITH_DIAGNOSE_OPEN_FAILURE
            // Reported to explain the failure if the retries do not succeed.
            // Neither condition alters the retry budget: a permission that is
            // absent because udev has not yet reapplied the ACL is
            // indistinguishable, at this point, from one that is absent
            // because the user lacks access altogether.
            if (!is_cur_user_acl_rw(filename)) {
               DECORATED_SYSLOG(DDCA_SYSLOG_WARNING, "User ACL is not RW");
               bool has_group_perms = cur_user_has_group_i2c_perms(filename);
               DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "Current user %s group i2c perms on %s",
                     (has_group_perms) ? "has" : "does not have", filename);
            }
#endif
         }

         if (eacces_retry_ct       < max_eacces_retry_ct &&
             total_eacces_retry_ms < eacces_budget_ms )
         {
            errinfo_free(err);
            err = NULL;
            total_eacces_retry_ms += eacces_retry_interval_ms;
            eacces_retry_ct++;
            SLEEP_MILLIS_WITH_SYSLOG(eacces_retry_interval_ms, "EACCES retry_ct=%d", eacces_retry_ct);
            // Back off, so that a momentary gap is waited out quickly while a
            // longer one does not consume the budget in short retries.
            eacces_retry_interval_ms *= 2;
            if (eacces_retry_interval_ms > EACCES_RETRY_MAX_INTERVAL_MS)
               eacces_retry_interval_ms = EACCES_RETRY_MAX_INTERVAL_MS;
            goto retry;
         }
      }
   }

   if ( ERRINFO_STATUS(err) == -EACCES)
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "open() failed with %d EACCES errors, total retry ms = %d",
            eacces_retry_ct, total_eacces_retry_ms);
   if (!err && eacces_retry_ct > 0) {
      DECORATED_SYSLOG(DDCA_SYSLOG_NOTICE, "open() succeeded with %d EACCES retries after %d millisec",
            eacces_retry_ct, total_eacces_retry_ms);
      // The transient condition cleared; end the episode so that the next
      // EACCES failure, whenever it comes, gets a fresh retry budget.
      eacces_episode_start_ns = 0;
      eacces_last_seen_ns = 0;
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "*fd_loc=%d, eacces_retry_ct=%d", *fd_loc, eacces_retry_ct);
   return err;
}


/** Opens a /dev/i2c device specified by its bus number, without further checks
 *  @param  busno      I2C bus number
 *  @param  callopts   if bit CALLOPT_RDONLY set, open RO, otherwise open RW
 *  @param  fd_loc     address which to return file descriptor, -1 if failure
 *  @return Error_Info struct if error, NULL if success
 */
Error_Info *
i2c_open_bus_basic_by_busno(int busno,  Byte callopts, int* fd_loc) {
   char busname[20];
   g_snprintf(busname, 20, "/dev/i2c-%d", busno);
   Error_Info * err = i2c_open_bus_basic(busname, callopts, fd_loc);
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
Error_Info *
i2c_open_bus(
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
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "open(%s) succeeded, tryctr=%d",filename,tryctr);
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
             cur_error = ERRINFO_NEW(flockrc, "flock_lock_by_fd(%s) returned %s",
                                              filename, psc_desc(flockrc));
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
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "Cross instance locking succeeded for %s", filename);
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
             MSG_W_SYSLOG(DDCA_SYSLOG_ERROR,
                   "unlock_display_by_dpath(%s) returned %d",dpath_repr_t(&dpath),err->status_code);
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
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Total wait %d exceeds max wait %d, tries=%d",
                                             total_wait_millisec, open_max_wait_millisec, tryctr);
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


/** Close an open /dev/i2c device
 *  @param  busno  /dev/i2c bus number
 *  @param  fd     file descriptor for open device
 *  @param  callopts  if bit CALLOPT_ERR_MSG set, write error message to terminal
 *  @return 0 if success, -errno if error
 *
 *  If an error occurs, a message is written to the system log
 */
Status_Errno
i2c_close_bus_basic(int busno, int fd, Call_Options callopts) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d, fd=%d, callopts=0x%02x", busno, fd, callopts);

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
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "Close failed for %s, errno=%s\n",
            filename_for_fd_t(fd), linux_errno_desc(errsv));
      // assert(rc == 0);     // don't bother with recovery for now
   }

   DBGTRC_RET_DDCRC(debug, DDCA_TRC_NONE, result, "");
   return result;
}


/** Closes an open I2C bus device, releasing cross-instance and
 *  cross-thread locks
 *
 * @param  busno     i2c_bus_number
 * @param  fd        Linux file descriptor
 * @param  callopts  call option flags, controlling failure action
 *
 * @retval 0  success
 * @retval <0 negative Linux errno value if close fails
 */
Status_Errno
i2c_close_bus(int busno, int fd, Call_Options callopts) {
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
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "/dev/i2c-%d.  i2c_close_bus_basic() returned %d", busno, result);
   // assert(result == 0);   // TODO; handle failure

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
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "%s", s);
      free(s);
      errinfo_free(erec);
   }

   assert(result <= 0);
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "busno=%d, fd=%d",busno, fd);
   return result;
}


//
// Initialization
//

void init_i2c_bus_open_close() {
   open_failures_reported = EMPTY_BIT_SET_256;

   RTTI_ADD_FUNC(i2c_simple_rw_test);
   RTTI_ADD_FUNC(i2c_open_bus_basic);
   RTTI_ADD_FUNC(i2c_open_bus);
   RTTI_ADD_FUNC(i2c_close_bus_basic);
   RTTI_ADD_FUNC(i2c_close_bus);
}
