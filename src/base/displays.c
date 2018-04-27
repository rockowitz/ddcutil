/* displays.c
 *
 * Maintains list of all detected monitors.
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 * Monitor identifier, reference, handle
 */

#include <config.h>

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/glib_util.h"
#include "util/string_util.h"
#include "util/report_util.h"
#include "util/udev_util.h"
#include "util/udev_usb_util.h"
/** \endcond */

#include "public/ddcutil_c_api.h"

#include "core.h"
#include "vcp_version.h"

#include "displays.h"


#ifdef NOT_NEEDED
typedef struct {
   char * s_did;
} Thread_Displays_Data;

static Thread_Displays_Data *  get_thread_displays_data() {
   static GPrivate per_thread_data_key = G_PRIVATE_INIT(g_free);

   Thread_Displays_Data *thread_data = g_private_get(&per_thread_data_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, settings=%p\n", __func__, this_thread, settings);

   if (!thread_data) {
      thread_data = g_new0(Thread_Displays_Data, 1);
      g_private_set(&per_thread_data_key, thread_data);
   }

   // printf("(%s) Returning: %p\n", __func__, thread_data);
   return thread_data;
}
#endif


// *** Miscellaneous ***

/** Reports whether a #DDCA_Adlno value is set or is currently undefined.
 *  \param adlno ADL adapter/index number pair
 *
 *  \remark
 *  Used to hide the magic number for "undefined"
 */
bool is_adlno_defined(DDCA_Adlno adlno) {
   return adlno.iAdapterIndex >= 0 && adlno.iDisplayIndex >= 0;
}


// *** DDCA_IO_Path ***

/** Tests 2 #DDCA_IO_Path instances for equality
 *
 *  \param p1  first instance
 *  \param p2  second instance
 *  \return  true/false
 */
bool dpath_eq(DDCA_IO_Path p1, DDCA_IO_Path p2) {
   bool result = false;
   if (p1.io_mode == p2.io_mode) {
      switch(p1.io_mode) {
      case DDCA_IO_I2C:
         result = (p1.path.i2c_busno == p2.path.i2c_busno);
         break;
      case DDCA_IO_ADL:
         result = (p1.path.adlno.iAdapterIndex == p2.path.adlno.iAdapterIndex) &&
                  (p1.path.adlno.iDisplayIndex == p2.path.adlno.iDisplayIndex);
         break;
      case DDCA_IO_USB:
         result = p1.path.hiddev_devno == p2.path.hiddev_devno;
      }
   }
   return result;
}


// *** Display_Async_Rec ***

// At least temporarily for development, base all async operations for a display
// on this struct.


static GMutex displays_master_list_mutex;
static GPtrArray * displays_master_list = NULL;  // only handful of displays, simple data structure suffices


void dbgrpt_displays_master_list(GPtrArray* displays_master_list, int depth) {
   int d1 = depth+1;

   rpt_structure_loc("displays_master_list", displays_master_list, depth);
   if (displays_master_list) {
      for (int ndx = 0; ndx < displays_master_list->len; ndx++) {
          Display_Async_Rec * cur = g_ptr_array_index(displays_master_list, ndx);
          // DBGMSG("%p", cur);
          rpt_vstring(d1, "%p - %s", cur, dpath_repr_t(&cur->dpath));
      }
   }
}


Display_Async_Rec * display_async_rec_new(DDCA_IO_Path dpath) {
   Display_Async_Rec * newrec = calloc(1, sizeof(Display_Async_Rec));
   memcpy(newrec->marker, DISPLAY_ASYNC_REC_MARKER, 4);
   newrec->dpath = dpath;
   // g_mutex_init(&newrec->thread_lock);
   // newrec->owning_thread = NULL;

   newrec->request_queue = g_queue_new();
   g_mutex_init(&newrec->request_queue_lock);
#ifdef FUTURE
   newrec->request_execution_thread =
         g_thread_new(
               strdup(dpath_repr_t(dref)),       // thread name
               NULL,                             // GThreadFunc    *** TEMP ***
               dref->request_queue);             // or just dref?, how to pass dh?
#endif

   return newrec;
}


Display_Async_Rec * find_display_async_rec(DDCA_IO_Path dpath) {
   bool debug = false;
   assert(displays_master_list);
   Display_Async_Rec * result = NULL;

   for (int ndx = 0; ndx < displays_master_list->len; ndx++) {
      Display_Async_Rec * cur = g_ptr_array_index(displays_master_list, ndx);
      if ( dpath_eq(cur->dpath, dpath) ) {
         result = cur;
         break;
      }
   }

   DBGMSF(debug, "Returning %p", result);
   return result;
}


/** Obtains a reference to the #Display_Async_Rec for a display.
 *
 */
Display_Async_Rec * get_display_async_rec(DDCA_IO_Path dpath) {
   bool debug = false;
   assert(displays_master_list);
   DBGMSF(debug, "dpath=%s", dpath_repr_t(&dpath));
   if (debug)
      dbgrpt_displays_master_list(displays_master_list, 1);

   // This is a simple critical section.  Always wait.
   // G_LOCK(global_locks_mutex);
   g_mutex_lock(&displays_master_list_mutex);

   Display_Async_Rec * result = find_display_async_rec(dpath);
   if (!result) {
      result = display_async_rec_new(dpath);
      // DBGMSG("Adding %p", gdl);
      g_ptr_array_add(displays_master_list, result);
   }
   // G_UNLOCK(global_locks_mutex);
   g_mutex_unlock(&displays_master_list_mutex);
   DBGMSF(debug, "Returning %p", result);
   return result;
}


// GLOCK... macros confuse Eclipse
// GLOCK_DEFINE_STATIC(global_locks_mutex);




/** Acquired at display open time.
 *  Only 1 thread can open
 */
bool lock_display_lock(Display_Async_Rec * async_rec, bool wait) {
   assert(async_rec && memcmp(async_rec->marker, DISPLAY_ASYNC_REC_MARKER, 4) == 0);

   bool lock_acquired = false;

   if (wait) {
      g_mutex_lock(&async_rec->display_lock);
      lock_acquired = true;
   }
   else {
      lock_acquired = g_mutex_trylock(&async_rec->display_lock);
   }
   if (lock_acquired)
      async_rec->thread_owning_display_lock = g_thread_self();

   return lock_acquired;
}


void unlock_display_lock(Display_Async_Rec * async_rec) {
   assert(async_rec && memcmp(async_rec->marker, DISPLAY_ASYNC_REC_MARKER, 4) == 0);

   if (async_rec->thread_owning_display_lock == g_thread_self()) {
      async_rec->thread_owning_display_lock = NULL;
      g_mutex_unlock(&async_rec->display_lock);
   }
}




// *** Display_Identifier ***

static char * Display_Id_Type_Names[] = {
      "DISP_ID_BUSNO",
      "DISP_ID_ADL",
      "DISP_ID_MONSER",
      "DISP_ID_EDID",
      "DISP_ID_DISPNO",
      "DISP_ID_USB",
      "DISP_ID_HIDDEV"
};


/** Returns symbolic name of display identifier type
 * \param val display identifier type
 * \return symbolic name
 */
char * display_id_type_name(Display_Id_Type val) {
   return Display_Id_Type_Names[val];
}


static
Display_Identifier* common_create_display_identifier(Display_Id_Type id_type) {
   Display_Identifier* pIdent = calloc(1, sizeof(Display_Identifier));
   memcpy(pIdent->marker, DISPLAY_IDENTIFIER_MARKER, 4);
   pIdent->id_type = id_type;
   pIdent->busno  = -1;
   pIdent->iAdapterIndex = -1;
   pIdent->iDisplayIndex = -1;
   pIdent->usb_bus = -1;
   pIdent->usb_device = -1;
   memset(pIdent->edidbytes, '\0', 128);
   *pIdent->model_name = '\0';
   *pIdent->serial_ascii = '\0';
   return pIdent;
}

/** Creates a #Display_Identifier using a **ddcutil** display number
 *
 * \param  dispno display number (1 based)
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_dispno_display_identifier(int dispno) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_DISPNO);
   pIdent->dispno = dispno;
   return pIdent;
}


/** Creates a #Display_Identifier using an I2C bus number
 *
 * \param  busno O2C bus number
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_busno_display_identifier(int busno) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_BUSNO);
   pIdent->busno = busno;
   return pIdent;
}


/** Creates a #Display_Identifier using an ADL adapter number/display number pair.
 *
 * \param  iAdapterIndex ADL adapter number
 * \param  iDisplayIndex ADL display number
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_adlno_display_identifier(
      int    iAdapterIndex,
      int    iDisplayIndex)
{
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_ADL);
   pIdent->iAdapterIndex = iAdapterIndex;
   pIdent->iDisplayIndex = iDisplayIndex;
   return pIdent;
}


/** Creates a #Display_Identifier using an EDID value
 *
 * \param  edidbytes  pointer to 128 byte EDID value
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_edid_display_identifier(
      const Byte* edidbytes
      )
{
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_EDID);
   memcpy(pIdent->edidbytes, edidbytes, 128);
   return pIdent;
}

/** Creates a #Display_Identifier using one or more of
 *  manufacturer id, model name, and/or serial number string
 *  as recorded in the EDID.
 *
 * \param  mfg_id       manufacturer id
 * \param  model_name   model name
 * \param  serial_ascii string serial number
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * Unspecified parameters can be either NULL or strings of length 0.
 * \remark
 * At least one parameter must be non-null and have length > 0.
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_mfg_model_sn_display_identifier(
      const char* mfg_id,
      const char* model_name,
      const char* serial_ascii
      )
{
   assert(!mfg_id       || strlen(mfg_id)       < EDID_MFG_ID_FIELD_SIZE);
   assert(!model_name   || strlen(model_name)   < EDID_MODEL_NAME_FIELD_SIZE);
   assert(!serial_ascii || strlen(serial_ascii) < EDID_SERIAL_ASCII_FIELD_SIZE);

   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_MONSER);
   if (mfg_id)
      strcpy(pIdent->mfg_id, mfg_id);
   else
      pIdent->model_name[0] = '\0';
   if (model_name)
      strcpy(pIdent->model_name, model_name);
   else
      pIdent->model_name[0] = '\0';
   if (serial_ascii)
      strcpy(pIdent->serial_ascii, serial_ascii);
   else
      pIdent->serial_ascii[0] = '\0';

   assert( strlen(pIdent->mfg_id) + strlen(pIdent->model_name) + strlen(pIdent->serial_ascii) > 0);

   return pIdent;
}


/** Creates a #Display_Identifier using a USB /dev/usb/hiddevN device number
 *
 * \param  hiddev_devno hiddev device number
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_usb_hiddev_display_identifier(int hiddev_devno) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_HIDDEV);
   pIdent->hiddev_devno = hiddev_devno;
   return pIdent;
}


/** Creates a #Display_Identifier using a USB bus number/device number pair.
 *
 * \param  bus    USB bus number
 * \param  device USB device number
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_usb_display_identifier(int bus, int device) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_USB);
   pIdent->usb_bus = bus;
   pIdent->usb_device = device;
   return pIdent;
}


/** Reports the contents of a #Display_Identifier in a format suitable
 *  for debugging use.
 *
 *  \param  pdid pointer to #Display_Identifier instance
 *  \param  depth logical indentation depth
 */
void dbgrpt_display_identifier(Display_Identifier * pdid, int depth) {
   rpt_structure_loc("BasicStructureRef", pdid, depth );
   int d1 = depth+1;
   rpt_mapped_int("ddc_io_mode",   NULL, pdid->id_type, (Value_To_Name_Function) display_id_type_name, d1);
   rpt_int( "dispno",        NULL, pdid->dispno,        d1);
   rpt_int( "busno",         NULL, pdid->busno,         d1);
   rpt_int( "iAdapterIndex", NULL, pdid->iAdapterIndex, d1);
   rpt_int( "iDisplayIndex", NULL, pdid->iDisplayIndex, d1);
   rpt_int( "usb_bus",       NULL, pdid->usb_bus,       d1);
   rpt_int( "usb_device",    NULL, pdid->usb_device,    d1);
   rpt_int( "hiddev_devno",  NULL, pdid->hiddev_devno,  d1);
   rpt_str( "mfg_id",        NULL, pdid->mfg_id,        d1);
   rpt_str( "model_name",    NULL, pdid->model_name,    d1);
   rpt_str( "serial_ascii",  NULL, pdid->serial_ascii,  d1);

   char * edidstr = hexstring(pdid->edidbytes, 128);
   rpt_str( "edid",          NULL, edidstr,             d1);
   free(edidstr);

#ifdef ALTERNATIVE
   // avoids a malloc and free, but less clear
   char edidbuf[257];
   char * edidstr2 = hexstring2(pdid->edidbytes, 128, NULL, true, edidbuf, 257);
   rpt_str( "edid",          NULL, edidstr2, d1);
#endif
}


/** Returns a succinct representation of a #Display_Identifier for
 *  debugging purposes.
 *
 *  \param pdid pointer to #Display_Identifier
 *  \return pointer to string description
 *
 *  \remark
 *  The returned pointer is valid until the #Display_Identifier is freed.
 */
char * did_repr(Display_Identifier * pdid) {
   if (!pdid->repr) {
      char * did_type_name = display_id_type_name(pdid->id_type);
      switch (pdid->id_type) {
      case(DISP_ID_BUSNO):
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, bus=/dev/i2c-%d]", did_type_name, pdid->busno);
            break;
      case(DISP_ID_ADL):
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, adlno=%d.%d]", did_type_name, pdid->iAdapterIndex, pdid->iDisplayIndex);
            break;
      case(DISP_ID_MONSER):
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, mfg=%s, model=%s, sn=%s]",
                     did_type_name, pdid->mfg_id, pdid->model_name, pdid->serial_ascii);
            break;
      case(DISP_ID_EDID):
      {
            char * hs = hexstring(pdid->edidbytes, 128);
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, edid=%8s...%8s]", did_type_name, hs, hs+248);
            free(hs);
            break;
      }
      case(DISP_ID_DISPNO):
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, dispno=%d]", did_type_name, pdid->dispno);
            break;
      case DISP_ID_USB:
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, usb bus:device=%d.%d]", did_type_name, pdid->usb_bus, pdid->usb_device);;
            break;
      case DISP_ID_HIDDEV:
            pdid->repr = g_strdup_printf(
                     "Display Id[type=%s, hiddev_devno=%d]", did_type_name, pdid->hiddev_devno);
            break;

      } // switch

   }
   return pdid->repr;
}



/** Frees a #Display_Identifier instance
 *
 * \param pdid pointer to #Display_Identifier to free
 */
void free_display_identifier(Display_Identifier * pdid) {
   if (pdid) {
      assert( memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) == 0);
      pdid->marker[3] = 'x';
      free(pdid->repr);   // may be null, that's ok
      free(pdid);
   }
}


#ifdef FUTURE
// *** Display Selector *** (future)

Display_Selector * dsel_new() {
   Display_Selector * dsel = calloc(1, sizeof(Display_Selector));
   memcpy(dsel->marker, DISPLAY_SELECTOR_MARKER, 4);
   dsel->dispno        = -1;
   dsel->busno         = -1;
   dsel->iAdapterIndex = -1;
   dsel->iDisplayIndex = -1;
   dsel->usb_bus       = -1;
   dsel->usb_device    = -1;
   return dsel;
}

void dsel_free(Display_Selector * dsel) {
   if (dsel) {
      assert(memcmp(dsel->marker, DISPLAY_SELECTOR_MARKER, 4) == 0);
      free(dsel->mfg_id);
      free(dsel->model_name);
      free(dsel->serial_ascii);
      free(dsel->edidbytes);
   }
}
#endif


// *** DDCA_IO_Mode and DDCA_IO_Path ***

static char * IO_Mode_Names[] = {
      "DDCA_IO_DEVI2C",
      "DDCA_IO_ADL",
      "DDCA_IO_USB"
};


/** Returns the symbolic name of a #DDCA_IO_Mode value.
 *
 * \param val #DDCA_IO_Mode value
 * \return symbolic name, e.g. "DDCA_IO_DEVI2C"
 */
char * io_mode_name(DDCA_IO_Mode val) {
   return (val >= 0 && val < 3)            // protect against bad arg
         ? IO_Mode_Names[val]
         : NULL;
}


/** Thread safe function that returns a brief string representation of a #DDCA_IO_Path.
 *  The returned value is valid until the next call to this function on the current thread.
 *
 *  \param  dpath  pointer to ##DDCA_IO_Path
 *  \return string representation of #DDCA_IO_Path
 */
char * dpath_short_name_t(DDCA_IO_Path * dpath) {
   static GPrivate  dpath_short_name_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dpath_short_name_key, 100);
   switch(dpath->io_mode) {
   case DDCA_IO_I2C:
      snprintf(buf, 100, "bus dev/i2c-%d", dpath->path.i2c_busno);
      break;
   case DDCA_IO_ADL:
      snprintf(buf, 100, "adlno (%d.%d)", dpath->path.adlno.iAdapterIndex, dpath->path.adlno.iDisplayIndex);
      break;
   case DDCA_IO_USB:
      snprintf(buf, 100, "usb /dev/usb/hiddev%d", dpath->path.hiddev_devno);
   }
   return buf;
}


/** Thread safe function that returns a string representation of a #DDCA_IO_Path
 *  suitable for diagnostic messages. The returned value is valid until the
 *  next call to this function on the current thread.
 *
 *  \param  dpath  pointer to ##DDCA_IO_Path
 *  \return string representation of #DDCA_IO_Path
 */
char * dpath_repr_t(DDCA_IO_Path * dpath) {
   static GPrivate  dpath_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dpath_repr_key, 100);
   switch(dpath->io_mode) {
   case DDCA_IO_I2C:
      snprintf(buf, 100, "Display_Path[/dev/i2c-%d]", dpath->path.i2c_busno);
      break;
   case DDCA_IO_ADL:
      snprintf(buf, 100, "Display_Path[adl=(%d.%d)]", dpath->path.adlno.iAdapterIndex, dpath->path.adlno.iDisplayIndex);
      break;
   case DDCA_IO_USB:
      snprintf(buf, 100, "Display_Path[/dev/usb/hiddev%d]", dpath->path.hiddev_devno);
   }
   return buf;
}


// *** Display_Ref ***


static Display_Ref * create_base_display_ref(DDCA_IO_Path io_path) {
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->io_path = io_path;
   dref->vcp_version = DDCA_VSPEC_UNQUERIED;

   dref->async_rec  = get_display_async_rec(io_path);    // keep?

   return dref;
}



// PROBLEM: bus display ref getting created some other way
/** Creates a #Display_Ref for IO mode #DDCA_IO_I2C
 *
 * @param busno /dev/i2c bus number
 * \return pointer to newly allocated #Display_Ref
 */
Display_Ref * create_bus_display_ref(int busno) {
   bool debug = false;
   DDCA_IO_Path io_path;
   io_path.io_mode   = DDCA_IO_I2C;
   io_path.path.i2c_busno = busno;
   Display_Ref * dref = create_base_display_ref(io_path);
   if (debug) {
      DBGMSG("Done.  Constructed bus display ref:");
      dbgrpt_display_ref(dref,0);
   }
   return dref;
}


/** Creates a #Display_Ref for IO mode #DDCA_IO_ADL
 *
 * @param  iAdapterIndex   ADL adapter index
 * @param  iDisplayIndex   ADL display index
 * \return pointer to newly allocated #Display_Ref
 */
Display_Ref * create_adl_display_ref(int iAdapterIndex, int iDisplayIndex) {
   bool debug = false;
   DDCA_IO_Path io_path;
   io_path.io_mode   = DDCA_IO_ADL;
   io_path.path.adlno.iAdapterIndex = iAdapterIndex;
   io_path.path.adlno.iDisplayIndex = iDisplayIndex;
   Display_Ref * dref = create_base_display_ref(io_path);
   if (debug) {
      DBGMSG("Done.  Constructed ADL display ref:");
      dbgrpt_display_ref(dref,0);
   }
   return dref;
}


#ifdef USE_USB
/** Creates a #Display_Ref for IO mode #DDCA_IO_ADL
 *
 * @param  usb_bus USB bus number
 * @param  usb_device USB device number
 * @param  hiddev_devname device name, e.g. /dev/usb/hiddev1
 * \return pointer to newly allocated #Display_Ref
 */
Display_Ref * create_usb_display_ref(int usb_bus, int usb_device, char * hiddev_devname) {
   assert(hiddev_devname);
   bool debug = false;
   DDCA_IO_Path io_path;
   io_path.io_mode      = DDCA_IO_USB;
   io_path.path.hiddev_devno = hiddev_name_to_number(hiddev_devname);
   Display_Ref * dref = create_base_display_ref(io_path);

   dref->usb_bus     = usb_bus;
   dref->usb_device  = usb_device;
   dref->usb_hiddev_name = strdup(hiddev_devname);

   if (debug) {
      DBGMSG("Done.  Constructed ADL display ref:");
      dbgrpt_display_ref(dref,0);
   }
   return dref;
}
#endif


#ifdef THANKFULLY_UNNEEDED
// Issue: what to do with referenced data structures
Display_Ref * clone_display_ref(Display_Ref * old) {
   assert(old);
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   // dref->ddc_io_mode = old->ddc_io_mode;
   // dref->busno         = old->busno;
   // dref->iAdapterIndex = old->iAdapterIndex;
   // dref->iDisplayIndex = old->iDisplayIndex;
   // DBGMSG("dref=%p, old=%p, len=%d  ", dref, old, (int) sizeof(BasicDisplayRef) );
   memcpy(dref, old, sizeof(Display_Ref));
   if (old->usb_hiddev_name) {
      dref->usb_hiddev_name = strcpy(dref->usb_hiddev_name, old->usb_hiddev_name);
   }
   return dref;
}
#endif


// Is it still meaningful to free a display ref?

void free_display_ref(Display_Ref * dref) {
   if (dref && (dref->flags & DREF_TRANSIENT) ) {
      assert(memcmp(dref->marker, DISPLAY_REF_MARKER,4) == 0);
      dref->marker[3] = 'x';
      if (dref->usb_hiddev_name)       // always set by strdup()
         free(dref->usb_hiddev_name);
      if (dref->capabilities_string)   // always a private copy
         free(dref->capabilities_string);
      // 9/2017: what about pedid, detail2?
      // what to do with gdl, request_queue?
      free(dref);
   }
}

/** Tests if 2 #Display_Ref instances specify the same path to the
 *  display.
 *
 *  Note that if a display communicates MCCS over both I2C and USB
 *  these are different paths to the display.
 *
 *  \param this pointer to first #Display_Ref
 *  \param that pointer to second Ddisplay_Ref
 *  \retval true same display
 *  \retval false different displays
 */
bool dref_eq(Display_Ref* this, Display_Ref* that) {
#ifdef OLD
   bool result = false;
   if (!this && !that)
      result = true;
   else if (this && that) {
      if (this->io_mode == that->io_mode) {
         switch (this->io_mode) {

         case DDCA_IO_I2C:
            result = (this->busno == that->busno);
            break;

         case DDCA_IO_ADL:
            result = (this->iAdapterIndex == that->iAdapterIndex &&
                      this->iDisplayIndex == that->iDisplayIndex);
            break;

         case DDCA_IO_USB:
            result = (this->usb_bus    == that->usb_bus  &&
                      this->usb_device == that->usb_device);
            break;

         }
      }
   }
   return result;
#endif
   // return dpath_eq(dpath_from_dref(this), dpath_from_dref(that));
   return dpath_eq(this->io_path, that->io_path);
}


/** Reports the contents of a #Display_Ref in a format appropriate for debugging.
 *
 *  \param  dref  pointer to #Display_Ref instance
 *  \param  depth logical indentation depth
 */
void dbgrpt_display_ref(Display_Ref * dref, int depth) {
   rpt_structure_loc("DisplayRef", dref, depth );
   int d1 = depth+1;
   int d2 = depth+2;

#ifdef OLD
   // old
   rpt_mapped_int("ddc_io_mode", NULL, dref->io_mode, (Value_To_Name_Function) io_mode_name, d1);

   switch (dref->io_mode) {

   case DDCA_IO_I2C:
      rpt_int("busno", NULL, dref->busno, d1);
      break;

   case DDCA_IO_ADL:
      rpt_int("iAdapterIndex", NULL, dref->iAdapterIndex, d1);
      rpt_int("iDisplayIndex", NULL, dref->iDisplayIndex, d1);
      break;

   case DDCA_IO_USB:
      rpt_int("usb_bus",    NULL, dref->usb_bus,    d1);
      rpt_int("usb_device", NULL, dref->usb_device, d1);
      rpt_str("usb_hiddev_name", NULL, dref->usb_hiddev_name, d1);
      rpt_int("usb_hiddev_devno", NULL, dref->usb_hiddev_devno, d1);
      break;

   }
#endif

   // alt:
   rpt_vstring(d1, "io_path:      %s", dpath_repr_t(&(dref->io_path)));
   if (dref->io_path.io_mode == DDCA_IO_USB) {
      rpt_int("usb_bus",    NULL, dref->usb_bus,    d1);
      rpt_int("usb_device", NULL, dref->usb_device, d1);
      rpt_str("usb_hiddev_name", NULL, dref->usb_hiddev_name, d1);
   }

   // rpt_vstring(d1, "vcp_version:  %d.%d\n", dref->vcp_version.major, dref->vcp_version.minor );
   rpt_vstring(d1, "vcp_version:  %s", format_vspec(dref->vcp_version) );
   rpt_vstring(d1, "flags:        0x%02x", dref->flags);
   rpt_vstring(d2, "DDC communication checked:                  %s", (dref->flags & DREF_DDC_COMMUNICATION_CHECKED) ? "true" : "false");
   if (dref->flags & DREF_DDC_COMMUNICATION_CHECKED)
   rpt_vstring(d2, "DDC communication working:                  %s", (dref->flags & DREF_DDC_COMMUNICATION_WORKING) ? "true" : "false");
   rpt_vstring(d2, "DDC NULL response usage checked:            %s", bool_repr(dref->flags & DREF_DDC_NULL_RESPONSE_CHECKED));
   if (dref->flags & DREF_DDC_NULL_RESPONSE_CHECKED)
   rpt_vstring(d2, "DDC NULL response may indicate unsupported: %s", bool_repr(dref->flags & DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED));
}


#ifdef OLD
/** Creates a short description of a #Display_Ref in a buffer provided
 *  by the caller.
 *
 *  \param  dref   pointer to #Display_Ref
 *  \param  buf    pointer to buffer
 *  \param  bufsz  buffer size
 */
static
char * dref_short_name_r(Display_Ref * dref, char * buf, int bufsz) {
   assert(buf);
   assert(bufsz > 0);

   switch (dref->io_mode) {

   case DDCA_IO_I2C:
      SAFE_SNPRINTF(buf, bufsz, "bus /dev/i2c-%d", dref->busno);

      // snprintf(buf, bufsz, "bus /dev/i2c-%d", dref->busno);
      // buf[bufsz-1] = '\0';  // ensure null terminated
      break;

   case DDCA_IO_ADL:
      SAFE_SNPRINTF(buf, bufsz, "adl display %d.%d", dref->iAdapterIndex, dref->iDisplayIndex);
      // snprintf(buf, bufsz, "adl display %d.%d", dref->iAdapterIndex, dref->iDisplayIndex);
      // buf[bufsz-1] = '\0';  // ensure null terminated
      break;

   case DDCA_IO_USB:
      SAFE_SNPRINTF(buf, bufsz, "usb %d:%d", dref->usb_bus, dref->usb_device);
      // snprintf(buf, bufsz, "usb %d:%d", dref->usb_bus, dref->usb_device);
      buf[bufsz-1] = '\0';  // ensure null terminated
      break;

   }
   return buf;
}
#endif

/** Thread safe function that returns a short description of a #Display_Ref.
 *  The returned value is valid until the next call to this function on
 *  the current thread.
 *
 *  \param  dref  pointer to #Display_Ref
 *  \return short description
 */
char * dref_short_name_t(Display_Ref * dref) {
   return dpath_short_name_t(&dref->io_path);
#ifdef OLD
   static GPrivate  dref_short_name_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&dref_short_name_key, 100);
   char buf2[80];
   snprintf(buf, 100, "Display_Ref[%s]", dref_short_name_r(dref, buf2, 80) );
   return buf;
#endif
}


/** Thread safe function that returns a string representation of a #Display_Ref
 *  suitable for diagnostic messages. The returned value is valid until the
 *  next call to this function on the current thread.
 *
 *  \param  dref  pointer to #Display_Ref
 *  \return string representation of #Display_Ref
 */
char * dref_repr_t(Display_Ref * dref) {
   static GPrivate  dref_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dref_repr_key, 100);
   g_snprintf(buf, 100, "Display_Ref[%s]", dpath_short_name_t(&dref->io_path));
   return buf;
}


// *** Display_Handle ***

/** Creates a #Display_Handle for an I2C #Display_Ref.
 *
 *  \param  fh   file handle of open display
 *  \param  dref pointer to #Display_Ref
 *  \return newly allocated #Display_Handle
 *
 *  \remark
 *  This functions handles the boilerplate of creating a #Display_Handle.
 */
Display_Handle * create_bus_display_handle_from_display_ref(int fh, Display_Ref * dref) {
   // assert(dref->io_mode == DDCA_IO_DEVI2C);
   assert(dref->io_path.io_mode == DDCA_IO_I2C);
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->fh = fh;
   dh->dref = dref;
   dref->vcp_version = DDCA_VSPEC_UNQUERIED;
   dh->repr = g_strdup_printf(
                "Display_Handle[i2c: fh=%d, busno=%d]",
                dh->fh, dh->dref->io_path.path.i2c_busno);
   return dh;
}


/** Creates a #Display_Handle for an ADL #Display_Ref.
 *
 *  \param  dref pointer to #Display_Ref
 *  \return newly allocated #Display_Handle
 *
 *  \remark
 *  This functions handles the boilerplate of creating a #Display_Handle.
 */
Display_Handle * create_adl_display_handle_from_display_ref(Display_Ref * dref) {
   // assert(dref->io_mode == DDCA_IO_ADL);
   assert(dref->io_path.io_mode == DDCA_IO_ADL);
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->dref = dref;
   dref->vcp_version = DDCA_VSPEC_UNQUERIED;   // needed?
   dh->repr = g_strdup_printf(
                "Display_Handle[adl: display %d.%d]",
                 dh->dref->io_path.path.adlno.iAdapterIndex, dh->dref->io_path.path.adlno.iDisplayIndex);
   return dh;
}


#ifdef USE_USB
/** Creates a #Display_Handle for a USB #Display_Ref.
 *
 *  \param  fh   file handle of open display
 *  \param  dref pointer to #Display_Ref
 *  \return newly allocated #Display_Handle
 *
 *  \remark
 *  This functions handles to boilerplate of creating a #Display_Handle.
 */
Display_Handle * create_usb_display_handle_from_display_ref(int fh, Display_Ref * dref) {
   // assert(dref->io_mode == DDCA_IO_USB);
   assert(dref->io_path.io_mode == DDCA_IO_USB);
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->fh = fh;
   dh->dref = dref;
   dh->repr = g_strdup_printf(
                "Display_Handle[usb: %d:%d, %s/hiddev%d]",
                dh->dref->usb_bus, dh->dref->usb_device,
                usb_hiddev_directory(), dh->dref->io_path.path.hiddev_devno);
   dref->vcp_version = DDCA_VSPEC_UNQUERIED;
   return dh;
}
#endif


/** Reports the contents of a #Display_Handle in a format useful for debugging.
 *
 *  \param  dh       display handle
 *  \param  msg      if non-null, output this string before the #Display_Handle detail
 *  \param  depth    logical indentation depth
 */
void dbgrpt_display_handle(Display_Handle * dh, const char * msg, int depth) {
   int d1 = depth+1;
   if (msg)
      rpt_vstring(depth, "%s", msg);
   rpt_vstring(d1, "Display_Handle: %p", dh);
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0) {
         rpt_vstring(d1, "Invalid marker in struct: 0x%08x, |%.4s|\n",
                         *dh->marker, (char *)dh->marker);
      }
      else {
         rpt_vstring(d1, "dref:                 %p", dh->dref);
         rpt_vstring(d1, "io mode:              %s",  io_mode_name(dh->dref->io_path.io_mode) );
         switch (dh->dref->io_path.io_mode) {
         case (DDCA_IO_I2C):
            // rpt_vstring(d1, "ddc_io_mode = DDC_IO_DEVI2C");
            rpt_vstring(d1, "fh:                  %d", dh->fh);
            rpt_vstring(d1, "busno:               %d", dh->dref->io_path.path.i2c_busno);
            break;
         case (DDCA_IO_ADL):
            // rpt_vstring(d1, "ddc_io_mode = DDC_IO_ADL");
            rpt_vstring(d1, "iAdapterIndex:       %d", dh->dref->io_path.path.adlno.iAdapterIndex);
            rpt_vstring(d1, "iDisplayIndex:       %d", dh->dref->io_path.path.adlno.iDisplayIndex);
            break;
         case (DDCA_IO_USB):
            // rpt_vstring(d1, "ddc_io_mode = USB_IO");
            rpt_vstring(d1, "fh:                  %d", dh->fh);
            rpt_vstring(d1, "usb_bus:             %d", dh->dref->usb_bus);
            rpt_vstring(d1, "usb_device:          %d", dh->dref->usb_device);
            rpt_vstring(d1, "hiddev_device_name:  %s", dh->dref->usb_hiddev_name);
            break;
         }
      }
      // rpt_vstring(d1, "vcp_version:         %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   }
}

#ifdef OLD
/* Returns a string summarizing the specified #Display_Handle.
 *
 * The string is returned in a newly allocated buffer.
 * It is the responsibility of the caller to free this buffer.
 *
 * \param  dh    display handle
 *
 * \return  string representation of handle
 */
char * dh_repr_a(Display_Handle * dh) {
   assert(dh);
   assert(dh->dref);

   char * repr = NULL;

      switch (dh->dref->io_mode) {
      case DDCA_IO_I2C:
          repr = g_strdup_printf(
                   "Display_Handle[i2c: fh=%d, busno=%d]",
                   dh->fh, dh->dref->busno);
          break;
      case DDCA_IO_ADL:
          repr = g_strdup_printf(
                   "Display_Handle[adl: display %d.%d]",
                   dh->dref->iAdapterIndex, dh->dref->iDisplayIndex);
          break;
      case DDCA_IO_USB:
          repr = g_strdup_printf(
                   "Display_Handle[usb: %d:%d, %s/hiddev%d]",
                   dh->dref->usb_bus, dh->dref->usb_device,
                   usb_hiddev_directory(), dh->dref->usb_hiddev_devno);
          break;
      }
      return repr;
   }
#endif


#ifdef OLD
/* Returns a string summarizing the specified #Display_Handle.
 * The string is returned in a buffer provided by the caller.
 *
 * \param  dh    display handle
 * \param  buf   pointer to buffer
 * \param  bufsz buffer size
 *
 * \return  string representation of handle (buf)
 */
static char * dh_repr_r(Display_Handle * dh, char * buf, int bufsz) {
   assert(dh);
   assert(dh->dref);

   switch (dh->dref->io_mode) {
   case DDCA_IO_I2C:
       snprintf(buf, bufsz,
                "Display_Handle[i2c: fh=%d, busno=%d]",
                dh->fh, dh->dref->busno);
       break;
   case DDCA_IO_ADL:
       snprintf(buf, bufsz,
                "Display_Handle[adl: display %d.%d]",
                dh->dref->iAdapterIndex, dh->dref->iDisplayIndex);
       break;
   case DDCA_IO_USB:
       snprintf(buf, bufsz,
                "Display_Handle[usb: %d:%d, %s/hiddev%d]",
                dh->dref->usb_bus, dh->dref->usb_device,
                usb_hiddev_directory(), dh->dref->usb_hiddev_devno);
       break;
   }
   buf[bufsz-1] = '\0';
   return buf;
}
#endif


/** Returns a string summarizing the specified #Display_Handle.
 *
 *  The string is valid until the next call to this function
 *  from within the current thread.
 *
 *  This variant of #dh_repr() is thread safe.
 *
 * \param  dh    display handle
 * \return  string representation of handle
 */
char * dh_repr_t(Display_Handle * dh) {
   static GPrivate  dh_buf_key = G_PRIVATE_INIT(g_free);
   const int bufsz = 100;
   char * buf = get_thread_fixed_buffer(&dh_buf_key, bufsz);

   assert(dh);
   assert(dh->dref);

   switch (dh->dref->io_path.io_mode) {
   case DDCA_IO_I2C:
        snprintf(buf, bufsz,
                 "Display_Handle[i2c: fh=%d, busno=%d]",
                 dh->fh, dh->dref->io_path.path.i2c_busno);
        break;
    case DDCA_IO_ADL:
        snprintf(buf, bufsz,
                 "Display_Handle[adl: display %d.%d]",
                 dh->dref->io_path.path.adlno.iAdapterIndex, dh->dref->io_path.path.adlno.iDisplayIndex);
        break;
    case DDCA_IO_USB:
        snprintf(buf, bufsz,
                 "Display_Handle[usb: %d:%d, %s/hiddev%d]",
                 dh->dref->usb_bus, dh->dref->usb_device,
                 usb_hiddev_directory(), dh->dref->io_path.path.hiddev_devno);
        break;
    }
    buf[bufsz-1] = '\0';

   return buf;
}


/** Returns a string summarizing the specified #Display_Handle.
 *
 * \param  dh    display handle
 *
 * \return  string representation of handle
 */
char * dh_repr(Display_Handle * dh) {
   assert(dh);
   assert(dh->dref);
   assert(dh->repr);
   // Do not calculate and memoize dh->repr here, due to possible race condition between threads
   // Instead always precalculate at time of Display_Handle creation
   return dh->repr;
}


/** Frees a #Display_Handle struct.
 *
 * \param  dh  display handle to free
 */
void   free_display_handle(Display_Handle * dh) {
   if (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) == 0) {
      dh->marker[3] = 'x';
      free(dh->repr);
      free(dh);
   }
}


// *** Miscellaneous ***

/** Creates and initializes a #Video_Card_Info struct.
 *
 * \return new instance
 *
 * \remark
 * Currently unused.  Struct Video_Card_Info is referenced only in ADL code.
 */
Video_Card_Info * create_video_card_info() {
   Video_Card_Info * card_info = calloc(1, sizeof(Video_Card_Info));
   memcpy(card_info->marker, VIDEO_CARD_INFO_MARKER, 4);
   return card_info;
}


/** Given a hiddev device name, e.g. /dev/usb/hiddev3,
 *  extract its number, e.g. 3.
 *
 *  \param   hiddev_name device name
 *  \return  device number, -1 if error
 */
int hiddev_name_to_number(char * hiddev_name) {
   assert(hiddev_name);
   char * p = strstr(hiddev_name, "hiddev");

   int hiddev_number = -1;
   if (p) {
      p = p + strlen("hiddev");
      if (strlen(p) > 0) {
         // hiddev_number unchanged if error
         // n str_to_int() allows leading whitespace, not worth checking
         bool ok = str_to_int(p, &hiddev_number, 10);
         if (!ok)
            hiddev_number = -1;   // not necessary, but makes coverity happy
      }
   }
   // DBGMSG("hiddev_name = |%s|, returning: %d", hiddev_name, hiddev_number);
   return hiddev_number;
}


/** Given a hiddev device number, e.g. 3, return its name, e.g. /dev/usb/hiddev3
 *
 *  \param   hiddev_number device number
 *  \return  device name
 *
 *  \remark It the the responsibility of the caller to free the returned string.
 */
char * hiddev_number_to_name(int hiddev_number) {
   assert(hiddev_number >= 0);
   char * s = g_strdup_printf("%s/hiddev%d", usb_hiddev_directory(),hiddev_number);
   // DBGMSG("hiddev_number=%d, returning: %s", hiddev_number, s);
   return s;
}




void init_displays() {
   displays_master_list = g_ptr_array_new();

}



