/** \f i2c_display_lock.c
 *  Provides locking for displays to ensure that a given display is not
 *  opened simultaneously from multiple threads.
 */

// Copyright (C) 2018-2024 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <glib-2.0/glib.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/error_info.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/displays.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"

#include "i2c/i2c_display_lock.h"

#include "ddcutil_types.h"
#include "ddcutil_status_codes.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;


static bool lock_rec_matches_io_path(Display_Lock_Record * ddesc, DDCA_IO_Path path) {
   bool result = false;
   // i2c_busno and hiddev are same size, to following works for DDCA_IO_USB
   if (ddesc->io_path.io_mode == path.io_mode && ddesc->io_path.path.i2c_busno == path.path.i2c_busno)
      result = true;
   return result;
}


#ifdef UNUSED
static bool lock_rec_matches_dref(Display_Lock_Record * ddesc, Display_Ref * dref) {
   bool result = false;
   if (dpath_eq(ddesc->io_path, dref->io_path))
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
      Display_Lock_Record * new_desc = calloc(1, sizeof(Display_Lock_Record));
      memcpy(new_desc->marker, DISPLAY_LOCK_MARKER, 4);
      new_desc->io_path           = io_path;
      g_mutex_init(&new_desc->display_mutex);
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


/** Locks a distinct display.
 *
 *  \param  id                 distinct display identifier
 *  \param  flags              if **DDISP_WAIT** set, wait for locking
 *  \retval NULL               success
 *  \retval Error_Info(DDCRC_LOCKED)       locking failed, display already locked by another
 *                                         thread and DDISP_WAIT not set
 *  \retval Error_Info(DDCRC_ALREADY_OPEN) display already locked in current thread
 */
Error_Info *
lock_display(
      Display_Lock_Record * ddesc,
      Display_Lock_Flags flags)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "ddesc=%p -> %s", ddesc, lockrec_repr_t(ddesc));

   Error_Info * err = NULL;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(ddesc->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   bool self_thread = false;
   g_mutex_lock(&master_display_lock_mutex);  //wrong - will hold lock during wait
   if (ddesc->display_mutex_thread == g_thread_self() )
      self_thread = true;
   g_mutex_unlock(&master_display_lock_mutex);
   if (self_thread) {
      MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Attempting to lock display already locked by current thread, tid=%jd", get_thread_id());
      err = errinfo_new(DDCRC_ALREADY_OPEN, __func__,   // is there a better status code?
            "Attempting to lock display already locked by current thread"); // poor
      goto bye;
   }

   bool locked = true;
   if (flags & DDISP_WAIT) {
      g_mutex_lock(&ddesc->display_mutex);
   }
   else {
      locked = g_mutex_trylock(&ddesc->display_mutex);
      if (!locked)
         err = errinfo_new(DDCRC_LOCKED, __func__, "Locking failed");
   }

   if (locked) {  // note that this thread owns the lock
       ddesc->display_mutex_thread = g_thread_self();
       ddesc->linux_thread_id = get_thread_id();
   }

bye:
   // need a new DDC status code
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "ddesc=%p -> %s", ddesc, lockrec_repr_t(ddesc));
   if (err)
      show_backtrace(2);
   return err;
}


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
   DBGTRC_STARTING(debug, TRACE_GROUP, "dpath=%s, flags=0x%02x", dpath_repr_t(&dpath), flags);
   Display_Lock_Record * lockid = get_display_lock_record_by_dpath(dpath);
   Error_Info * result = lock_display(lockid, flags);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, result, "dpath=%s", dpath_repr_t(&dpath));
   return result;
}



/** Unlocks a distinct display.
 *
 *  \param id  distinct display identifier
 *  \retval Error_Info(DDCRC_LOCKED) attempting to unlock a display owned by a different thread
 *  \retval NULL   no error
 */
Error_Info *
unlock_display(Display_Lock_Record * ddesc) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "ddesc=%p -> %s", ddesc, lockrec_repr_t(ddesc));
   Error_Info * err = NULL;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(ddesc->marker, DISPLAY_LOCK_MARKER, 4) == 0);
   g_mutex_lock(&master_display_lock_mutex);
   if (ddesc->display_mutex_thread != g_thread_self()) {
      SYSLOG2(DDCA_SYSLOG_ERROR, "Attempting to unlock display lock owned by different thread");
      err = errinfo_new(DDCRC_LOCKED, __func__, "Attempting to unlock display lock owned by different thread");
   }
   else {
      ddesc->display_mutex_thread = NULL;
      ddesc->linux_thread_id = 0;
      g_mutex_unlock(&ddesc->display_mutex);
   }
   g_mutex_unlock(&master_display_lock_mutex);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "ddesc=%p -> %s", ddesc, lockrec_repr_t(ddesc));
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
   bool debug = true;
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


/** Initializes this module */
void
init_i2c_display_lock(void) {
   lock_records= g_ptr_array_new_with_free_func(g_free);

   RTTI_ADD_FUNC(get_display_lock_record_by_dpath);
   RTTI_ADD_FUNC(lock_display);
   RTTI_ADD_FUNC(lock_display_by_dpath);
   RTTI_ADD_FUNC(unlock_display);
   RTTI_ADD_FUNC(unlock_display_by_dpath);
#ifdef UNUSED
   RTTI_ADD_FUNC(get_display_lock_record_by_dref);
   RTTI_ADD_FUNC(lock_display_by_dref);
   RTTI_ADD_FUNC(unlock_display_by_dref);
#endif
}


void
terminate_i2c_display_lock() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "");
   g_ptr_array_free(lock_records, true);
   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "");
}
