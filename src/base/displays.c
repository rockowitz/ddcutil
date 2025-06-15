/** @file displays.c   Monitor identifier, reference, handle  */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/timestamp.h"
#ifdef ENABLE_UDEV
#include "util/udev_util.h"
#include "util/udev_usb_util.h"
#endif
/** \endcond */

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"

#include "core.h"
#include "i2c_bus_base.h"
#include "monitor_model_key.h"
#include "per_display_data.h"
#include "rtti.h"
#include "vcp_version.h"

#include "displays.h"

GPtrArray * all_display_refs = NULL;         // all detected displays, array of Display_Ref *
GMutex      all_display_refs_mutex;
bool        debug_locks = false;

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
      case DDCA_IO_USB:
         result = p1.path.hiddev_devno == p2.path.hiddev_devno;
      }
   }
   return result;
}


/** Creates a unique integer number from a #DDCA_IO_Path, suitable
 *  for use as a hash key.
 *
 *  \param  path   io path
 *  \return integer value
 */
int dpath_hash(DDCA_IO_Path path) {
   return path.io_mode * 100 + path.path.i2c_busno;
}


// *** Display_Identifier ***

static char * Display_Id_Type_Names[] = {
      "DISP_ID_BUSNO",
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


/** Creates a #Display_Identifier using an EDID value
 *
 * \param  edidbytes  pointer to 128 byte EDID value
 * \return pointer to newly allocated #Display_Identifier
 *
 * \remark
 * It is the responsibility of the caller to free the allocated
 * #Display_Identifier using #free_display_identifier().
 */
Display_Identifier* create_edid_display_identifier(const Byte* edidbytes) {
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
      const char* serial_ascii)
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
   rpt_int( "usb_bus",       NULL, pdid->usb_bus,       d1);
   rpt_int( "usb_device",    NULL, pdid->usb_device,    d1);
   rpt_int( "hiddev_devno",  NULL, pdid->hiddev_devno,  d1);
   rpt_str( "mfg_id",        NULL, pdid->mfg_id,        d1);
   rpt_str( "model_name",    NULL, pdid->model_name,    d1);
   rpt_str( "serial_ascii",  NULL, pdid->serial_ascii,  d1);

   char * edidstr = hexstring(pdid->edidbytes, 128);
   rpt_str( "edid",          NULL, edidstr,             d1);
   free(edidstr);
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
   char * result = NULL;
   if (pdid) {
      if (!pdid->repr) {
         char * did_type_name = display_id_type_name(pdid->id_type);
         switch (pdid->id_type) {
         case(DISP_ID_BUSNO):
               pdid->repr = g_strdup_printf(
                        "Display Id[type=%s, bus=/dev/i2c-%d]", did_type_name, pdid->busno);
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
      } // !pdid->repr
      result = pdid->repr;
   }
   return result;
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


// #ifdef FUTURE
// *** Display Selector *** (future)

Display_Selector * dsel_new() {
   Display_Selector * dsel = calloc(1, sizeof(Display_Selector));
   memcpy(dsel->marker, DISPLAY_SELECTOR_MARKER, 4);
   dsel->dispno        = -1;
   dsel->busno         = -1;
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

// #endif


// *** DDCA_IO_Mode and DDCA_IO_Path ***

static char * IO_Mode_Names[] = {
      "DDCA_IO_I2C",
      "DDCA_IO_USB"
};


/** Returns the symbolic name of a #DDCA_IO_Mode value.
 *
 * \param val #DDCA_IO_Mode value
 * \return symbolic name, e.g. "DDCA_IO_DEVI2C"
 */
char * io_mode_name(DDCA_IO_Mode val) {
   return (val >= 0 && val < 2)            // protect against bad arg
         ? IO_Mode_Names[val]
         : NULL;
}


/** A simple function allowing for the assignment of a value to a
 *  #DDCA_IO_Path instance in a single line of code.
 *
 *  @parm   busno    I2C bus number
 *  @return DDCA_IO_Path value
 */
DDCA_IO_Path i2c_io_path(int busno) {
   DDCA_IO_Path path;
   path.io_mode = DDCA_IO_I2C;
   path.path.i2c_busno = busno;
   return path;
}


/** A simple function allowing for the assignment of a value to a
 *  #DDCA_IO_Path instance in a single line of code.
 *
 *  @parm   hiddev  USB device number
 *  @return DDCA_IO_Path value
 */
DDCA_IO_Path usb_io_path(int hiddev_devno) {
   DDCA_IO_Path path;
   path.io_mode = DDCA_IO_USB;
   path.path.hiddev_devno = hiddev_devno;
   return path;
}


/** Thread safe function that returns a brief string representation of a #DDCA_IO_Path.
 *  The returned value is valid until the next call to this function on the current thread.
 *
 *  \remark
 *  A bus number of 255 represents a value that has not been set.  The string
 *  "NOT SET" is returned.
 *
 *  \param  dpath  pointer to ##DDCA_IO_Path
 *  \return string representation of #DDCA_IO_Path
 */
char * dpath_short_name_t(DDCA_IO_Path * dpath) {
   static GPrivate  dpath_short_name_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dpath_short_name_key, 100);
   switch(dpath->io_mode) {
   case DDCA_IO_I2C:
      if (dpath->path.i2c_busno == 255)
         g_strlcpy(buf, "NOT SET", 100);
      else
         g_snprintf(buf, 100, "bus /dev/i2c-%d", dpath->path.i2c_busno);
      break;
   case DDCA_IO_USB:
      g_snprintf(buf, 100, "usb /dev/usb/hiddev%d", dpath->path.hiddev_devno);
   }
   return buf;
}


/** Thread safe function that returns a string representation of a #DDCA_IO_Path
 *  suitable for diagnostic messages. The returned value is valid until the
 *  next call to this function on the current thread.
 *
 *  \param  dpath  pointer to #DDCA_IO_Path
 *  \return string representation of #DDCA_IO_Path
 */
char * dpath_repr_t(DDCA_IO_Path * dpath) {
   static GPrivate  dpath_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dpath_repr_key, 100);
   if (dpath) {
      switch(dpath->io_mode) {
      case DDCA_IO_I2C:
         if (dpath->path.i2c_busno == BUSNO_NOT_SET)
            snprintf(buf, 100, "Display Path not set");
         else
            snprintf(buf, 100, "Display_Path[/dev/i2c-%d]", dpath->path.i2c_busno);
         break;
      case DDCA_IO_USB:
         snprintf(buf, 100, "Display_Path[/dev/usb/hiddev%d]", dpath->path.hiddev_devno);
      }
   }
   else
      snprintf(buf, 100, "NULL Display Path");
   return buf;
}


//
// *** Published Display Reference Table
//
// Converts between the internal (pointer) and external (integer) display
// reference form, and maintains a table of all display references that
// have been exposed by the API. This table is used to validate display
// reference arguments on API calls.
//
// The external display reference is the unique dref_id stored in each
// Display_Reference when it is created. Table published_dref_hash has
// entries whose keys are the external DDCA_Display_Ref and whose values
// are pointers to the internal Display_Ref.

static uint max_dref_id = 0;
static GMutex max_dref_id_mutex;
static GHashTable * published_dref_hash = NULL;
static GMutex dref_hash_mutex;


void init_published_dref_hash() {
   published_dref_hash = g_hash_table_new(g_direct_hash, NULL);
}


void reset_published_dref_hash() {
   if (published_dref_hash)
      g_hash_table_destroy(published_dref_hash);
   init_published_dref_hash();
}


void dbgrpt_published_dref_hash(const char * msg, int depth) {
    if (msg)
       rpt_vstring(depth, "%s: dref_hash_contents:", msg);
    else
       rpt_label(depth, "dref_hash contents: ");

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, published_dref_hash);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
       uint dref_id = GPOINTER_TO_UINT(key);
       Display_Ref * dref = (Display_Ref *) value;
       rpt_vstring(depth+1, "dref_id %d -> %s", dref_id, dref_reprx_t(dref));
    }
}


static uint next_dref_id(Display_Ref * dref) {
   bool debug = false;
   g_mutex_lock (&max_dref_id_mutex);
   guint nextid = ++max_dref_id;
   g_mutex_unlock(&max_dref_id_mutex);
   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "nextid = %u", nextid);
   return nextid;
}


void add_published_dref_id_by_dref(Display_Ref * dref) {
   bool debug = false;
   g_mutex_lock (&dref_hash_mutex);
   g_hash_table_insert(published_dref_hash, GUINT_TO_POINTER(dref->dref_id), dref);
   if (debug) {
      char msgbuf[100];
      g_snprintf(msgbuf, 100, "After dref %s inserted", dref_reprx_t(dref));
      dbgrpt_published_dref_hash(msgbuf, 0);
   }
   g_mutex_unlock(&dref_hash_mutex);
   DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "%s -> %d", dref_reprx_t(dref), dref->dref_id);
}


static void delete_published_dref_id(uint dref_id) {
   bool debug = false;
   g_mutex_lock (&dref_hash_mutex);
   g_hash_table_remove(published_dref_hash, GUINT_TO_POINTER(dref_id));
   if (debug) {
      char msgbuf[50];
      g_snprintf(msgbuf, 50, "After dref_id %d removed", dref_id);
      dbgrpt_published_dref_hash(msgbuf, 0);
   }
   g_mutex_unlock(&dref_hash_mutex);
}


#ifdef UNUSED
Display_Ref * dref_id_to_ptr(guint dref_id) {
   bool debug = false;
   if (debug)
      dbgrpt_published_dref_hash("Before g_hash_table_lookup", 2);

   Display_Ref * dref = g_hash_table_lookup(published_dref_hash, GUINT_TO_POINTER(dref_id));
   return dref;
}
#endif


/** Given a DDCA_Display_Ref, looks up the corresponding Display_Ref*
 *  in the hash table of external display refs that have been given
 *  to the client.
 *
 *   @param ddca_dref public opaque display ref
 *   @result pointer to internal Display_Ref, NULL if not found
 */
Display_Ref * dref_from_published_ddca_dref(DDCA_Display_Ref ddca_dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "ddca_dref = %p", ddca_dref);

#ifdef NUMERIC_DDCA_DISPLAY_REF
    // if (debug)
    //    dbgrpt_published_dref_hash(__func__, 1);
   guint id = GPOINTER_TO_UINT(ddca_dref);
   Display_Ref * dref = g_hash_table_lookup(published_dref_hash, GUINT_TO_POINTER(id));

   if (dref) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "ddca_dref=%p -> %s", ddca_dref, dref_reprx_t(dref));
      if (memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0)
         dbgrpt_display_ref(dref, true, 2);
      assert(memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
   }
#else
   Display_Ref * dref = (Display_Ref*) ddca_dref;
   if (dref) {
      if (memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0)
         dref = NULL;
   }
#endif

   if (dref)
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "ddca_dref=%p, returning %p -> %s", ddca_dref, dref, dref_reprx_t(dref));
   else
      DBGTRC_DONE(debug, DDCA_TRC_NONE, "ddca_dref=%p, returning %p", ddca_dref, dref);
   return dref;
}


DDCA_Display_Ref dref_to_ddca_dref(Display_Ref * dref) {
   bool debug = false;
   DDCA_Display_Ref ddca_dref = (DDCA_Display_Ref*) GUINT_TO_POINTER(0);
   if (dref) {
#ifdef NUMERIC_DDCA_DISPLAY_REF
      ddca_dref = (DDCA_Display_Ref*) GUINT_TO_POINTER(dref->dref_id);
#else
      ddca_dref = (void*) dref;
#endif
      DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "dref=%p, dref->dref_id=%d, returning %p",
                                            dref, dref->dref_id, ddca_dref);
   }
   else
      DBGTRC_EXECUTED(debug, DDCA_TRC_NONE, "dref=%p, returning %p", dref, ddca_dref);
   return ddca_dref;
}


// *** Display_Ref ***

Display_Ref * create_base_display_ref(DDCA_IO_Path io_path) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "io_path=%s", dpath_repr_t(&io_path));
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->io_path = io_path;
   dref->dref_id = next_dref_id(dref);
   dref->vcp_version_xdf = DDCA_VSPEC_UNQUERIED;
   dref->vcp_version_cmdline = DDCA_VSPEC_UNQUERIED;
   dref->creation_timestamp = cur_realtime_nanosec();
   // Per_Display_Data * pdd = pdd_get_per_display_data(io_path, true);
   // dref->pdd = pdd;
   g_mutex_init(&dref->access_mutex);
   // DBGTRC_RET_STRUCT(debug, DDCA_TRC_BASE, "Display_Ref", dbgrpt_display_ref, dref);
   DBGTRC_DONE(debug, DDCA_TRC_BASE, "Returning %p", dref);
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
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "busno=%d", busno);
   DDCA_IO_Path io_path;
   io_path.io_mode   = DDCA_IO_I2C;
   io_path.path.i2c_busno = busno;
   Display_Ref * dref = create_base_display_ref(io_path);
   if (debug) {
      DBGMSG("Done.  Constructed bus display ref %s:", dref_repr_t(dref));
      dbgrpt_display_ref(dref, true, 0);
   }

   DBGTRC_RET_STRUCT(debug, DDCA_TRC_BASE, "Display_Ref", dbgrpt_display_ref0, dref);
   return dref;
}


#ifdef ENABLE_USB
/** Creates a #Display_Ref for IO mode #DDCA_IO_USB
 *
 * @param  usb_bus USB bus number
 * @param  usb_device USB device number
 * @param  hiddev_devname device name, e.g. /dev/usb/hiddev1
 * \return pointer to newly allocated #Display_Ref
 */
Display_Ref * create_usb_display_ref(int usb_bus, int usb_device, char * hiddev_devname) {
   assert(hiddev_devname);
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "usb_bus=%d, usb_device=%d, hiddev_devname=%s",
         usb_bus, usb_device, hiddev_devname);
   DDCA_IO_Path io_path;
   io_path.io_mode      = DDCA_IO_USB;
   io_path.path.hiddev_devno = hiddev_name_to_number(hiddev_devname);
   Display_Ref * dref = create_base_display_ref(io_path);

   dref->usb_bus     = usb_bus;
   dref->usb_device  = usb_device;
   dref->usb_hiddev_name = g_strdup(hiddev_devname);

   DBGTRC_RET_STRUCT(debug, DDCA_TRC_BASE, "Display_Ref", dbgrpt_display_ref0, dref);
   // DBGTRC_DONE(debug, DDCA_TRC_BASE, "Returning %p", dref);
   return dref;
}
#endif


Display_Ref * copy_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "dref=%p, iopath=%s", dref, (dref) ? dpath_repr_t(&dref->io_path) : NULL);
   Display_Ref * copy = NULL;
   if (dref) {
      DDCA_IO_Path iopath = dref->io_path;
      copy = create_base_display_ref(iopath);
      copy->usb_bus = dref->usb_bus;
      copy->dref_id = next_dref_id(copy);
      copy->usb_device = dref->usb_device;
      copy->usb_hiddev_name = g_strdup(dref->usb_hiddev_name);
      copy->vcp_version_xdf = dref->vcp_version_xdf;
      copy->vcp_version_cmdline = dref->vcp_version_cmdline;
      copy->flags = dref->flags & ~DREF_DYNAMIC_FEATURES_CHECKED;
      copy->capabilities_string = g_strdup(dref->capabilities_string);
      if (dref->pedid) {
         copy->pedid = copy_parsed_edid(dref->pedid);
      }
      if (dref->mmid) {
         copy->mmid = calloc(1, sizeof(Monitor_Model_Key));
         memcpy(copy->mmid, dref->mmid, sizeof(Monitor_Model_Key));
      }
      copy->dispno = dref->dispno;
      // do not set detail
      // do not set dfr
      // do not set actual_display
      copy->actual_display_path = dref->actual_display_path;
      // dont set pdd
      copy->drm_connector = g_strdup(dref->drm_connector);
      copy->drm_connector_id = dref->drm_connector_id;
      copy->drm_connector_found_by = dref->drm_connector_found_by;
   }
   // DBGTRC_RET_STRUCT(debug, DDCA_TRC_BASE, "Display_Ref", dbgrpt_display_ref, copy);
   DBGTRC_DONE(debug, DDCA_TRC_BASE, "Returning %p", copy);
   return copy;
}

//#define CK_INVALIDATED_MARKER(_marker, _marker_name)  ( (memcmp(_marker, _marker_name, 3) == 0) && (_marker[3] = 'x')))
//    assert( !(INVALIDATED_MARKER(dref->marker, DISPLAY_REF_MARKER));


/** Frees a display reference.
 *
 *  \param  dref  ptr to display reference to free, if NULL no operation is performed
 *  \retval DDCRC_OK       success
 *  \retval DDCRC_LOCKED   display reference not marked as transient
 */
DDCA_Status free_display_ref(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "dref=%p", dref);
   DDCA_Status ddcrc = 0;
   if (dref) {
      assert ( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_BASE, "dref=%s, DREF_TRANSIENT=%s, DREF_OPEN=%s",
            dref_repr_t(dref), SBOOL(dref->flags & DREF_TRANSIENT), SBOOL(dref->flags&DREF_OPEN));
      if (dref->flags & DREF_TRANSIENT)  {
         if (dref->flags & DREF_OPEN) {
            ddcrc = DDCRC_LOCKED;
         }
         else {
            uint dref_id = dref->dref_id;
            free(dref->usb_hiddev_name);        // private copy
            free(dref->capabilities_string);    // private copy
            free(dref->mmid);                   // private copy
            if (dref->pedid)  {
               DBGTRC(debug, DDCA_TRC_NONE, "Freeing dref->pedid = %p", dref->pedid);
               free_parsed_edid(dref->pedid);  // private copy
            }
            dfr_free(dref->dfr);
            free(dref->drm_connector);
            free(dref->communication_error_summary);
            g_mutex_clear(&dref->access_mutex);
            dref->marker[3] = 'x';
            free(dref);
            delete_published_dref_id(dref_id);
         }
      }
   }
   DBGTRC_RET_DDCRC(debug, DDCA_TRC_BASE, ddcrc, "");
   return ddcrc;
}


void dref_lock(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "locking dref %s ...", dref_reprx_t(dref));
   bool was_locked = !g_mutex_trylock(&(dref->access_mutex));
   if (was_locked ) {
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "dref %s is locked,  waiting ... ", dref_reprx_t(dref));
      g_mutex_lock(&(dref->access_mutex));
      DBGTRC_NOPREFIX(true, DDCA_TRC_NONE, "obtained lock on %s",  dref_reprx_t(dref));
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "dref %s", dref_reprx_t(dref));
}


void dref_unlock(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "unlocking dref %s ...", dref_reprx_t(dref));

   g_mutex_unlock(&dref->access_mutex);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "dref %s unlocked", dref_reprx_t(dref));
}


#ifdef UNNEEDED
// wraps free_display_ref() as GDestroyNotify()
void gdestroy_display_ref(void * data) {
   free_display_ref((Display_Ref*) data);
}
#endif


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
   return dpath_eq(this->io_path, that->io_path);
}


/** Gets the driver name for an I2C device.
 *
 *  @param dref display reference
 *  @return driver name, caller SHOULD NOT free
 *
 *  Returns NULL if not a I2C device, or display ref is disconnected
 */
const char * dref_get_i2c_driver(Display_Ref* dref) {
   char * result = NULL;
   if (dref->io_path.io_mode == DDCA_IO_I2C) {
      I2C_Bus_Info* businfo = dref->detail;
      if (businfo)
         result = businfo->driver;
   }
   return result;
}


#ifdef UNUSED
bool dref_set_alive(Display_Ref * dref, bool alive) {
   assert(dref);
   bool debug = false;
   bool old = dref->flags & DREF_ALIVE;
   if (old != alive)
      DBGTRC_EXECUTED(debug, DDCA_TRC_BASE, "dref=%s, alive changed: %s -> %s",
                             dref_repr_t(dref), SBOOL(old), SBOOL(alive));
   SETCLR_BIT(dref->flags, DREF_ALIVE, alive);
   return old;
}


bool dref_get_alive(Display_Ref * dref) {
   assert(dref);
   return dref->flags & DREF_ALIVE;;
}
#endif


/** Reports the contents of a #Display_Ref in a format useful for debugging.
 *
 *  \param  dref  pointer to #Display_Ref instance
 *  \param  depth logical indentation depth
 */
void dbgrpt_display_ref(Display_Ref * dref, bool include_businfo, int depth) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dref=%s", dref_repr_t(dref));
   int d1 = depth+1;
   int d2 = depth+2;

   rpt_structure_loc("Display_Ref", dref, depth);
   rpt_vstring(d1, "marker            %.4s", dref->marker);
   rpt_vstring(d1, "dref_id           %d", dref->dref_id);
   rpt_vstring(d1, "io_path:          %s", dpath_repr_t(&(dref->io_path)));
   if (dref->io_path.io_mode == DDCA_IO_USB) {
      rpt_int("usb_bus",         NULL, dref->usb_bus,         d1);
      rpt_int("usb_device",      NULL, dref->usb_device,      d1);
      rpt_str("usb_hiddev_name", NULL, dref->usb_hiddev_name, d1);
   }
   rpt_vstring(d1, "vcp_version_xdf:     %s", format_vspec(dref->vcp_version_xdf) );
   rpt_vstring(d1, "vcp_version_cmdline: %s", format_vspec(dref->vcp_version_cmdline) );
   rpt_vstring(d1, "flags:               %s", interpret_dref_flags_t(dref->flags) );
   rpt_vstring(d1, "capabilities_string: %s", dref->capabilities_string);
   rpt_vstring(d1, "mmid:                %s", (dref->mmid) ? mmk_repr(*dref->mmid) : "NULL");
   rpt_vstring(d1, "dispno:              %d", dref->dispno);
   rpt_vstring(d1, "pedid:               %p", dref->pedid);
   report_parsed_edid(dref->pedid, /*verbose*/ false, depth+1);
   rpt_vstring(d1, "actual_display:   %p", dref->actual_display);
   rpt_vstring(d1, "actual_display_path: %s",
         (dref->actual_display_path) ? dpath_repr_t(dref->actual_display_path) : "NULL");
   rpt_vstring(d1, "detail:         %p", dref->detail);
   if (dref->io_path.io_mode == DDCA_IO_I2C && include_businfo) {
      I2C_Bus_Info * businfo = dref->detail;
      if (businfo) {
         i2c_dbgrpt_bus_info(businfo, true, d2);
      }
   }
   rpt_vstring(d1, "drm_connector:   %s", dref->drm_connector);
   rpt_vstring(d1, "drm_connector_found_by: %s",  drm_connector_found_by_name(dref->drm_connector_found_by));
   rpt_vstring(d1, "drm_connector_id: %d", dref->drm_connector_id);
   rpt_vstring(d1, "creation_timestamp: %s", formatted_time_t(dref->creation_timestamp));

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


// for use by DBGTRC_RET_STRUCT()
void dbgrpt_display_ref0(Display_Ref * dref, int depth) {
   dbgrpt_display_ref(dref, true, depth);
}


void dbgrpt_display_ref_summary(Display_Ref * dref, bool include_businfo, int depth) {
   bool debug = false;
   int d1 = depth+1;
   int d2 = depth+2;
   assert(dref);

   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dref=%s", dref_reprx_t(dref));
   rpt_vstring(depth, "%s", dref_reprx_t(dref));
   rpt_vstring(d1, "dref_id              %d", dref->dref_id);
   // rpt_vstring(d1, "io_path:          %s", dpath_repr_t(&(dref->io_path)));
   rpt_vstring(d1, "flags:               %s", interpret_dref_flags_t(dref->flags) );
   rpt_vstring(d1, "mmid:                %s", (dref->mmid) ? mmk_repr(*dref->mmid) : "NULL");
   rpt_vstring(d1, "dispno:              %d", dref->dispno);
   rpt_vstring(d1, "pedid:               %p", dref->pedid);
   // report_parsed_edid(dref->pedid, /*verbose*/ false, depth+1);

   rpt_vstring(d1, "detail:              %p", dref->detail);
   if (dref->io_path.io_mode == DDCA_IO_I2C && include_businfo) {
      I2C_Bus_Info * businfo = dref->detail;
      if (businfo) {
         i2c_dbgrpt_bus_info(businfo, false, d2);
      }
   }
   rpt_vstring(d1, "drm_connector:       %s", dref->drm_connector);
   rpt_vstring(d1, "drm_connector_id: %d", dref->drm_connector_id);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


/** Thread safe function that returns a short description of a #Display_Ref.
 *  The returned value is valid until the next call to this function on
 *  the current thread.
 *
 *  \param  dref  pointer to #Display_Ref
 *  \return short description
 */
char * dref_short_name_t(Display_Ref * dref) {
   return dpath_short_name_t(&dref->io_path);
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
   if (dref)
#ifdef WITH_ADDR
      g_snprintf(buf, 100, "Display_Ref[%d:%s @%p]", dref->dref_id, dpath_short_name_t(&dref->io_path), (void*)dref);
#else
   g_snprintf(buf, 100, "Display_Ref[%d:%s]", dref->dref_id, dpath_short_name_t(&dref->io_path));
#endif
   else
      strcpy(buf, "Display_Ref[NULL]");
   return buf;
}


/** Thread safe function that returns an extended string representation
 *  of a #Display_Ref, suitable for diagnostic messages.
 *  The representation includes the address of the #Display_Ref and
 *  an indication if the display reference is for a disconnected monitor.
 *
 *  The returned value is valid until the next call to this function on
 *  the current thread.
 *
 *  \param  dref  pointer to #Display_Ref
 *  \return string representation of #Display_Ref
 */
char * dref_reprx_t(Display_Ref * dref) {
   static GPrivate  dref_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dref_repr_key, 100);
   if (dref)
      g_snprintf(buf, 200, "Display_Ref[%s%d:%s @%p]",
            (dref->flags & DREF_REMOVED) ? "Disconnected: " : "",
            dref->dref_id,
            dpath_short_name_t(&dref->io_path),
            (void*) dref);

   else
      strcpy(buf, "Display_Ref[NULL]");
   return buf;
}

char * ddci_dref_repr_t(DDCA_Display_Ref * ddca_dref) {
   static GPrivate  dref_repr_key = G_PRIVATE_INIT(g_free);

   char * buf = get_thread_fixed_buffer(&dref_repr_key, 100);
#ifdef NUMERIC_DDCA_DISPLAY_REF
   g_snprintf(buf, 100, "DDCA_Display_Ref[%d]", GPOINTER_TO_INT(ddca_dref));
#else
   if (ddca_dref) {
      Display_Ref * dref = (Display_Ref*) ddca_dref;
#ifdef WITH_ADDR
      g_snprintf(buf, 100, "DDCA_Display_Ref[%s @%p]", dpath_short_name_t(&dref->io_path), (void*)dref);
#else
      g_snprintf(buf, 100, "DDCA_Display_Ref[%s]", dpath_short_name_t(&dref->io_path));
   }
#endif
   else
      strcpy(buf, "DDCA_Display_Ref[NULL]");
#endif
   return buf;
}


/** Locates the currently live Display_Ref for the specified bus.
 *  Discarded display references, i.e. ones marked removed (flag DREF_REMOVED)
 *  are ignored. There should be at most one non-removed Display_Ref.
 *
 *  @param  busno    I2C_Bus_Number
 *  @param  connector
 *  @param  ignore_invalid
 *  @return  display reference, NULL if no live reference exists
 */
Display_Ref * get_dref_by_busno_or_connector(
      int          busno,
      const char * connector,
      bool         ignore_invalid)
{
   ASSERT_IFF(busno >= 0, !connector);
   bool debug = false;
   debug = debug || debug_locks;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno = %d, connector = %s, ignore_invalid=%s",
                                       busno, connector, SBOOL(ignore_invalid));
   assert(all_display_refs);

   Display_Ref * result = NULL;
   int non_removed_ct = 0;
   uint64_t highest_non_removed_creation_timestamp = 0;
   // lock entire function on the extremely rare possibility that recovery
   // will mark a display ref removed
   g_mutex_lock(&all_display_refs_mutex);
   for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
      // If a display is repeatedly removed and added on a particular connector,
      // there will be multiple Display_Ref records.  All but one should already
      // be flagged DDCA_DISPLAY_REMOVED,
      // ?? and should not have a pointer to an I2C_Bus_Info struct.

      Display_Ref * cur_dref = g_ptr_array_index(all_display_refs, ndx);
      // DBGMSG("Checking dref %s", dref_repr_t(cur_dref));

      if (ignore_invalid && cur_dref->dispno <= 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_dref=%s@%p dispno < 0, Ignoring",
               dref_repr_t(cur_dref), cur_dref);
         continue;
      }

      // I2C_Bus_Info * businfo = (I2C_Bus_Info*) cur_dref->detail;
      // DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "DREF_REMOVED=%s, dref_detail=%p -> /dev/i2c-%d",
      //       sbool(cur_dref->flags&DREF_REMOVED), cur_dref->detail,  businfo->busno);

      if (ignore_invalid && (cur_dref->flags&DREF_REMOVED)) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_dref=%s@%p DREF_REMOVED set, Ignoring",
                dref_repr_t(cur_dref), cur_dref);
         continue;
      }

      if (cur_dref->io_path.io_mode != DDCA_IO_I2C) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "cur_dref=%s@%p io_mode != DDCA_IO_I2C, Ignoring",
                dref_repr_t(cur_dref), cur_dref);
         continue;
      }

      if (connector)   {   // consistency check
         I2C_Bus_Info * businfo = cur_dref->detail;
         if (businfo) {
            assert(streq(businfo->drm_connector_name, cur_dref->drm_connector));
         }
         else {
            SEVEREMSG("active display ref has no bus info");
         }
      }

      if ( (busno >= 0 && cur_dref->io_path.path.i2c_busno == busno) ||
           (connector  && streq(connector, cur_dref->drm_connector) ) )
      {
         // the match should only happen once, but count matches as check
         non_removed_ct++;
         if (cur_dref->creation_timestamp > highest_non_removed_creation_timestamp) {
            highest_non_removed_creation_timestamp = cur_dref->creation_timestamp;
            result = cur_dref;
         }
      }
   }
   // assert(non_removed_ct <= 1);
   if (non_removed_ct > 1) {
      if (!ignore_invalid) {
         // don't try to recover from this very very very rare case
         assert(non_removed_ct <= 1);
      }
      SEVEREMSG("Multiple non-removed displays on device %s detected. "
                "All but the most recent are being marked DDC_REMOVED",
                dpath_repr_t(&result->io_path));
      for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
         Display_Ref * cur_dref = g_ptr_array_index(all_display_refs, ndx);
         if (ignore_invalid && cur_dref->dispno <= 0)
            continue;
         if (ignore_invalid && (cur_dref->flags&DREF_REMOVED))
            continue;
         if (cur_dref->io_path.io_mode != DDCA_IO_I2C)
            continue;
         if ( (busno >= 0 && cur_dref->io_path.path.i2c_busno == busno) ||
              (connector  && streq(connector, cur_dref->drm_connector) ) )
         {
            if (cur_dref->creation_timestamp < highest_non_removed_creation_timestamp) {
               SEVEREMSG("Marking dref %s removed", dref_reprx_t(cur_dref));
               //ddc_mark_display_ref_removed(cur_dref);
               cur_dref->flags |= DREF_REMOVED;
            }
         }
      }
   }
   g_mutex_unlock(&all_display_refs_mutex);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: %p= %s", result, dref_repr_t(result));
   return result;
}

#ifdef UNUSED
Display_Ref *
ddc_get_display_ref_by_drm_connector(
      const char * connector_name,
      bool         ignore_invalid)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
         "connector_name=%s, ignore_invalid=%s", connector_name, sbool(ignore_invalid));
   Display_Ref * result = NULL;
   TRACED_ASSERT(all_display_refs);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "all_displays->len=%d", all_display_refs->len);
   for (int ndx = 0; ndx < all_display_refs->len; ndx++) {
      Display_Ref * cur = g_ptr_array_index(all_display_refs, ndx);
      // ddc_dbgrpt_display_ref(cur, 4);
      bool pass_filter = true;
      if (ignore_invalid) {
         pass_filter = (cur->dispno > 0 || !(cur->flags&DREF_REMOVED));
      }
      if (pass_filter) {
         if (cur->io_path.io_mode == DDCA_IO_I2C) {
            I2C_Bus_Info * businfo = cur->detail;
            if (!businfo) {
               SEVEREMSG("active display ref has no bus info");
               continue;
            }
            // TODO: handle drm_connector_name not yet checked
            if (businfo->drm_connector_name && streq(businfo->drm_connector_name,connector_name)) {
               result = cur;
               break;
            }
         }
      }
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s = %p", dref_repr_t(result), result);
   return result;
}
#endif


// *** Display_Handle ***

/** Creates a #Display_Handle for a #Display_Ref.
 *
 *  \param  fd    Linux file descriptor of open display
 *  \param  dref  pointer to #Display_Ref
 *  \return newly allocated #Display_Handle
 *
 *  \remark
 *  This functions handles the boilerplate of creating a #Display_Handle.
 */
Display_Handle * create_base_display_handle(int fd, Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "fd=%d, dref=%s", fd, dref_reprx_t(dref));
   if (debug)
      dbgrpt_display_ref(dref, false,  1);
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->fd = fd;
   dh->dref = dref;
   if (dref->io_path.io_mode == DDCA_IO_I2C) {
      dh->repr = g_strdup_printf("Display_Handle[i2c-%d: fd=%d]",
                          dh->dref->io_path.path.i2c_busno, dh->fd);
      dh->repr_p = g_strdup_printf("Display_Handle[i2c-%d: fd=%d @%p]",
                          dh->dref->io_path.path.i2c_busno, dh->fd, (void*)dh);
   }
#ifdef ENABLE_USB
   else if (dref->io_path.io_mode == DDCA_IO_USB) {
      dh->repr = g_strdup_printf(
                "Display_Handle[usb: %d:%d, %s/hiddev%d @%p]",
                // "Display_Handle[usb: %d:%d, %s/hiddev%d]",
                dh->dref->usb_bus, dh->dref->usb_device,
                usb_hiddev_directory(), dh->dref->io_path.path.hiddev_devno,
                (void*)dh);
   }
#endif
   else {
      // DDCA_IO_USB if !ENABLE_USB
      PROGRAM_LOGIC_ERROR("Unimplemented io_mode = %d", dref->io_path.io_mode);
      dbgrpt_display_ref(dref, false,  1);
      dh->repr = NULL;
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning %p", dh);
   return dh;
}


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
            rpt_vstring(d1, "fd:                  %d", dh->fd);
            rpt_vstring(d1, "busno:               %d", dh->dref->io_path.path.i2c_busno);
            break;
         case (DDCA_IO_USB):
            // rpt_vstring(d1, "ddc_io_mode = USB_IO");
            rpt_vstring(d1, "fd:                  %d", dh->fd);
            rpt_vstring(d1, "usb_bus:             %d", dh->dref->usb_bus);
            rpt_vstring(d1, "usb_device:          %d", dh->dref->usb_device);
            rpt_vstring(d1, "hiddev_device_name:  %s", dh->dref->usb_hiddev_name);
            break;
         }
         rpt_vstring(d1, "testing_unsupported_feature_active: %s",
                          sbool(dh->testing_unsupported_feature_active));
      }
      // rpt_vstring(d1, "vcp_version:         %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   }
}


/** Returns a string summarizing the specified #Display_Handle.
 *
 * \param  dh    display handle
 * \return  string representation of handle
 *
 * \remark
 * The value is calculated when the Display_Handle is created.
 */
char * dh_repr(Display_Handle * dh) {
   if (!dh)
      return "Display_Handle[NULL]";
   return dh->repr;
}


/** Returns a string summarizing the specified #Display_Handle,
 *  including its address.
 *
 * \param  dh    display handle
 * \return  string representation of handle
 *
 * \remark
 * The value is calculated when the Display_Handle is created.
 */
char * dh_repr_p(Display_Handle * dh) {
   if (!dh)
      return "Display_Handle[NULL]";
   return dh->repr_p;
}

/** Frees a #Display_Handle struct.
 *
 * \param  dh  display handle to free
 */
void   free_display_handle(Display_Handle * dh) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_BASE, "dh=%p -> %s", dh, dh_repr(dh));
   if (dh && memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) == 0) {
      dh->marker[3] = 'x';
      free(dh->repr);
      free(dh->repr_p);
      free(dh);
   }
   DBGTRC_DONE(debug, DDCA_TRC_BASE, "");
}


// *** Miscellaneous ***

#ifdef ENABLE_USB
/** Given a hiddev device name, e.g. /dev/usb/hiddev3,
 *  extract its number, e.g. 3.
 *
 *  \param   hiddev_name device name
 *  \return  device number, -1 if error
 */
int hiddev_name_to_number(const char * hiddev_name) {
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


#ifdef UNUSED
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
#endif
#endif


// globals
bool ddc_never_uses_null_response_for_unsupported = false;
// bool ddc_always_uses_null_response_for_unsupported = false;

Value_Name_Table dref_flags_table = {
      VN(DREF_DDC_COMMUNICATION_CHECKED),
      VN(DREF_DDC_COMMUNICATION_WORKING),
      VN(DREF_DDC_IS_MONITOR_CHECKED),
      VN(DREF_DDC_IS_MONITOR),

      VN(DREF_UNSUPPORTED_CHECKED),
      VN(DREF_DDC_USES_NULL_RESPONSE_FOR_UNSUPPORTED),
      VN(DREF_DDC_USES_MH_ML_SH_SL_ZERO_FOR_UNSUPPORTED),
      VN(DREF_DDC_USES_DDC_FLAG_FOR_UNSUPPORTED),
      VN(DREF_DDC_DOES_NOT_INDICATE_UNSUPPORTED),

      VN(DREF_TRANSIENT),
      VN(DREF_DYNAMIC_FEATURES_CHECKED),
      VN(DREF_OPEN),
      VN(DREF_DDC_BUSY),
      VN(DREF_REMOVED),
      VN(DREF_DDC_DISABLED),
      VN(DREF_DPMS_SUSPEND_STANDBY_OFF),
//    VN(CALLOPT_NONE),                // special entry
      VN_END
};


/** Interprets a **Dref_Flags** value as a printable string.
 *  The returned value is valid until the next call of this function in
 *  the current thread.
 *
 *  @param flags  value to interpret
 *
 *  @return interpreted value
 */
char * interpret_dref_flags_t(Dref_Flags flags) {
   static GPrivate  buf_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&buf_key, 300);

   char * buftemp = vnt_interpret_flags(flags, dref_flags_table, false, ", ");
   g_strlcpy(buf, buftemp, 300);    // n. this is a debug msg, truncation benign
   free(buftemp);

   return buf;
}


const char * watch_mode_name(DDC_Watch_Mode mode) {
   char * result = NULL;
   switch (mode) {
   case Watch_Mode_Poll:     result = "Watch_Mode_Poll";     break;
   case Watch_Mode_Xevent:   result = "Watch_Mode_Xevent";   break;
   case Watch_Mode_Udev:     result = "Watch_Mode_Udev";     break;
   case Watch_Mode_Dynamic:  result = "Watch_Mode_Dynamic";  break;
   }
   return result;
}


void free_bus_open_error(Bus_Open_Error * boe) {
   free(boe->detail);
   free(boe);
}


//
// Monitor models for which DDC is disabled
//

static GPtrArray  * ddc_disabled_table = NULL;


/** Adds a Monitor Model Id to the list of monitors for which DDC is disabled
 *
 *  @param  mmid  monitor model key string
 *  @return true  if mmid is defined, false if not
 *
 *  @remark
 *  If the **ddc_disabled_table** does not already exist, it is created.
 */
bool add_disabled_display(Monitor_Model_Key * p_mmk) {
   bool debug = false;
   char * repr = NULL;
   if (debug) {
      repr = mmk_repr(*p_mmk);
      DBG("Starting. mmk=|%s|", repr);
   }

   bool result = false;
   bool missing = true;
   if (p_mmk->defined) {  // if it's a valid monitor model id string
      DBGF(debug, "%s is valid:", repr);
      if (!ddc_disabled_table)
         ddc_disabled_table = g_ptr_array_new();
      // n. g_ptr_array_find_with_equal_func() requires glib 2.54
      for (int ndx = 0; ndx < ddc_disabled_table->len; ndx++) {
         Monitor_Model_Key* p = g_ptr_array_index(ddc_disabled_table, ndx);
         if (monitor_model_key_eq(*p_mmk, *p)) {
            missing = false;
            break;
         }
      }
      if (missing)
         g_ptr_array_add(ddc_disabled_table, p_mmk);
      result = true;
   }

   DBGF(debug, "Done. mmk=%s, missing=%s, returning: %s", repr,
               sbool(missing), sbool(result));
   return result;
}


bool add_disabled_mmk_by_string(const char * mmid) {
   bool result = false;
   Monitor_Model_Key* p_mmk = mmk_new_from_string(mmid);
   if (p_mmk) {
      add_disabled_display(p_mmk);
      result = true;
   }
   return result;
}


void dbgrpt_ddc_disabled_table(int depth) {
   const char * table_name = "ddc_disabled_table";
   GPtrArray* table = ddc_disabled_table;
   if (table) {
      if (table->len == 0)
         rpt_vstring(depth, "%s: empty", table_name);
      else {
         rpt_vstring(depth, "%s:", table_name);
         for (int ndx = 0; ndx < table->len; ndx++) {
             rpt_vstring(depth+1, mmk_repr(* (Monitor_Model_Key*) g_ptr_array_index(table, ndx)));
         }
      }
   }
   else {
      rpt_vstring(depth, "%s: NULL", table_name);
   }
}


/** Checks if DDC is disabled for a monitor model
 *
 *  @param mmk  monitor-model-id
 *  @return **true** if the display type is disabled, **false** if not
 */
bool is_disabled_mmk(Monitor_Model_Key mmk) {
   bool debug = false;
   DBGF(debug, "Starting. mmk=%s", mmk_repr(mmk));

  // dbgrpt_ddc_disabled_table(2);

   bool result = false;
   if (ddc_disabled_table) {
      for (int ndx = 0; ndx < ddc_disabled_table->len; ndx++) {
         Monitor_Model_Key* p = g_ptr_array_index(ddc_disabled_table, ndx);
         DBGF(debug, "Comparing vs p = %p -> %s", p, mmk_repr(*p));
         if (monitor_model_key_eq(mmk, *p)) {
            result = true;
            break;
         }
      }
   }

   DBGF(debug, "mmid=|%s|, returning: %s", mmk_repr(mmk), SBOOL(result));
   return result;
}


void init_displays() {
   RTTI_ADD_FUNC(copy_display_ref);
   RTTI_ADD_FUNC(create_base_display_handle);
   RTTI_ADD_FUNC(create_base_display_ref);
   RTTI_ADD_FUNC(create_bus_display_ref);
#ifdef ENABLE_USB
   RTTI_ADD_FUNC(create_usb_display_ref);
#endif
   RTTI_ADD_FUNC(dbgrpt_display_ref);
   RTTI_ADD_FUNC(free_display_handle);
   RTTI_ADD_FUNC(free_display_ref);
   RTTI_ADD_FUNC(dref_lock);
   RTTI_ADD_FUNC(dref_unlock);
   RTTI_ADD_FUNC(get_dref_by_busno_or_connector);

   init_published_dref_hash();
}


void terminate_displays() {
   g_hash_table_destroy(published_dref_hash);
}
