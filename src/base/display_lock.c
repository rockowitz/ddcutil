/** \f display_lock.c
 *  Provides locking for displays to ensure that a given display is not
 *  opened simultaneously from multiple threads.
 */

// Copyright (C) 2018-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
#ifdef TOO_MANY_EDGE_CASES
 *  It is conceivable that there are multiple paths to the same monitor, e.g.
 *  multiple cables from different video card outputs, or USB as well as I2c
 *  connections.  Therefore, this module checks the manufacturer id, model string,
 *  and ascii serial number fields in the EDID when comparing monitors.
 *
 *  (Note that comparing the full 128 byte EDIDs will not work, as a monitor can
 *  return different EDIDs on different inputs, e.g. VGA vs DVI.
#endif
 *
 *  Only the io path to the display is checked.
 */

/* 5/2023:
 *
 * This method of locking is vestigial from the time that there could be more
 * than one Display_Ref for a display, which could be held in different threads.
 *
 * The code could be simplified, or eliminated almost entirely,  e.g. by recording
 * in the Display_Ref which thread has  opened the display.
 *
 * Given the imminent release of 2.0.0-rc1, such changes are left to a future release.
 */

#include <assert.h>
#include <base/display_lock.h>
#include <glib-2.0/glib.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/error_info.h"
#include "util/glib_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "ddcutil_types.h"
#include "ddcutil_status_codes.h"

#include "base/displays.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#ifdef ALT_LCOK_REC
#include "usb/usb_displays.h"   // forward ref, need to split out usb_displays_base.h
#endif

#include "base/display_lock.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

char * interpret_display_lock_flags_t(Display_Lock_Flags lock_flags) {
   static GPrivate  buf_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&buf_key, 200);

   if (lock_flags & DDISP_WAIT)
      strcpy(buf, "DDISP_WAIT");
   else
      strcpy(buf, "DDISP_NONE");

   return buf;
}


static bool lock_rec_matches_io_path(Display_Lock_Record * dlr, DDCA_IO_Path path) {
   bool result = false;
   // i2c_busno and hiddev are same size, to following works for DDCA_IO_USB
   if (dlr->io_path.io_mode == path.io_mode && dlr->io_path.path.i2c_busno == path.path.i2c_busno)
      result = true;
   return result;
}


#ifdef UNUSED
static bool lock_rec_matches_dref(Display_Lock_Record * dlr, Display_Ref * dref) {
   bool result = false;
   if (dpath_eq(dlr->io_path, dref->io_path))
      result = true;
   return result;
}
#endif


static GPtrArray * lock_records = NULL;   // array of Diaplay_Lock_Record *
static GMutex descriptors_mutex;          // single threads access to lock records
static GMutex master_display_lock_mutex;


// must be called when lock not held by current thread, o.w. deadlock
static char *
lockrec_repr_t(Display_Lock_Record * ref) {
   static GPrivate  repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&repr_key, 100);
   g_mutex_lock(&descriptors_mutex);
   assert(memcmp(ref->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   g_snprintf(buf, 100, "Display_Lock_Record[%s tid=%jd @%p]", dpath_repr_t(&ref->io_path), ref->linux_thread_id, ref);
   g_mutex_unlock(&descriptors_mutex);
   return buf;
}


Display_Lock_Record * create_display_lock_record(DDCA_IO_Path io_path) {
   Display_Lock_Record * new_desc = calloc(1, sizeof(Display_Lock_Record));
   memcpy(new_desc->marker, DISPLAY_LOCK_MARKER, 4);
   new_desc->io_path           = io_path;
   g_mutex_init(&new_desc->display_mutex);
   return new_desc;
}


static Display_Lock_Record *
get_display_lock_record_by_dpath(DDCA_IO_Path io_path) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "io_path=%s", dpath_repr_t(&io_path));

   Display_Lock_Record * result = NULL;
   g_mutex_lock(&descriptors_mutex);

   for (int ndx=0; ndx < lock_records->len; ndx++) {
      Display_Lock_Record * cur = g_ptr_array_index(lock_records, ndx);
      if (lock_rec_matches_io_path(cur, io_path) ) {
         result = cur;
         break;
      }
   }
   if (!result) {
      Display_Lock_Record * new_desc = create_display_lock_record(io_path);
      g_ptr_array_add(lock_records, new_desc);
      result = new_desc;
   }

   g_mutex_unlock(&descriptors_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p -> %s", result,  lockrec_repr_t(result));
   return result;
}


#ifdef UNUSED
static Display_Lock_Record *
get_display_lock_record_by_dref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s", dref_repr_t(dref));

   Display_Lock_Record * result = get_display_lock_record_by_dpath(dref->io_path);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p -> %s", result, lockrec_repr_t(result));
   return result;
}
#endif



#define EMIT_BACKTRACE(_ddcutil_severity, _format, ...) \
do { \
   if (!msg_to_syslog_only) { \
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, _format, ##__VA_ARGS__); \
      if (IS_DBGTRC(debug, DDCA_TRC_NONE)) { \
         show_backtrace(0);          \
         dbgrpt_current_traced_function_stack(false, true); \
      } \
   } \
   if (test_emit_syslog(_ddcutil_severity)) { \
      int syslog_priority = syslog_importance_from_ddcutil_syslog_level(_ddcutil_severity);  \
      if (syslog_priority >= 0) { \
         char * body = g_strdup_printf(_format, ##__VA_ARGS__); \
         syslog(syslog_priority, PRItid" %s%s", (intmax_t) tid(), body, (tag_output) ? " (R)" : "" ); \
         free(body); \
         backtrace_to_syslog(syslog_priority, 2); \
         current_traced_function_stack_to_syslog(syslog_priority, false); \
      } \
   } \
} while(0)


/** Locks a distinct display.
 *
 *  \param  dlr              Display_Lock_Record distinct display identifier
 *  \param  flags              if **DDISP_WAIT** set, wait for locking
 *  \retval NULL               success
 *  \retval Error_Info(DDCRC_LOCKED)       locking failed, display already locked by another
 *                                         thread and DDISP_WAIT not set
 *  \retval Error_Info(DDCRC_ALREADY_OPEN) display already locked in current thread
 */
Error_Info *
lock_display(
      Display_Lock_Record * dlr,
      Display_Lock_Flags flags)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dlr->io_path=%s, dlr->linux_thread_id=%jd flags=%s",
         dpath_short_name_t(&dlr->io_path), dlr->linux_thread_id, interpret_display_lock_flags_t(flags));

   Error_Info * err = NULL;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(dlr->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   bool self_thread = false;
   bool locked = false;
   g_mutex_lock(&master_display_lock_mutex);
   if (dlr->display_mutex_thread == g_thread_self() )
      self_thread = true;
   g_mutex_unlock(&master_display_lock_mutex);
   if (self_thread) {
      EMIT_BACKTRACE(DDCA_SYSLOG_ERROR,
            "Attempting to lock display already locked by current thread, tid=%jd", TID());
      err = errinfo_new(DDCRC_ALREADY_OPEN, __func__,   // is there a better status code?
            "Attempting to lock display already locked by current thread"); // poor
      goto bye;
   }

   if (flags & DDISP_WAIT) {
      g_mutex_lock(&dlr->display_mutex);
      locked = true;
   }
   else {
      int lock_max_wait_millisec = DEFAULT_OPEN_MAX_WAIT_MILLISEC;
      int lock_wait_interval_millisec = DEFAULT_OPEN_WAIT_INTERVAL_MILLISEC;
      int total_wait_millisec = 0;
      int tryctr = 0;

      while (!locked && total_wait_millisec < lock_max_wait_millisec) {
         tryctr++;
         locked = g_mutex_trylock(&dlr->display_mutex);
         if (!locked) {
            DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "g_mutex_trylock() failed, dref=%s", 
                  dpath_short_name_t(&dlr->io_path));
            SLEEP_MILLIS_WITH_STATS(lock_wait_interval_millisec);
            total_wait_millisec += lock_wait_interval_millisec;
         }
      }

      if (locked) {  // note that this thread owns the lock
          if (tryctr > 1) {
             EMIT_BACKTRACE(DDCA_SYSLOG_NOTICE, PRItid"Locked %s after %d tries",
                   TID(), dpath_short_name_t(&dlr->io_path), tryctr);
          }
      }
      else {
         EMIT_BACKTRACE(DDCA_SYSLOG_ERROR, PRItid"Failed to Lock %s after %d tries. Locked by thread"PRItid,
               TID(), dpath_short_name_t(&dlr->io_path), tryctr, dlr->linux_thread_id);
         err = errinfo_new(DDCRC_LOCKED, __func__, "Locking failed for %s after %d tries. Locked by thread"PRItid,
               dpath_short_name_t(&dlr->io_path), tryctr, dlr->linux_thread_id);
      }
   }

bye:
   if (locked) {  // note that this thread owns the lock
        dlr->display_mutex_thread = g_thread_self();
        dlr->linux_thread_id = get_thread_id();
   }

   // need a new DDC status code
   // DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "dlr->io_path=%s", dpath_short_name_t(&dlr->io_path));
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "");
   return err;
}


#ifdef UNUSED    // For future use? n. coverity complains
int      lockrec_poll_millisec = DEFAULT_FLOCK_POLL_MILLISEC;   // *** TEMP ***
int      lockrec_max_wait_millisec = DEFAULT_FLOCK_MAX_WAIT_MILLISEC;


Error_Info *
lock_display2(
      Display_Lock_Record * dlr,
      Display_Lock_Flags flags)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "device %s, flags=%s",
         dpath_repr_t(&dlr->io_path),  interpret_display_lock_flags_t(flags));
   Error_Info * err = NULL;
   TRACED_ASSERT(memcmp(dlr->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   uint64_t lockrec_poll_microsec = 1000 * (uint64_t) lockrec_poll_millisec;

   //DBGTRC_STARTING(debug, TRACE_GROUP, "dlr=%p -> %s, flags=%s",
   //      dlr, lockrec_repr_t(dlr), interpret_display_lock_flags_t(flags));

   if (dlr->display_mutex_thread == g_thread_self() ) {
      char buf[80];
      g_snprintf(buf, 80,
            "Attempting to lock device %s already locked by current thread %jd",
            dpath_repr_t(&dlr->io_path), get_thread_id());
      MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s", buf);
      err = ERRINFO_NEW(DDCRC_ALREADY_OPEN, "%s", buf);
      goto bye;
   }

   int total_wait_millisec = 0;
   uint64_t max_wait_millisec = (flags & DDISP_WAIT) ? lockrec_max_wait_millisec : 0;
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
          "flock_poll_millisec=%d, flock_max_wait_millisec=%d ",
          lockrec_poll_millisec, lockrec_max_wait_millisec);

    int lock_call_ctr = 0;
    bool locked = false;

    while(!locked && total_wait_millisec <= max_wait_millisec) {
       lock_call_ctr++;
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "Calling g_mutex_trylcock(), mutex=%p, device=%s, lock_call_ctr=%d, total_wait_millisec %d",
             dlr->display_mutex, dpath_repr_t(&dlr->io_path), lock_call_ctr, total_wait_millisec);

       bool locked = g_mutex_trylock(&dlr->display_mutex);
       if (locked) {
           continue;
       }

       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Sleeping for %d millisec", lockrec_poll_millisec);
       g_usleep(lockrec_poll_microsec);
       total_wait_millisec += lockrec_poll_millisec;
    }

    if (locked) {
       DBGTRC(debug, DDCA_TRC_NONE, "Lock succeeded after %d tries and %d millisec",
             lock_call_ctr, total_wait_millisec);
       dlr->display_mutex_thread = g_thread_self();
       dlr->linux_thread_id = get_thread_id();
    }
    else {
       // living dangerously, but it's an error msg
       MSG_W_SYSLOG(DDCA_SYSLOG_ERROR,
             "Max wait time for %s exceeded after %d g_mutex_lock() calls",
             dpath_repr_t(&dlr->io_path), lock_call_ctr);
       MSG_W_SYSLOG(DDCA_SYSLOG_ERROR,
             "Lock currently held by thread %jd", dlr->linux_thread_id);
       err = ERRINFO_NEW(DDCRC_LOCKED, "Locking failed for %s. Apparently held by thread %jd",
             dpath_repr_t(&dlr->io_path), dlr->linux_thread_id);
    }


bye:
    DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "");
    return err;
}
#endif


#ifdef UNUSED
/** Locks a display.
 *
 *  \param  dref               display reference
 *  \param  flags              if **DDISP_WAIT** set, wait for locking
 *  \retval NULL               success
 *  \retval Error_Info(DDCRC_LOCKED)       locking failed, display already locked by another
 *                                         thread and DDISP_WAIT not set
 *  \retval Error_Info(DDCRC_ALREADY_OPEN) display already locked in current thread
 */
Error_Info *
lock_display_by_dref(
      Display_Ref *      dref,
      Display_Lock_Flags flags)
{
    bool debug = false;
    DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s, flags=0x%02x", dref_repr_t(dref), flags);
    Display_Lock_Record * lockid = get_display_lock_record_by_dref(dref);
    Error_Info * result = lock_display(lockid, flags);
    DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "dref=%s", dref_repr_t(dref));
    return result;
}
#endif


/** Locks a display, specified by its io path
 *
 *  \param  dpath              display path
 *  \param  flags              if **DDISP_WAIT** set, wait for locking
 *  \retval NULL               success
 *  \retval Error_Info(DDCRC_LOCKED)       locking failed, display already locked by another
 *                                         thread and DDISP_WAIT not set
 *  \retval Error_Info(DDCRC_ALREADY_OPEN) display already locked in current thread
 */
Error_Info *
lock_display_by_dpath(
      DDCA_IO_Path       dpath,
      Display_Lock_Flags flags)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dpath=%s, flags=0x%02x=%s",
         dpath_repr_t(&dpath), flags, interpret_display_lock_flags_t(flags));
   Display_Lock_Record * lockid = get_display_lock_record_by_dpath(dpath);
   Error_Info * result = lock_display(lockid, flags);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "dpath=%s", dpath_repr_t(&dpath));
   return result;
}


#ifdef UNUSED
Error_Info *
unlock_display2(Display_Lock_Record * dlr) {
   bool debug = false;
   assert(dlr);
   DBGTRC_STARTING(debug, TRACE_GROUP, "dlr->io_path=%s, dlr->linux_thread_id=%jd ",
         dpath_short_name_t(&dlr->io_path), dlr->linux_thread_id);
   Error_Info * err = NULL;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(dlr->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   char buf[80];
   intmax_t lock_tid = dlr->linux_thread_id;
   if (lock_tid != get_thread_id()) {
      if (lock_tid == 0) {
         g_snprintf(buf, 80, "Attempting to unlock device %s not currently locked",
                             dpath_repr_t(&dlr->io_path));
      }
      else {
         g_snprintf(buf, 80, "Attempting to unlock device %s locked by different thread %jd",
                    dpath_repr_t(&dlr->io_path), lock_tid);
      }
      SYSLOG2(DDCA_SYSLOG_ERROR, "%s", buf);
      err = ERRINFO_NEW(DDCRC_LOCKED, "%s", buf);
   }
   else {
      dlr->display_mutex_thread = NULL;
      dlr->linux_thread_id = 0;
      g_mutex_unlock(&dlr->display_mutex);
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "dlr->io_path=%s", dpath_short_name_t(&dlr->io_path));
   return err;
}
#endif


/** Unlocks a distinct display.
 *
 *  \param id  distinct display identifier
 *  \retval Error_Info(DDCRC_LOCKED) attempting to unlock a display owned by a different thread
 *  \retval NULL   no error
 */
Error_Info *
unlock_display(Display_Lock_Record * dlr) {
   bool debug = false;
   // DBGTRC_STARTING(debug, TRACE_GROUP, "dlr=%p -> %s", dlr, lockrec_repr_t(dlr));
   DBGTRC_STARTING(debug, TRACE_GROUP, "dlr->io_path=%s", dpath_short_name_t(&dlr->io_path));
   Error_Info * err = NULL;

   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(dlr->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   g_mutex_lock(&master_display_lock_mutex);
   intmax_t current_thread_id = dlr->linux_thread_id;
   // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "old linux_thread_id = %jd", dlr->linux_thread_id);
   if (dlr->display_mutex_thread != g_thread_self()) {
      SYSLOG2(DDCA_SYSLOG_ERROR, "Attempting to unlock display lock owned by different thread");
      err = errinfo_new(DDCRC_LOCKED, __func__, "Attempting to unlock display lock owned by different thread");
   }
   else {
      dlr->display_mutex_thread = NULL;
      dlr->linux_thread_id = 0;
      current_thread_id = 0;
      g_mutex_unlock(&dlr->display_mutex);
   }
   g_mutex_unlock(&master_display_lock_mutex);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "dlr->io_path=%s, final linux_thread_id=%d",
         dpath_repr_t(&dlr->io_path), current_thread_id);
   return err;
}


#ifdef UNUSED
/**  Unlocks a display.
 *
 *  \param  dref   display reference
 *  \retval NULL                      success
 *  \retval Error_Info(DDCRC_LOCKED)  locking failed, display already locked by another thread
 */
Error_Info *
unlock_display_by_dref(
      Display_Ref *      dref)
{
    Display_Lock_Record * lockid = get_display_lock_record_by_dref(dref);
    return unlock_display(lockid);
}
#endif


/**  Unlocks a display, specified by its io path
 *
 *  \param  dpath                     io path
 *  \retval NULL                      success
 *  \retval Error_Info(DDCRC_LOCKED)  locking failed, display already locked by another thread
 */
Error_Info *
unlock_display_by_dpath(
      DDCA_IO_Path   dpath)
{
    Display_Lock_Record * lockid = get_display_lock_record_by_dpath(dpath);
    return unlock_display(lockid);
}


#ifdef BAD
/** Unlocks all distinct displays.
 *
 *  The function is used during reinitialization.
 *
 *  BUG: DO NOT USE
 */
void unlock_all_distinct_displays() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   g_mutex_lock(&master_display_lock_mutex);   // are both locks needed?
   g_mutex_lock(&descriptors_mutex);
    for (int ndx=0; ndx < lock_records->len; ndx++) {
       Display_Lock_Record * cur = g_ptr_array_index(lock_records, ndx);
       DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%2d - %p  %-28s",
                                           ndx, cur,
                                           dpath_repr_t(&cur->io_path) );

       // WRONG: Attempt to unlock mutex that was not locked
       //        Aborted (core dumped)
       // Calling g_mutex_unlock() on a mutex that is not locked by the current thread leads to undefined behaviour.
       g_mutex_unlock(&cur->display_mutex);
    }
    g_mutex_unlock(&descriptors_mutex);
    g_mutex_unlock(&master_display_lock_mutex);
    DBGTRC_DONE(debug, TRACE_GROUP, "");
 }
#endif


/** Emits a report of all distinct display descriptors.
 *
 *  \param depth logical indentation depth
 */
void
dbgrpt_display_locks(int depth) {
   rpt_vstring(depth, "display_descriptors@%p", lock_records);
   g_mutex_lock(&descriptors_mutex);
   int d1 = depth+1;
   rpt_label(depth,"index  lock-record-ptr  dpath                         display_mutex_thread");
   for (int ndx=0; ndx < lock_records->len; ndx++) {
      Display_Lock_Record * cur = g_ptr_array_index(lock_records, ndx);
      rpt_vstring(d1, "%2d - %p  %-28s  thread ptr=%p, thread id=%jd",
                       ndx, cur,
                       dpath_repr_t(&cur->io_path),
                       (void*) &cur->display_mutex_thread, cur->linux_thread_id );
   }
   g_mutex_unlock(&descriptors_mutex);
}


int unlock_all_displays_for_current_thread() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "thread = "PRItid, TID());

   int depth = 0;
   int d1 = depth+1;

   if (IS_DBGTRC(debug, TRACE_GROUP)) {
      // dbgrpt_display_locks(depth);
      // rpt_nl();
      rpt_label(depth,"index  lock-record-ptr  dpath                         display_mutex_thread");
   }

   g_mutex_lock(&descriptors_mutex);
   int unlocked_ct = 0;
   for (int ndx=0; ndx < lock_records->len; ndx++) {
      Display_Lock_Record * dlr = g_ptr_array_index(lock_records, ndx);
      if (IS_DBGTRC(debug, TRACE_GROUP)) {
         rpt_vstring(d1, "%2d - %p  %-28s  thread ptr=%p, thread id="PRItid,
                          ndx, dlr,
                          dpath_repr_t(&dlr->io_path),
                          (void*) &dlr->display_mutex_thread, dlr->linux_thread_id );
      }

      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "old linux_thread_id = %jd", dlr->linux_thread_id);
       if (dlr->display_mutex_thread == g_thread_self()) {
          unlocked_ct++;
          dlr->display_mutex_thread = NULL;
          dlr->linux_thread_id = 0;
          g_mutex_unlock(&dlr->display_mutex);
          SYSLOG2(DDCA_SYSLOG_NOTICE, "Unlocked display %s on current thread "PRItid,
                dpath_repr_t(&dlr->io_path), TID() );
       }

   }
   g_mutex_unlock(&descriptors_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d", unlocked_ct);
   return unlocked_ct;
}



/** Initializes this module */
void
init_i2c_display_lock(void) {
   lock_records= g_ptr_array_new_with_free_func(g_free);

   RTTI_ADD_FUNC(get_display_lock_record_by_dpath);
   RTTI_ADD_FUNC(lock_display);
#ifdef UNUSED
   RTTI_ADD_FUNC(lock_display2);
   RTTI_ADD_FUNC(unlock_display2);
#endif
   RTTI_ADD_FUNC(lock_display_by_dpath);
   RTTI_ADD_FUNC(unlock_display);
   RTTI_ADD_FUNC(unlock_display_by_dpath);
#ifdef UNUSED
   RTTI_ADD_FUNC(get_display_lock_record_by_dref);
   RTTI_ADD_FUNC(lock_display_by_dref);
   RTTI_ADD_FUNC(unlock_display_by_dref);
#endif

   RTTI_ADD_FUNC(unlock_all_displays_for_current_thread);

}


void
terminate_i2c_display_lock() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "");
   g_ptr_array_free(lock_records, true);
   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "");
}
