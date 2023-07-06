/** \f ddc_display_lock.c
 *  Provides locking for displays to ensure that a given display is not
 *  opened simultaneously from multiple threads.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/displays.h"
#include "base/rtti.h"
#include "base/status_code_mgt.h"

#include "ddc/ddc_display_lock.h"

#include "ddcutil_types.h"
#include "ddcutil_status_codes.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

#define DISTINCT_DISPLAY_DESC_MARKER "DDSC"
typedef struct {
   char         marker[4];
   DDCA_IO_Path io_path;
   GMutex       display_mutex;
   GThread *    display_mutex_thread;     // thread owning mutex
} Distinct_Display_Desc;



static bool display_desc_matches(Distinct_Display_Desc * ddesc, Display_Ref * dref) {
   bool result = false;
   if (dpath_eq(ddesc->io_path, dref->io_path))
      result = true;
   return result;
}


static GPtrArray * display_descriptors = NULL;  // array of Distinct_Display_Desc *
static GMutex descriptors_mutex;                // single threads access to display_descriptors
static GMutex master_display_lock_mutex;


// must be called when lock not held by current thread, o.w. deadlock
static char *
distinct_display_ref_repr_t(Distinct_Display_Ref id) {
   static GPrivate  repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&repr_key, 100);
   g_mutex_lock(&descriptors_mutex);
   Distinct_Display_Desc * ref = (Distinct_Display_Desc *) id;
   assert(memcmp(ref->marker, DISTINCT_DISPLAY_DESC_MARKER, 4) == 0);
   g_snprintf(buf, 100, "Distinct_Display_Ref[%s @%p]", dpath_repr_t(&ref->io_path), ref);
   g_mutex_unlock(&descriptors_mutex);
   return buf;
}


Distinct_Display_Ref
get_distinct_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s", dref_repr_t(dref));

   void * result = NULL;
   g_mutex_lock(&descriptors_mutex);

   for (int ndx=0; ndx < display_descriptors->len; ndx++) {
      Distinct_Display_Desc * cur = g_ptr_array_index(display_descriptors, ndx);
      if (display_desc_matches(cur, dref) ) {
         result = cur;
         break;
      }
   }
   if (!result) {
      Distinct_Display_Desc * new_desc = calloc(1, sizeof(Distinct_Display_Desc));
      memcpy(new_desc->marker, DISTINCT_DISPLAY_DESC_MARKER, 4);
      new_desc->io_path           = dref->io_path;
      g_mutex_init(&new_desc->display_mutex);
      g_ptr_array_add(display_descriptors, new_desc);
      result = new_desc;
   }

   g_mutex_unlock(&descriptors_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p -> %s", result,  distinct_display_ref_repr_t(result));
   return result;
}


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
      Distinct_Display_Ref   id,
      Display_Lock_Flags flags)
{
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "id=%p -> %s", id, distinct_display_ref_repr_t(id));

   Error_Info * err = NULL;
   Distinct_Display_Desc * ddesc = (Distinct_Display_Desc *) id;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(ddesc->marker, DISTINCT_DISPLAY_DESC_MARKER, 4) == 0);
   bool self_thread = false;
   g_mutex_lock(&master_display_lock_mutex);  //wrong - will hold lock during wait
   if (ddesc->display_mutex_thread == g_thread_self() )
      self_thread = true;
   g_mutex_unlock(&master_display_lock_mutex);
   if (self_thread) {
      MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "Attempting to lock display already locked by current thread");
      err = errinfo_new(DDCRC_ALREADY_OPEN, __func__,   // is there a better status code?
            "Attempting to lock display already locked by current thread"); // poor
   }
   else {
      bool locked = true;
      if (flags & DDISP_WAIT) {
         g_mutex_lock(&ddesc->display_mutex);
      }
      else {
         locked = g_mutex_trylock(&ddesc->display_mutex);
         if (!locked)
            err = errinfo_new(DDCRC_LOCKED, __func__, "Locking failed");
      }

      if (locked)   // record that this thread owns the lock
          ddesc->display_mutex_thread = g_thread_self();
   }
   // need a new DDC status code
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "id=%p -> %s", id, distinct_display_ref_repr_t(id));
   return err;
}


/** Unlocks a distinct display.
 *
 *  \param id  distinct display identifier
 *  \retval Error_Info(DDCRC_LOCKED) attempting to unlock a display owned by a different thread
 *  \retval NULL   no error
 */
Error_Info *
unlock_display(Distinct_Display_Ref id) {
   bool debug = true;
   DBGTRC_STARTING(debug, TRACE_GROUP, "id=%p -> %s", id, distinct_display_ref_repr_t(id));
   Error_Info * err = NULL;
   Distinct_Display_Desc * ddesc = (Distinct_Display_Desc *) id;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(ddesc->marker, DISTINCT_DISPLAY_DESC_MARKER, 4) == 0);
   g_mutex_lock(&master_display_lock_mutex);
   if (ddesc->display_mutex_thread != g_thread_self()) {
      SYSLOG2(DDCA_SYSLOG_ERROR, "Attempting to unlock display lock owned by different thread");
      err = errinfo_new(DDCRC_LOCKED, __func__, "Attempting to unlock display lock owned by different thread");
   }
   else {
      ddesc->display_mutex_thread = NULL;
      g_mutex_unlock(&ddesc->display_mutex);
   }
   g_mutex_unlock(&master_display_lock_mutex);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "id=%p -> %s", id, distinct_display_ref_repr_t(id));
   return err;
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
    for (int ndx=0; ndx < display_descriptors->len; ndx++) {
       Distinct_Display_Desc * cur = g_ptr_array_index(display_descriptors, ndx);
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
   rpt_vstring(depth, "display_descriptors@%p", display_descriptors);
   g_mutex_lock(&descriptors_mutex);
   int d1 = depth+1;
   for (int ndx=0; ndx < display_descriptors->len; ndx++) {
      Distinct_Display_Desc * cur = g_ptr_array_index(display_descriptors, ndx);
      rpt_vstring(d1, "%2d - %p  %-28s  thread ptr=%p",
                       ndx, cur,
                       dpath_repr_t(&cur->io_path), (void*) &cur->display_mutex_thread );
   }
   g_mutex_unlock(&descriptors_mutex);
}


/** Initializes this module */
void
init_ddc_display_lock(void) {
   display_descriptors= g_ptr_array_new_with_free_func(g_free);

   RTTI_ADD_FUNC(get_distinct_display_ref);
   RTTI_ADD_FUNC(lock_display);
   RTTI_ADD_FUNC(unlock_display);
}


void
terminate_ddc_display_lock() {
   g_ptr_array_free(display_descriptors, true);
}
