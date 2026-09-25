/** @file ddc_open_close.c
 *  Opening and closing a display at the DDC level
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "base/parms.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/** \endcond */

#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

#include "util/edid.h"
#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/display_lock.h"
#include "base/displays.h"
#include "base/execution_stats.h"
#include "base/i2c_bus_base.h"
#include "base/per_display_data.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"

#include "sysfs/sysfs_base.h"
#include "sysfs/sysfs_dpms.h"
#include "sysfs/sysfs_simple.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_bus_open_close.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_displays.h"

#include "ddc_open_close.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDCIO;

// Display handles currently open, in any thread.  Written under the mutex by
// ddc_open_display() and ddc_close_display(), read by the validation and
// report functions below.
static GHashTable * open_displays = NULL;
static GMutex open_displays_mutex;


#ifdef DEPRECATED
bool
ddc_is_valid_display_handle(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%p", dh);
   assert(open_displays);
   g_mutex_lock (&open_displays_mutex);
   bool result = g_hash_table_contains(open_displays, dh);
   g_mutex_unlock(&open_displays_mutex);
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "dh=%s", dh_repr(dh));
   return result;
}
#endif

#ifdef OLD
DDCA_Status
ddc_validate_display_handle(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%p", dh);
   assert(open_displays);

   // DDCA_Status result = ddc_validate_display_ref(dh->dref, /*basic_only*/ false, /*test_asleep*/ true);
   DDCA_Status result = ddc_validate_display_ref2(dh->dref, DREF_VALIDATE_EDID|DREF_VALIDATE_AWAKE);
   if (result == DDCRC_OK) {
      g_mutex_lock (&open_displays_mutex);
      if (!g_hash_table_contains(open_displays, dh) )
         result = DDCRC_ARG;
      g_mutex_unlock(&open_displays_mutex);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "dh=%s", dh_repr(dh));
   return result;
}
#endif

DDCA_Status
ddc_validate_display_handle2(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%p", dh);
   assert(open_displays);

   DDCA_Status result = DDCRC_OK;
   // DDCA_Status result = ddc_validate_display_ref2(dh->dref,  DREF_VALIDATE_EDID|DREF_VALIDATE_AWAKE);
   // DDCA_Status result = ddc_validate_display_ref2(dh->dref,  DREF_VALIDATE_BASIC_ONLY);
   if (dh->dref->disconnected) {
      result = DDCRC_DISCONNECTED;
   }

   if (result == DDCRC_OK) {
      g_mutex_lock (&open_displays_mutex);
      if (!g_hash_table_contains(open_displays, dh) )
         result = DDCRC_ARG;
      g_mutex_unlock(&open_displays_mutex);
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, result, "dh=%s", dh_repr(dh));
   return result;
}


void ddc_dbgrpt_valid_display_handles(int depth) {
   rpt_vstring(depth, "Valid display handle = open_displays:");
   assert(open_displays);
   g_mutex_lock (&open_displays_mutex);
   GList * display_handles = g_hash_table_get_keys(open_displays);
   if (g_list_length(display_handles) > 0) {
      for (GList * cur = display_handles; cur; cur = cur->next) {
         Display_Handle * dh = cur->data;
         rpt_vstring(depth+1, "%p -> %s", dh, dh_repr(dh));
      }
   }
   else {
      rpt_vstring(depth+1, "None");
   }
   g_list_free(display_handles);
   g_mutex_unlock(&open_displays_mutex);
}

#ifdef OUT
// TODO: generalize, move to more appropriate location
static bool is_drm_conformant_driver(const char * driver_name) {
   return streq(driver_name, "amdgpu") || streq(driver_name, "i915");
}
#endif


//
// Open/Close Display
//

__thread GPtrArray * open_displays_for_thread;


bool add_open_display_for_current_thread(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "open_displays_for_thread=%p, dh=%s",
         open_displays_for_thread, dh_repr_p(dh));

   bool found = false;
   if (!open_displays_for_thread)
      open_displays_for_thread = g_ptr_array_new();
   else
      found = g_ptr_array_find(open_displays_for_thread, dh, NULL);
   if (!found) {
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "open_displays_for_thread=%p, dh=%p", open_displays_for_thread, dh);
      g_ptr_array_add(open_displays_for_thread, dh);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, !found, "dh=%s", dh_repr_p(dh));
   return !found;
}


bool remove_open_display_for_current_thread(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr_p(dh));

   bool found = false;
   if (open_displays_for_thread) {
      found = g_ptr_array_remove(open_displays_for_thread, dh);
      if (open_displays_for_thread->len == 0) {
         g_ptr_array_free(open_displays_for_thread, true);
         open_displays_for_thread = NULL;
      }
   }


   DBGTRC_RET_BOOL(debug, TRACE_GROUP, found, "dh=%s", dh_repr_p(dh));
   return found;
}


#ifdef UNUSED
bool in_ddci_open_display() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "");
   bool found = false;
   GPtrArray* callers = get_current_traced_function_stack_contents(true);
   for (int ndx = 0; ndx < callers->len; ndx++) {
      char * cur = g_ptr_array_index(callers, ndx);
      // rpt_vstring(0, "cur=|%s|", cur);
      // drpt_vstring(0, "cur=|%s|", cur);
      if (streq(cur, "ddci_open_display3")) {
         found = true;
         // drpt_vstring(0, "FOUND");
         break;
      }
   }
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, found, "");
   return found;
}
#endif


/** Opens a DDC display.
 *
 *  \param  dref            display reference
 *  \param  callopts        call option flags
 *  \param  dh_loc          address at which to return display handle
 *  \return Error_Info      if error, with status
 *                            status code from  #i2c_open_bus(), #usb_open_hiddev_device()
 *                          DDCRC_LOCKED    display open in another thread
 *                          DDCRC_ALREADY_OPEN display already open in current thread
 *                          DDCRC_DISCONNECTED display has been disconnected
 *
 *  **Call_Option** flags recognized:
 *  - CALLOPT_WAIT
 */
Error_Info *
ddc_open_display(
      Display_Ref *    dref,
      Call_Options     callopts,
      Display_Handle** dh_loc)
{
   bool debug = false;
   assert(dref);
   // static int ctr = 0;
   // ctr++;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dref=%s, callopts=%s, dref->detail=%p, dh_loc=%p",
                      dref_reprx_t(dref), interpret_call_options_t(callopts),dref->detail, dh_loc);
   TRACED_ASSERT(dh_loc);
   // TRACED_ASSERT(1==5);    // for testing

   Display_Handle * dh = NULL;
   Error_Info * err = NULL;
   int fd = -1;

   g_mutex_lock (&dref->disconnect_mutex);
   // if (ctr % 8 == 0)
   //    dref->detail = NULL;
   if (dref->disconnected) {
      char * s = g_strdup_printf("Attempting to open disconnected display reference %s",
            dref_repr_t(dref));
      DUAL_MSGNV(debug, DDCA_SYSLOG_ERROR, "%s", s);
      err = ERRINFO_NEW(DDCRC_DISCONNECTED, "%s", s);
      free(s);
      goto bye;
   }

   // make copy here because dref->detail can for some reason become NULL after this point
   void * dref_detail = dref->detail;   // ignore possibility of USB detail
   if (!dref_detail) {
      char * s = g_strdup_printf( "Display_Ref.detail == NULL, but DREF_DISCONNECTED not set, dref=%s",
            dref_repr_t(dref));
      DUAL_MSGNV(debug, DDCA_SYSLOG_ERROR,"%s", s);
      dbgrpt_current_traced_function_stack(true, true, 1);
      TRACED_FUNCTION_STACK_TO_SYSLOG(DDCA_SYSLOG_ERROR, TFS_MOST_RECENT_FIRST);
      // mark_display_ref_disconnected(dref);  // don't call - double lock
      dref->disconnected = true;
      dref->detail = NULL;
      err = ERRINFO_NEW(DDCRC_DISCONNECTED, "%s", s);
      free(s);
      goto bye;
   }

   const char * driver_name = dref_get_i2c_driver(dref);
   DBGTRC_NOPREFIX(false, DDCA_TRC_NONE, "driver_name: %s", driver_name);
   if (driver_name && is_sysfs_reliable_for_driver(driver_name) &&
       dref->drm_connector && strlen(dref->drm_connector) > 0)
   {
      POSSIBLY_WRITE_DETECT_TO_STATUS_BY_DREF(dref);
      char * status;
      int tryct = 0;
   retry_status:
      RPT_ATTR_TEXT(-1, &status, "/sys/class/drm", dref->drm_connector, "status");
      if (streq(status, "disconnected")) {
         if (tryct == 0) {
            free(status);
            // DBGTRC_NOPREFIX(debug, TRACE_GROUP, "status == disconnected, sleeping 1 sec and retrying");
            SLEEP_MILLIS_WITH_SYSLOG(1000, "Delay before rechecking attribute status");
            tryct++;
            goto retry_status;
         }
         DUAL_MSGXV(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP,
               "%s still disconnected after 1 second delay and retry", dref_reprx_t(dref));
         err = ERRINFO_NEW(DDCRC_DISCONNECTED, "Display disconnected");
      }
      free(status);
#ifdef MADE_UNECESSARY_BY_MUTEX
      // In case dw_remove_display_by_businfo() called during the one second window,
      // per Charistian Gudrian
      if (dref->flags & DREF_DISCONNECTED)
         err = ERRINFO_NEW(DDCRC_DISCONNECTED, "Display disconnected");
#endif
      if (err)
         goto bye;
   }

#ifdef NO
    Display_Lock_Flags ddisp_flags = DDISP_NONE;
   if (callopts & CALLOPT_WAIT)
      ddisp_flags |= DDISP_WAIT;

   err = lock_display_by_dref(dref, ddisp_flags);
   if (err)
      goto bye;
#endif

   switch (dref->io_path.io_mode) {

   case DDCA_IO_I2C:
      {
         I2C_Bus_Info * businfo = dref_detail;

         // Issue #556, powerdevil bug report, says that businfo == NULL,
         // which is logically impossible at this point.
         // Perhaps it was actually the memcmp() on the next line that failed.
         // Lacking further detail in the bug report for proper diagnosis,
         // all we can do at this point is return an internal error.

         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "businfo=%p", businfo);
         bool reported = false;
// #define DEBUG_556
#ifdef DEBUG_556
         if (in_ddci_open_display()) {
            dbgrpt_current_traced_function_stack(true, false, 0);
            dbgrpt_published_dref_hash("In ddc_open_display (1)", 0);
            reported = true;
         }
#endif

         TRACED_ASSERT(businfo);
         if (memcmp(businfo, I2C_BUS_INFO_MARKER, 4) != 0) {
            if (!reported) {
               dbgrpt_current_traced_function_stack(true, false, 0);
               dbgrpt_published_dref_hash("In ddc_open_display (2)", 0);
            }
            char * msg = g_strdup_printf("dref=%s, businfo->marker = |%.4s| = %s",
                      dref_reprx_t(dref), (char*)businfo, hexstring_t((unsigned char*) businfo->marker, 4));
            MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s", msg);
            TRACED_FUNCTION_STACK_TO_SYSLOG(DDCA_SYSLOG_ERROR, TFS_MOST_RECENT_LAST);
            published_dref_hash_to_syslog(DDCA_SYSLOG_ERROR, "In ddc_open_display() (3)");

#define RECOVER_556
#ifndef RECOVER_556
            free(msg);
            TRACED_ASSERT(memcmp(businfo, I2C_BUS_INFO_MARKER, 4) == 0);
#else
            err = ERRINFO_NEW(DDCRC_INTERNAL_ERROR, "%s", msg);
            free(msg);
            goto bye;
#endif
         }

         if (!businfo->edid) {
            // How is this even possible?
            // 1/2017:  Observed with x260 laptop and Ultradock, See ddcutil user report.
            //          close(fd) fails
            char * msg = g_strdup_printf("No EDID for device on bus /dev/"I2C"-%d",
                                          dref->io_path.path.i2c_busno);
            MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s", msg);
            err = ERRINFO_NEW(DDCRC_EDID, "%s", msg);
            free(msg);
         }

         if (!err) {
            DBGMSF(debug, "Calling i2c_open_bus() ...");
            Error_Info * err2 = i2c_open_bus(dref->io_path.path.i2c_busno, callopts, &fd);
            ASSERT_IFF(err2, fd == -1);
            if (err2) {
               err = errinfo_new_with_cause(err2->status_code, err2, __func__,
                               "Opening /dev/i2c-%d", dref->io_path.path.i2c_busno);
            }
         }
         if (!err) {
            dh = create_base_display_handle(fd, dref);
            if (!dref->pedid)
               dref->pedid = copy_parsed_edid(businfo->edid);
            if (!dref->pdd)
               dref->pdd = pdd_get_per_display_data(dref->io_path, true);
         }
      }
      break;

   case DDCA_IO_USB:
#ifdef ENABLE_USB
      {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Opening USB device: %s", dref->usb_hiddev_name);
         TRACED_ASSERT(dref && dref->usb_hiddev_name);
         // if (!dref->usb_hiddev_name) { // HACK
         //    DBGMSG("HACK FIXUP.  dref->usb_hiddev_name");
         //    dref->usb_hiddev_name = get_hiddev_devname_by_dref(dref);
         // }
         fd = usb_open_hiddev_device(dref->usb_hiddev_name, callopts);
         if (fd < 0) {
            err = ERRINFO_NEW(fd, "Error opening %s", dref->usb_hiddev_name);
         }
         else {
            dh = create_base_display_handle(fd, dref);
            if (!dref->pedid)
               dref->pedid = copy_parsed_edid(usb_get_parsed_edid_by_dh(dh));
            if (!dref->pdd)
               dref->pdd = pdd_get_per_display_data(dref->io_path, true);
         }
      }
#else
      PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
      assert(false);   // avoid coverity error re null dreference
#endif
      break;
   } // switch
   ASSERT_IFF(!err, dh);
   if (!err) {
      assert(dh->dref->pedid);
      dref->flags |= DREF_OPEN;
      TRACED_ASSERT(open_displays);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Adding dh=%s to open_displays hash table", dh_repr_p(dh));
      g_mutex_lock (&open_displays_mutex);
      g_hash_table_add(open_displays, dh);
      g_mutex_unlock(&open_displays_mutex);
   }
   else {
#ifdef NO
      Error_Info * err2 = unlock_display_by_dref(dref);
      if (err2) {
         PROGRAM_LOGIC_ERROR("unlock_distinct_display() returned %s", errinfo_summary(err));
         errinfo_free(err2);
      }
#endif
   }

bye:
   if (err) {
      COUNT_STATUS_CODE(err->status_code);
   }
   else {
      add_open_display_for_current_thread(dh);
   }
   *dh_loc = dh;
   TRACED_ASSERT_IFF( !err, *dh_loc );
   g_mutex_unlock (&dref->disconnect_mutex);
   // dbgrpt_distinct_display_descriptors(0);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "*dh_loc=%s", dh_repr_p(*dh_loc));
   return err;
}


/** Closes a DDC display.
 *
 *  @param  dh  display handle
 *  @return NULL if no error, #Error_Info struct if error
 *
 *  @remark
 *  Logs underlying status code if error.
 */
Error_Info *
ddc_close_display(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, dref=%s, fd=%d, dpath=%s",
              dh_repr_p(dh), dref_repr_t(dh->dref), dh->fd, dpath_short_name_t(&dh->dref->io_path));
   Display_Ref * dref = dh->dref;
   Error_Info * err = NULL;
   Status_Errno rc = 0;
   if (dh->fd == -1) {
      rc = DDCRC_INVALID_OPERATION;    // or DDCRC_ARG?
      err = ERRINFO_NEW(rc, "Invalid display handle");
   }
   else {
      switch(dh->dref->io_path.io_mode) {
      case DDCA_IO_I2C:
         {
            DBGMSF(debug, "Calling is2_close_bus() ...");
            rc = i2c_close_bus(dh->dref->io_path.path.i2c_busno, dh->fd, CALLOPT_NONE);
            if (rc != 0) {
               TRACED_ASSERT(rc < 0);
               char * msg = g_strdup_printf("i2c_close_bus returned %d, errno=%s",
                                            rc, psc_desc(errno) );
               DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "%s", msg);
               err = ERRINFO_NEW(rc, msg);
               free(msg);
               COUNT_STATUS_CODE(rc);
            }
            dh->fd = -1;    // indicate invalid, in case we try to continue using dh
            break;
         }
      case DDCA_IO_USB:
#ifdef ENABLE_USB
         {
            rc = usb_close_device(dh->fd, dh->dref->usb_hiddev_name, CALLOPT_NONE);
            if (rc != 0) {
               TRACED_ASSERT(rc < 0);
               char * msg = g_strdup_printf("usb_close_bus returned %d, errno=%s",
                                            rc, psc_desc(errno) );
               MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s", msg);
               err = ERRINFO_NEW(rc, "%s", msg);
               free(msg);
               COUNT_STATUS_CODE(rc);
            }
            dh->fd = -1;
            break;
         }
#else
         PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      } //switch
   }

   dh->dref->flags &= (~DREF_OPEN);
#ifdef NO
   Error_Info * err2 = unlock_display_by_dref(dref);
   if (err2) {
      DECORATED_SYSLOG(DDCA_SYSLOG_ERROR, "%s", err2->detail);
      if (!err)
         err = err2;
      else
         BASE_ERRINFO_FREE_WITH_REPORT(err2, true);
   }
#endif
   assert(open_displays);
   g_mutex_lock (&open_displays_mutex);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Removing dh=%s from open_displays hash table of size %d",
         dh_repr_p(dh), g_hash_table_size(open_displays) );
   g_hash_table_remove(open_displays, dh);
   g_mutex_unlock (&open_displays_mutex);
   remove_open_display_for_current_thread(dh);

   free_display_handle(dh);
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, err, "dref=%s", dref_repr_t(dref));
   return err;
}


// Handles common case where the return value of ddc_close_display is ignored
void ddc_close_display_wo_return(Display_Handle * dh) {
   Error_Info * err = ddc_close_display(dh);
   if (err) {
      MSG_W_SYSLOG(DDCA_SYSLOG_ERROR, "%s: %s", err->detail, psc_desc(err->status_code));
      ERRINFO_FREE_WITH_REPORT(err, true);
   }
}


/** Closes all open displays, ignoring any errors */
void ddc_close_all_displays() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   assert(open_displays);
   // ddc_dbgrpt_valid_display_handles(2);
   DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Closing %d open displays",g_hash_table_size(open_displays));
   GList * display_handles = g_hash_table_get_keys(open_displays);
   for (GList * cur = display_handles; cur; cur = cur->next) {
      Display_Handle * dh = cur->data;
      ddc_close_display_wo_return(dh);
   }
   g_free(display_handles);
   // open_displays should be empty at this point
   TRACED_ASSERT(g_hash_table_size(open_displays) == 0);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void ddc_close_all_displays_for_current_thread(bool error_if_open) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");

   int closed_ct = 0;
   if (open_displays_for_thread) {
      for (int ndx = 0; ndx < open_displays_for_thread->len; ndx++) {
         Display_Handle * dh = g_ptr_array_index(open_displays_for_thread, ndx);
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Closing %s...", dh_repr_p(dh));
         if (error_if_open) {
            DECORATED_SYSLOG(DDCA_SYSLOG_ERROR,"Closing %s that should not be open", dh_repr_p(dh) );
         }
         closed_ct++;
         ddc_close_display_wo_return(dh);
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Closed %d open display handles", closed_ct);
}


void init_ddc_open_close() {
   RTTI_ADD_FUNC(ddc_open_display);
   RTTI_ADD_FUNC(ddc_close_display);
   RTTI_ADD_FUNC(ddc_validate_display_handle2);
   RTTI_ADD_FUNC(add_open_display_for_current_thread);
   RTTI_ADD_FUNC(remove_open_display_for_current_thread);
   RTTI_ADD_FUNC(ddc_close_all_displays_for_current_thread);

   open_displays = g_hash_table_new(g_direct_hash, NULL);
}


void terminate_ddc_open_close() {
   g_hash_table_destroy(open_displays);
}
