/* displays.c
 *
 * Maintains list of all detected monitors.
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <config.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/string_util.h"
#include "util/report_util.h"

#include "base/displays.h"
#include "base/vcp_version.h"


// *** DisplayIdentifier ***

static char * Display_Id_Type_Names[] = {
      "DISP_ID_BUSNO",
      "DISP_ID_ADL",
      "DISP_ID_MONSER",
      "DISP_ID_EDID",
      "DISP_ID_DISPNO",
      "DISP_ID_USB"
};


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

Display_Identifier* create_dispno_display_identifier(int dispno) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_DISPNO);
   pIdent->dispno = dispno;
   return pIdent;
}


Display_Identifier* create_busno_display_identifier(int busno) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_BUSNO);
   pIdent->busno = busno;
   return pIdent;
}

Display_Identifier* create_adlno_display_identifier(
      int    iAdapterIndex,
      int    iDisplayIndex
      )
{
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_ADL);
   pIdent->iAdapterIndex = iAdapterIndex;
   pIdent->iDisplayIndex = iDisplayIndex;
   return pIdent;
}

Display_Identifier* create_edid_display_identifier(
      const Byte* edidbytes
      )
{
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_EDID);
   memcpy(pIdent->edidbytes, edidbytes, 128);
   return pIdent;
}

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

// #ifdef USE_USB
Display_Identifier* create_usb_display_identifier(int bus, int device) {
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_USB);
   pIdent->usb_bus = bus;
   pIdent->usb_device = device;
   return pIdent;

}
// #endif


void report_display_identifier(Display_Identifier * pdid, int depth) {

   rpt_structure_loc("BasicStructureRef", pdid, depth );
   int d1 = depth+1;
   rpt_mapped_int("ddc_io_mode",   NULL, pdid->id_type, (Value_To_Name_Function) display_id_type_name, d1);
   rpt_int( "dispno",        NULL, pdid->dispno,        d1);
   rpt_int( "busno",         NULL, pdid->busno,         d1);
   rpt_int( "iAdapterIndex", NULL, pdid->iAdapterIndex, d1);
   rpt_int( "iDisplayIndex", NULL, pdid->iDisplayIndex, d1);
   rpt_int( "usb_bus",       NULL, pdid->usb_bus,       d1);
   rpt_int( "usb_device",    NULL, pdid->usb_device,    d1);
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


void free_display_identifier(Display_Identifier * pdid) {
   // all variants use the same common data structure,
   // with no pointers to other memory
   assert( memcmp(pdid->marker, DISPLAY_IDENTIFIER_MARKER, 4) == 0);
   pdid->marker[3] = 'x';
   free(pdid);
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


// *** DisplayRef ***

static char * MCCS_IO_Mode_Names[] = {
      "DDC_IO_DEVI2C",
      "DDC_IO_ADL",
      "USB_IO"
};


char * mccs_io_mode_name(DDCA_IO_Mode val) {
   return (val >= 0 && val < 3)            // protect against bad arg
         ? MCCS_IO_Mode_Names[val]
         : NULL;
}


// PROBLEM: bus display ref getting created some other way
Display_Ref * create_bus_display_ref(int busno) {
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->io_mode = DDCA_IO_DEVI2C;
   dref->busno       = busno;
   dref->vcp_version = VCP_SPEC_UNQUERIED;
   // DBGMSG("Done.  Constructed bus display ref: ");
   // report_display_ref(dref,0);
   return dref;
}

Display_Ref * create_adl_display_ref(int iAdapterIndex, int iDisplayIndex) {
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->io_mode   = DDCA_IO_ADL;
   dref->iAdapterIndex = iAdapterIndex;
   dref->iDisplayIndex = iDisplayIndex;
   dref->vcp_version   = VCP_SPEC_UNQUERIED;
   return dref;
}

#ifdef USE_USB
Display_Ref * create_usb_display_ref(int usb_bus, int usb_device, char * hiddev_devname) {
   assert(hiddev_devname);
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->io_mode     = DDCA_IO_USB;
   dref->usb_bus     = usb_bus;
   dref->usb_device  = usb_device;
   dref->usb_hiddev_name = strdup(hiddev_devname);
   dref->vcp_version = VCP_SPEC_UNQUERIED;
   return dref;
}
#endif


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

void free_display_ref(Display_Ref * dref) {
   if (dref) {
      assert(memcmp(dref->marker, DISPLAY_REF_MARKER,4) == 0);
      dref->marker[3] = 'x';
      if (dref->usb_hiddev_name)
         free(dref->usb_hiddev_name);
      free(dref);
   }
}

bool dreq(Display_Ref* this, Display_Ref* that) {
   bool result = false;
   if (!this && !that)
      result = true;
   else if (this && that) {
      if (this->io_mode == that->io_mode) {
         switch (this->io_mode) {

         case DDCA_IO_DEVI2C:
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
}


void report_display_ref(Display_Ref * dref, int depth) {
   rpt_structure_loc("DisplayRef", dref, depth );
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_mapped_int("ddc_io_mode", NULL, dref->io_mode, (Value_To_Name_Function) mccs_io_mode_name, d1);

   switch (dref->io_mode) {

   case DDCA_IO_DEVI2C:
      rpt_int("busno", NULL, dref->busno, d1);
      break;

   case DDCA_IO_ADL:
      rpt_int("iAdapterIndex", NULL, dref->iAdapterIndex, d1);
      rpt_int("iDisplayIndex", NULL, dref->iDisplayIndex, d1);
      break;

   case DDCA_IO_USB:
      rpt_int("usb_bus",    NULL, dref->usb_bus,    d1);
      rpt_int("usb_device", NULL, dref->usb_device, d1);
      break;

   }

   // rpt_vstring(d1, "vcp_version:  %d.%d\n", dref->vcp_version.major, dref->vcp_version.minor );
   rpt_vstring(d1, "vcp_version:  %s", format_vspec(dref->vcp_version) );
   rpt_vstring(d1, "flags:        0x%02x", dref->flags);
   rpt_vstring(d2, "DDC communication checked:  %s", (dref->flags & DREF_DDC_COMMUNICATION_CHECKED) ? "true" : "false");
   rpt_vstring(d2, "DDC communication working:  %s", (dref->flags & DREF_DDC_COMMUNICATION_WORKING) ? "true" : "false");
}


char * dref_short_name_r(Display_Ref * dref, char * buf, int bufsize) {
   switch (dref->io_mode) {

   case DDCA_IO_DEVI2C:
      snprintf(buf, bufsize, "bus /dev/i2c-%d", dref->busno);
      break;

   case DDCA_IO_ADL:
      snprintf(buf, bufsize, "adl display %d.%d", dref->iAdapterIndex, dref->iDisplayIndex);
      break;

   case DDCA_IO_USB:
      snprintf(buf, bufsize, "usb %d:%d", dref->usb_bus, dref->usb_device);
      break;

   }
   return buf;
}


char * dref_short_name(Display_Ref * dref) {
   static char display_ref_short_name_buffer[100];
   return dref_short_name_r(dref, display_ref_short_name_buffer, 100);
}


char * dref_repr(Display_Ref * dref) {
   char buf[100];
   static char display_ref_short_id_buffer[100];
   snprintf(display_ref_short_id_buffer, 100,
            "Display_Ref[%s]", dref_short_name_r(dref, buf, 100) );
   return display_ref_short_id_buffer;
}


// *** Display_Handle ***

static Display_Handle * create_bus_display_handle_from_busno(int fh, int busno) {
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->io_mode = DDCA_IO_DEVI2C;
   dh->fh = fh;
   dh->busno = busno;
   dh->vcp_version = VCP_SPEC_UNQUERIED;

   // report_display_handle(dh,__func__);
   return dh;
}


// hacky implementation for transition
Display_Handle * create_bus_display_handle_from_display_ref(int fh, Display_Ref * dref) {
   assert(dref->io_mode == DDCA_IO_DEVI2C);
   Display_Handle * dh = create_bus_display_handle_from_busno(fh, dref->busno);
   dh->dref = dref;
   return dh;
}


static Display_Handle * create_adl_display_handle_from_adlno(int iAdapterIndex, int iDisplayIndex) {
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->io_mode = DDCA_IO_ADL;
   dh->iAdapterIndex = iAdapterIndex;
   dh->iDisplayIndex = iDisplayIndex;
   dh->vcp_version = VCP_SPEC_UNQUERIED;
   return dh;
}


// hacky implementation for transition
Display_Handle * create_adl_display_handle_from_display_ref(Display_Ref * dref) {
   assert(dref->io_mode == DDCA_IO_ADL);
   Display_Handle * dh = create_adl_display_handle_from_adlno(dref->iAdapterIndex, dref->iDisplayIndex);
   dh->dref = dref;
   return dh;
}

#ifdef USE_USB
Display_Handle * create_usb_display_handle_from_display_ref(int fh, Display_Ref * dref) {
   assert(dref->io_mode == DDCA_IO_USB);
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->io_mode = DDCA_IO_USB;
   dh->fh = fh;
   dh->dref = dref;
   dh->hiddev_device_name = dref->usb_hiddev_name;
   dh->usb_bus = dref->usb_bus;
   dh->usb_device = dref->usb_device;
   dh->vcp_version = VCP_SPEC_UNQUERIED;

   // report_display_handle(dh,__func__);
   return dh;
}
#endif

/* Reports the contents of a Display_Handle
 *
 * Arguments:
 *    dh       display handle
 *    msg      if non-null, output this string before the Display_Handle detail
 *    depth    logical indentation depth
 *
 * Returns: nothing
 */
void report_display_handle(Display_Handle * dh, const char * msg, int depth) {
   int d1 = depth+1;
   if (msg)
      rpt_vstring(depth, "%s", msg);
   rpt_vstring(d1, "Display_Handle: %p\n", dh);
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0) {
         rpt_vstring(d1, "Invalid marker in struct: 0x%08x, |%.4s|\n",
                         *dh->marker, (char *)dh->marker);
      }
      else {
         rpt_vstring(d1, "dref:                 %p", dh->dref);
         rpt_vstring(d1, "ddc_io_mode:          %s",  display_id_type_name(dh->io_mode) );
         switch (dh->io_mode) {
         case (DDCA_IO_DEVI2C):
            // rpt_vstring(d1, "ddc_io_mode = DDC_IO_DEVI2C");
            rpt_vstring(d1, "fh:                  %d", dh->fh);
            rpt_vstring(d1, "busno:               %d", dh->busno);
            break;
         case (DDCA_IO_ADL):
            // rpt_vstring(d1, "ddc_io_mode = DDC_IO_ADL");
            rpt_vstring(d1, "iAdapterIndex:       %d", dh->iAdapterIndex);
            rpt_vstring(d1, "iDisplayIndex:       %d", dh->iDisplayIndex);
            break;
         case (DDCA_IO_USB):
            // rpt_vstring(d1, "ddc_io_mode = USB_IO");
            rpt_vstring(d1, "fh:                  %d", dh->fh);
            rpt_vstring(d1, "usb_bus:             %d", dh->usb_bus);
            rpt_vstring(d1, "usb_device:          %d", dh->usb_device);
            rpt_vstring(d1, "hiddev_device_name:  %s", dh->hiddev_device_name);
            break;
         }
      }
      rpt_vstring(d1, "   vcp_version:         %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   }
}


/* Returns a string summarizing the specified Display_Handle, in
 * a buffer provided by the caller.
 *
 * Arguments:
 *   dh      display handle
 *   buf     pointer to buffer in which to return summary string
 *   bufsz   buffer size
 *
 * Returns:
 *   string representation of handle
 */
char * display_handle_repr_r(Display_Handle * dref, char * buf, int bufsz) {
   assert(memcmp(dref->marker, DISPLAY_HANDLE_MARKER, 4) == 0);
   assert(buf && bufsz);

   switch (dref->io_mode) {

   case DDCA_IO_DEVI2C:
      snprintf(buf, bufsz,
               "Display_Handle[i2c: fh=%d, busno=%d]",
               dref->fh, dref->busno);
      break;

   case DDCA_IO_ADL:
      snprintf(buf, bufsz,
               "Display_Handle[adl: display %d.%d]",
               dref->iAdapterIndex, dref->iDisplayIndex);
      break;

   case DDCA_IO_USB:
      snprintf(buf, bufsz,
               "Display_Handle[usb: %d:%d]",
               dref->usb_bus, dref->usb_device);
      break;
   }

   return buf;
}


/* Returns a string summarizing the specified Display_Handle.
 * The string is valid until the next call to this function.
 * Caller should NOT free this string.
 *
 * Arguments:
 *   dh    display handle
 *
 * Returns:
 *   string representation of handle
 */
char * display_handle_repr(Display_Handle * dh) {
   static char dh_repr_buf[100];
   return display_handle_repr_r(dh,dh_repr_buf,100);
}


// *** Display_Info ***


// n.b. frees the contents of dinfo, but not dinfo itself
void free_display_info(Display_Info * dinfo) {
   if (dinfo) {
      assert(memcmp(dinfo->marker, DISPLAY_INFO_MARKER, 4) == 0);
      dinfo->marker[3] = 'x';
      if (dinfo->dref) {
         // TODO: are there other references to dinfo->dref?   need to check
         // free_display_ref(dinfo->dref);
      }
      if (dinfo->edid) {
         // TODO: are there other references to dinfo->edid?   need to check
         // free_parsed_edid(dinfo->edid);
      }
      // free(dinfo);
   }
}


/* Outputs a debug report of a Display_Info struct.
 *
 * Arguments:
 *   dinfo   pointer to display_Info
 *   depth   logical indentation depth
 *
 * Returns:  nothing
 */
void report_display_info(Display_Info * dinfo, int depth) {
   const int d1 = depth+1;
   rpt_structure_loc("Display_Info", dinfo, depth);
   if (dinfo) {
      assert(memcmp(dinfo->marker, DISPLAY_INFO_MARKER, 4) == 0);
      rpt_vstring(d1, "dref:         %p  %s",
                      dinfo->dref,
                      (dinfo->dref) ? dref_short_name(dinfo->dref) : "");
      rpt_vstring(d1, "edid          %p", dinfo->edid);
      if (dinfo->edid) {
         report_parsed_edid(dinfo->edid, false /* !verbose */, d1);
      }
   }
}


void free_display_info_list(Display_Info_List * pinfo_list) {
   if (pinfo_list && pinfo_list->info_recs) {
      for (int ndx = 0; ndx < pinfo_list->ct; ndx++) {
         Display_Info * dinfo = &pinfo_list->info_recs[ndx];
         free_display_info(dinfo);
      }
   }
   free(pinfo_list);
}


/* Outputs a debug report of a Display_Info_List.
 *
 * Arguments:
 *   pinfo_list  pointer to display_Info_List
 *   depth       logical indentation depth
 *
 * Returns:  nothing
 */
void report_display_info_list(Display_Info_List * pinfo_list, int depth) {
   rpt_vstring(depth, "Display_Info_List at %p", pinfo_list);
   if (pinfo_list) {
      int d1 = depth+1;
      rpt_vstring(d1, "Count:         %d", pinfo_list->ct);
      int ndx = 0;
      for (; ndx < pinfo_list->ct; ndx++) {
         Display_Info * dinfo = &pinfo_list->info_recs[ndx];
         report_display_info(dinfo, d1);
      }
   }
}


// *** Miscellaneous ***

// Currently unused.  Needed for video card information retrieval
// currently defined only in ADL code.

/* Creates and initializes a Video_Card_Info struct.
 *
 * Arguments:  none
 *
 * Returns:    new instance
 */
Video_Card_Info * create_video_card_info() {
   Video_Card_Info * card_info = calloc(1, sizeof(Video_Card_Info));
   memcpy(card_info->marker, VIDEO_CARD_INFO_MARKER, 4);
   return card_info;
}

