/** \f ddc_display_lock.c
 *  Provides locking for displays to ensure that a given display is not
 *  opened simultaneously from multiple threads.
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>

#include <glib-2.0/glib.h>
#include <string.h>

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
#ifdef TOO_MANY_EDGE_CASES
   char *       edid_mfg;
   char *       edid_model_name;
   char *       edid_serial_ascii;
#endif
   GMutex       display_mutex;
   GThread *    display_mutex_thread;     // thread owning mutex
} Distinct_Display_Desc;


#ifdef REDUNDANT
bool io_path_eq(DDCA_IO_Path path1, DDCA_IO_Path path2) {
   bool result = false;
   if (path1.io_mode == path2.io_mode) {
      switch(path1.io_mode) {
      case DDCA_IO_I2C:
      if (path1.path.i2c_busno == path2.path.i2c_busno)
         result = true;
      break;
      case DDCA_IO_ADL:
      if (path1.path.adlno.iAdapterIndex == path2.path.adlno.iAdapterIndex &&
          path1.path.adlno.iDisplayIndex == path2.path.adlno.iDisplayIndex )
         result = true;
      break;
      case DDCA_IO_USB:
      if (path1.path.hiddev_devno == path2.path.hiddev_devno)
         result = true;
      break;
      }
   }
   return result;
}
#endif


static bool display_desc_matches(Distinct_Display_Desc * ddesc, Display_Ref * dref) {
   bool result = false;
   if (dpath_eq(ddesc->io_path, dref->io_path))
      result = true;
#ifdef TOO_MANY_EDGE_CASES
   else {
      if (dref->pedid) {
      if (streq(dref->pedid->mfg_id,       ddesc->edid_mfg)        &&
          streq(dref->pedid->model_name,   ddesc->edid_model_name) &&
          streq(dref->pedid->serial_ascii, ddesc->edid_serial_ascii)
         )
             result = true;
      }
      else {
         // pathological case, should be impossible but user report
         // re X260 laptop and Ultradock indicates possible (see note in ddc_open_display()
         DBGMSG("Null EDID");
      }
   }
#endif
   return result;
}


static GPtrArray * display_descriptors = NULL;  // array of Distinct_Display_Desc *
static GMutex descriptors_mutex;                // single threads access to display_descriptors
static GMutex master_display_lock_mutex;


// must be called when lock not held by current thread, o.w. deadlock
static char * distinct_display_ref_repr_t(Distinct_Display_Ref id) {
   static GPrivate  repr_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&repr_key, 100);
   g_mutex_lock(&descriptors_mutex);
   Distinct_Display_Desc * ref = (Distinct_Display_Desc *) id;
   assert(memcmp(ref->marker, DISTINCT_DISPLAY_DESC_MARKER, 4) == 0);
   g_snprintf(buf, 100, "Distinct_Display_Ref[%s]", dpath_repr_t(&ref->io_path));
   g_mutex_unlock(&descriptors_mutex);
   return buf;
}


Distinct_Display_Ref get_distinct_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGMSF(debug, "Starting. dref=%s", dref_repr_t(dref));

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
#ifdef TOO_MANY_EDGE_CASES
      new_desc->edid_mfg          = strdup(dref->pedid->mfg_id);
      new_desc->edid_model_name   = strdup(dref->pedid->model_name);
      new_desc->edid_serial_ascii = strdup(dref->pedid->serial_ascii);
#endif
      g_mutex_init(&new_desc->display_mutex);
      g_ptr_array_add(display_descriptors, new_desc);
      result = new_desc;
   }

   g_mutex_unlock(&descriptors_mutex);

   DBGMSF(debug, "Done.     Returning: %p -> %s", result,  distinct_display_ref_repr_t(result));
   return result;
}


/** Locks a distinct display.
 *
 *  \param  id                 distinct display identifier
 *  \param  flags              if **DDISP_WAIT** set, wait for locking
 *  \retval DDCRC_OK           success
 *  \retval DDCRC_LOCKED       locking failed, display already locked by another
 *                             thread and DDISP_WAIT not set
 *  \retval DDCRC_ALREADY_OPEN display already locked in current thread
 */
DDCA_Status
lock_distinct_display(
      Distinct_Display_Ref   id,
      Distinct_Display_Flags flags)
{
   DDCA_Status ddcrc = 0;
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. id=%p -> %s", id, distinct_display_ref_repr_t(id));

   Distinct_Display_Desc * ddesc = (Distinct_Display_Desc *) id;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(ddesc->marker, DISTINCT_DISPLAY_DESC_MARKER, 4) == 0);
   bool self_thread = false;
   g_mutex_lock(&master_display_lock_mutex);  //wrong - will hold lock during wait
   if (ddesc->display_mutex_thread == g_thread_self() )
      self_thread = true;
   g_mutex_unlock(&master_display_lock_mutex);
   if (self_thread) {
      DBGMSG("Attempting to lock display already locked by current thread");
      ddcrc = DDCRC_ALREADY_OPEN;    // poor
   }
   else {
      bool locked = true;
      if (flags & DDISP_WAIT) {
         g_mutex_lock(&ddesc->display_mutex);
      }
      else {
         locked = g_mutex_trylock(&ddesc->display_mutex);
         if (!locked)
            ddcrc = DDCRC_LOCKED;
      }
      if (locked)
         ddesc->display_mutex_thread = g_thread_self();
   }
   // need a new DDC status code
   DBGTRC(debug, TRACE_GROUP, "Done.     id=%p -> %s, Returning: %s",
                 id, distinct_display_ref_repr_t(id), psc_desc(ddcrc));
   return ddcrc;
}


/** Unlocks a distinct display.
 *
 *  \param id  distinct display identifier
 *  \retval DDCRC_LOCKED attempting to unlock a display owned by a different thread
 *  \retval DDCRC_OK
 */
DDCA_Status unlock_distinct_display(Distinct_Display_Ref id) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. id=%p -> %s", id, distinct_display_ref_repr_t(id));
   DDCA_Status ddcrc = 0;
   Distinct_Display_Desc * ddesc = (Distinct_Display_Desc *) id;
   // TODO:  If this function is exposed in API, change assert to returning illegal argument status code
   TRACED_ASSERT(memcmp(ddesc->marker, DISTINCT_DISPLAY_DESC_MARKER, 4) == 0);
   g_mutex_lock(&master_display_lock_mutex);
   if (ddesc->display_mutex_thread != g_thread_self()) {
      DBGMSG("Attempting to unlock display lock owned by different thread");
      ddcrc = DDCRC_LOCKED;
   }
   else {
      ddesc->display_mutex_thread = NULL;
      g_mutex_unlock(&ddesc->display_mutex);
   }
   g_mutex_unlock(&master_display_lock_mutex);
   DBGTRC(debug, TRACE_GROUP, "Done.     id=%p -> %s, Returning %s",
                 id, distinct_display_ref_repr_t(id), psc_desc(ddcrc));
   return ddcrc;
}


/** Unlocks all distinct displays.
 *
 *  The function is used during reinitialization.
 */
void unlock_all_distinct_displays() {
   bool debug = true;
   DBGTRC(debug, TRACE_GROUP, "Starting");
   g_mutex_lock(&master_display_lock_mutex);   // are both locks needed?
   g_mutex_lock(&descriptors_mutex);
    for (int ndx=0; ndx < display_descriptors->len; ndx++) {
       Distinct_Display_Desc * cur = g_ptr_array_index(display_descriptors, ndx);
       DBGTRC(debug, TRACE_GROUP, "%2d - %p  %-28s",
                        ndx, cur,
                        dpath_repr_t(&cur->io_path) );

       g_mutex_unlock(&cur->display_mutex);
    }
    g_mutex_unlock(&descriptors_mutex);
    g_mutex_unlock(&master_display_lock_mutex);
    DBGTRC(debug, TRACE_GROUP, "Done");
 }


/** Emits a report of all distinct display descriptors.
 *
 *  \param depth logical indentation depth
 */
void dbgrpt_distinct_display_descriptors(int depth) {
   rpt_vstring(depth, "display_descriptors@%p", display_descriptors);
   g_mutex_lock(&descriptors_mutex);
   int d1 = depth+1;
   for (int ndx=0; ndx < display_descriptors->len; ndx++) {
      Distinct_Display_Desc * cur = g_ptr_array_index(display_descriptors, ndx);
#ifdef TOO_MANY_EDGE_CASES
      rpt_vstring(d1, "%2d - %p  %-28s - %-4s %-13s %-13s",
                       ndx, cur,
                       dpath_repr_t(&cur->io_path),
                       cur->edid_mfg,
                       cur->edid_model_name,
                       cur->edid_serial_ascii);
#endif
      rpt_vstring(d1, "%2d - %p  %-28s",
                       ndx, cur,
                       dpath_repr_t(&cur->io_path) );
   }
   g_mutex_unlock(&descriptors_mutex);
}


/** Initializes this module */
void init_ddc_display_lock(void) {
   display_descriptors= g_ptr_array_new();

   RTTI_ADD_FUNC(get_distinct_display_ref);
   RTTI_ADD_FUNC(lock_distinct_display);
   RTTI_ADD_FUNC(unlock_distinct_display);
}
