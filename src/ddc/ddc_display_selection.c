/** @file ddc_display_selection.c */

// Copyright (C) 2022-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/edid.h"
#include "util/report_util.h"
#include "util/string_util.h"
#ifdef ENABLE_UDEV
#include "util/udev_usb_util.h"
#include "util/udev_util.h"
#endif
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/rtti.h"

#ifdef ENABLE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_displays.h"

#include "ddc/ddc_display_selection.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;


//
// Display Selection
//

/** Display selection criteria */
typedef struct {
   int     dispno;
   int     i2c_busno;
   int     hiddev;
   int     usb_busno;
   int     usb_devno;
   char *  mfg_id;
   char *  model_name;
   char *  serial_ascii;
   Byte *  edidbytes;
} Display_Criteria;


/** Allocates a new #Display_Criteria and initializes it to contain no criteria.
 *
 * @return initialized #Display_Criteria
 */
static Display_Criteria *
new_display_criteria() {
   bool debug = false;
   DBGMSF(debug, "Starting");
   Display_Criteria * criteria = calloc(1, sizeof(Display_Criteria));
   criteria->dispno = -1;
   criteria->i2c_busno  = -1;
   criteria->hiddev = -1;
   criteria->usb_busno = -1;
   criteria->usb_devno = -1;
   DBGMSF(debug, "Done.    Returning: %p", criteria);
   return criteria;
}


/** Checks if a given #Display_Ref satisfies all the criteria specified in a
 *  #Display_Criteria struct.
 *
 *  @param  drec     pointer to #Display_Ref to test
 *  @param  criteria pointer to criteria
 *  @retval true     all specified criteria match
 *  @retval false    at least one specified criterion does not match
 *
 *  @remark
 *  In the degenerate case that no criteria are set in **criteria**, returns true.
 */
static bool
ddc_test_display_ref_criteria(Display_Ref * dref, Display_Criteria * criteria) {
   TRACED_ASSERT(dref && criteria);
   bool result = false;

   if (criteria->dispno >= 0 && criteria->dispno != dref->dispno)
      goto bye;

   if (criteria->i2c_busno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_I2C || dref->io_path.path.i2c_busno != criteria->i2c_busno)
         goto bye;
   }

#ifdef ENABLE_USB
   if (criteria->hiddev >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      char buf[40];
      snprintf(buf, 40, "%s/hiddev%d", usb_hiddev_directory(), criteria->hiddev);
      Usb_Monitor_Info * moninfo = dref->detail;
      TRACED_ASSERT(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      if (!streq( moninfo->hiddev_device_name, buf))
         goto bye;
   }

   if (criteria->usb_busno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      // Usb_Monitor_Info * moninfo = drec->detail2;
      // assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      // if ( moninfo->hiddev_devinfo->busnum != criteria->usb_busno )
      if ( dref->usb_bus != criteria->usb_busno )
         goto bye;
   }

   if (criteria->usb_devno >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      // Usb_Monitor_Info * moninfo = drec->detail2;
      // assert(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
      // if ( moninfo->hiddev_devinfo->devnum != criteria->usb_devno )
      if ( dref->usb_device != criteria->usb_devno )
         goto bye;
   }

   if (criteria->hiddev >= 0) {
      if (dref->io_path.io_mode != DDCA_IO_USB)
         goto bye;
      if ( dref->io_path.path.hiddev_devno != criteria->hiddev )
         goto bye;
   }
#endif

   if (criteria->mfg_id && (strlen(criteria->mfg_id) > 0) &&
         !streq(dref->pedid->mfg_id, criteria->mfg_id) )
      goto bye;

   if (criteria->model_name && (strlen(criteria->model_name) > 0) &&
         !streq(dref->pedid->model_name, criteria->model_name) )
      goto bye;

   if (criteria->serial_ascii && (strlen(criteria->serial_ascii) > 0) &&
         !streq(dref->pedid->serial_ascii, criteria->serial_ascii) )
      goto bye;

   if (criteria->edidbytes && memcmp(dref->pedid->bytes, criteria->edidbytes, 128) != 0)
      goto bye;

   result = true;

bye:
   return result;
}


static Display_Ref *
ddc_find_display_ref_by_criteria(Display_Criteria * criteria) {
   Display_Ref * result = NULL;
   GPtrArray * all_displays = ddc_get_all_display_refs();
   for (int ndx = 0; ndx < all_displays->len; ndx++) {
      Display_Ref * drec = g_ptr_array_index(all_displays, ndx);
      TRACED_ASSERT(memcmp(drec->marker, DISPLAY_REF_MARKER, 4) == 0);
      if (ddc_test_display_ref_criteria(drec, criteria)) {
         result = drec;
         break;
      }
   }
   return result;
}


/** Searches the master display list for a display matching the
 *  specified #Display_Identifier, returning its #Display_Ref
 *
 *  @param did display identifier to search for
 *  @return #Display_Ref for the display, NULL if not found or
 *          display doesn't support DDC
 *
 * @remark
 * The returned value is a pointer into an internal data structure
 * and should not be freed by the caller.
 */
static Display_Ref *
ddc_find_display_ref_by_display_identifier(Display_Identifier * did) {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. did=%s", did_repr(did));
   if (debug)
      dbgrpt_display_identifier(did, 1);

   Display_Ref * result = NULL;

   Display_Criteria * criteria = new_display_criteria();

   switch(did->id_type) {
   case DISP_ID_BUSNO:
      criteria->i2c_busno = did->busno;
      break;
   case DISP_ID_MONSER:
      criteria->mfg_id = did->mfg_id;
      criteria->model_name = did->model_name;
      criteria->serial_ascii = did->serial_ascii;
      break;
   case DISP_ID_EDID:
      criteria->edidbytes = did->edidbytes;
      break;
   case DISP_ID_DISPNO:
      criteria->dispno = did->dispno;
      break;
   case DISP_ID_USB:
      criteria->usb_busno = did->usb_bus;
      criteria->usb_devno = did->usb_device;
      break;
   case DISP_ID_HIDDEV:
      criteria->hiddev = did->hiddev_devno;
   }

   result = ddc_find_display_ref_by_criteria(criteria);

   // Is this the best location in the call chain to make this check?
   if (result && (result->dispno < 0)) {
      DBGMSF(debug, "Found a display that doesn't support DDC.  Ignoring.");
      result = NULL;
   }

   free(criteria);   // do not free pointers in criteria, they are owned by Display_Identifier

   DBGTRC_RETURNING(debug, DDCA_TRC_NONE, dref_repr_t(result), "");
   return result;
}


/** Searches the detected displays for one matching the criteria in a
 *  #Display_Identifier.
 *
 *  @param pdid  pointer to a #Display_Identifier
 *  @param callopts  standard call options
 *  @return pointer to #Display_Ref for the display, NULL if not found
 *
 *  \todo
 *  If the criteria directly specify an access path
 *  (e.g. I2C bus number) and CALLOPT_FORCE specified, then create a
 *  temporary #Display_Ref, bypassing the list of detected monitors.
 */
Display_Ref *
get_display_ref_for_display_identifier(
                Display_Identifier* pdid,
                Call_Options        callopts)
{
   Display_Ref * dref = ddc_find_display_ref_by_display_identifier(pdid);

   return dref;
}


void
init_ddc_display_selection() {
   RTTI_ADD_FUNC(ddc_find_display_ref_by_display_identifier);
}

