/* displays.c
 *
 * Maintains list of all detected monitors.
 *
 *
 * Created on: Jul 21, 2014
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/string_util.h"
#include "util/report_util.h"

#include "base/edid.h"

#include "base/displays.h"


// *** DisplayIdentifier ***

static char * Display_Id_Type_Names[] = {
      "DISP_ID_BUSNO",
      "DISP_ID_ADL",
      "DISP_ID_MONSER",
      "DISP_ID_EDID",
      "DISP_ID_DISPNO"
};


char * display_id_type_name(Display_Id_Type val) {
   return Display_Id_Type_Names[val];
}


#ifdef REFERENCE
typedef
struct {
   char          marker[4];         // always "DPID"
   Display_Id_Type id_type;
   int           busno;
   int           iAdapterIndex;
   int           iDisplayIndex;
// char          mfg_id[EDID_MFG_ID_FIELD_SIZE];
   char          model_name[EDID_MODEL_NAME_FIELD_SIZE];
   char          serial_ascii[EDID_SERIAL_ASCII_FIELD_SIZE];
   Byte          edidbyes[128]
} Display_Identifier;
#endif


static
Display_Identifier* common_create_display_identifier(Display_Id_Type id_type) {
   Display_Identifier* pIdent = calloc(1, sizeof(Display_Identifier));
   memcpy(pIdent->marker, DISPLAY_IDENTIFIER_MARKER, 4);
   pIdent->id_type = id_type;
   pIdent->busno  = -1;
   pIdent->iAdapterIndex = -1;
   pIdent->iDisplayIndex = -1;
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
      Byte* edidbytes
      )
{
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_EDID);
   memcpy(pIdent->edidbytes, edidbytes, 128);
   return pIdent;
}

Display_Identifier* create_mon_ser_display_identifier(
      char* model_name,
      char* serial_ascii
      )
{
   assert(model_name && strlen(model_name) > 0 && strlen(model_name) < EDID_MODEL_NAME_FIELD_SIZE);
   assert(serial_ascii && strlen(serial_ascii) > 0 && strlen(serial_ascii) < EDID_SERIAL_ASCII_FIELD_SIZE);
   Display_Identifier* pIdent = common_create_display_identifier(DISP_ID_MONSER);
   strcpy(pIdent->model_name, model_name);
   strcpy(pIdent->serial_ascii, serial_ascii);
   return pIdent;
}


void report_display_identifier(Display_Identifier * pdid, int depth) {

   rpt_structure_loc("BasicStructureRef", pdid, depth );
   int d1 = depth+1;
   rpt_mapped_int("ddc_io_mode",   NULL, pdid->id_type, (Value_To_Name_Function) display_id_type_name, d1);
   rpt_int( "dispno",        NULL, pdid->dispno,        d1);
   rpt_int( "busno",         NULL, pdid->busno,         d1);
   rpt_int( "iAdapterIndex", NULL, pdid->iAdapterIndex, d1);
   rpt_int( "iDisplayIndex", NULL, pdid->iDisplayIndex, d1);
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


// *** DisplayRef ***

static char * DDC_IO_Mode_Names[] = {
      "DDC_IO_DEVI2C",
      "DDC_IO_ADL"
};

char * ddc_io_mode_name(DDC_IO_Mode val) {
   return DDC_IO_Mode_Names[val];
}

static const Version_Spec version_spec_unqueried = {0xff, 0xff};

bool is_version_unqueried(Version_Spec vspec) {
   return (vspec.major == 0xff && vspec.minor == 0xff);
}


// PROBLEM: bus display ref getting created some other way
Display_Ref * create_bus_display_ref(int busno) {
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->ddc_io_mode = DDC_IO_DEVI2C;
   dref->busno       = busno;
   dref->vcp_version = version_spec_unqueried;
   // DBGMSG("Done.  Constructed bus display ref: ");
   // report_display_ref(dref,0);
   return dref;
}

Display_Ref * create_adl_display_ref(int iAdapterIndex, int iDisplayIndex) {
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   memcpy(dref->marker, DISPLAY_REF_MARKER, 4);
   dref->ddc_io_mode   = DDC_IO_ADL;
   dref->iAdapterIndex = iAdapterIndex;
   dref->iDisplayIndex = iDisplayIndex;
   dref->vcp_version   = version_spec_unqueried;
   return dref;
}

Display_Ref * clone_display_ref(Display_Ref * old) {
   assert(old);
   Display_Ref * dref = calloc(1, sizeof(Display_Ref));
   // dref->ddc_io_mode = old->ddc_io_mode;
   // dref->busno         = old->busno;
   // dref->iAdapterIndex = old->iAdapterIndex;
   // dref->iDisplayIndex = old->iDisplayIndex;
   // DBGMSG("dref=%p, old=%p, len=%d  ", dref, old, (int) sizeof(BasicDisplayRef) );
   memcpy(dref, old, sizeof(Display_Ref));
   return dref;
}

void free_display_ref(Display_Ref * dref) {
   if (dref) {
      assert(memcmp(dref->marker, DISPLAY_REF_MARKER,4) == 0);
      dref->marker[3] = 'x';
      free(dref);
   }
}

bool dreq(Display_Ref* this, Display_Ref* that) {
   bool result = false;
   if (!this && !that)
      result = true;
   else if (this && that) {
      if (this->ddc_io_mode == that->ddc_io_mode) {
         if (this->ddc_io_mode == DDC_IO_DEVI2C)
            result = (this->busno == that->busno);
         else
            result = (this->iAdapterIndex == that->iAdapterIndex &&
                      this->iDisplayIndex == that->iDisplayIndex);
      }
   }
   return result;
}


void report_display_ref(Display_Ref * dref, int depth) {
   rpt_structure_loc("BasicStructureRef", dref, depth );
   int d1 = depth+1;
   rpt_mapped_int("ddc_io_mode", NULL, dref->ddc_io_mode, (Value_To_Name_Function) ddc_io_mode_name, d1);
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      rpt_int("busno", NULL, dref->busno, d1);
   }
   else {
      rpt_int("iAdapterIndex", NULL, dref->iAdapterIndex, d1);
      rpt_int("iDisplayIndex", NULL, dref->iDisplayIndex, d1);
   }
   // TODO: fix to use report function:
   printf("   vcp_version:  %d.%d\n", dref->vcp_version.major, dref->vcp_version.minor );
}

char * display_ref_short_name_r(Display_Ref * dref, char * buf, int bufsize) {
   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      snprintf(buf, bufsize, "bus /dev/i2c-%d", dref->busno);
   }
   else {
      snprintf(buf, bufsize, "adl display %d.%d", dref->iAdapterIndex, dref->iDisplayIndex);
   }
   return buf;
}

static char display_ref_short_name_buffer[100];

char * display_ref_short_name(Display_Ref * dref) {
   return display_ref_short_name_r(dref, display_ref_short_name_buffer, 100);
}



// *** DisplayHandle ***

Display_Handle * create_bus_display_handle(int fh, int busno) {
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->ddc_io_mode = DDC_IO_DEVI2C;
   dh->fh = fh;
   dh->busno = busno;
   dh->vcp_version = version_spec_unqueried;

   // report_display_handle(dh,__func__);
   return dh;
}

Display_Handle * create_adl_display_handle(int iAdapterIndex, int iDisplayIndex) {
   Display_Handle * dh = calloc(1, sizeof(Display_Handle));
   memcpy(dh->marker, DISPLAY_HANDLE_MARKER, 4);
   dh->ddc_io_mode = DDC_IO_ADL;
   dh->iAdapterIndex = iAdapterIndex;
   dh->iDisplayIndex = iDisplayIndex;
   dh->vcp_version = version_spec_unqueried;
   return dh;
}

Display_Handle * create_adl_display_handle_from_display_ref(Display_Ref * ref) {
   return create_adl_display_handle(ref->iAdapterIndex, ref->iDisplayIndex);
}

void report_display_handle(Display_Handle * dh, const char * msg) {
   if (msg)
      printf("%s", msg);
   printf("Display_Handle: %p\n", dh);
   if (dh) {
      if (memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0) {
         printf("  Invalid marker in struct: 0x%08x, |%.4s|\n", *dh->marker, (char *)dh->marker);
      }
      else {
         switch (dh->ddc_io_mode) {
         case (DDC_IO_DEVI2C):
            printf("  ddc_io_mode = DDC_IO_DEVI2C\n");
            printf("  fh:    %d\n", dh->fh);
            printf("  busno: %d\n", dh->busno);
            break;
         case (DDC_IO_ADL):
            printf("  ddc_io_mode = DDC_IO_ADL\n");
            printf("  iAdapterIndex:    %d\n", dh->iAdapterIndex);
            printf("  iDisplayIndex:    %d\n", dh->iDisplayIndex);
            break;
         default:
            PROGRAM_LOGIC_ERROR("Invalid ddc_io_mode: %d\n", dh->ddc_io_mode);
         }
      }
      printf("   vcp_version:     %d.%d\n", dh->vcp_version.major, dh->vcp_version.minor);
   }

}

static char dh_repr_buf[100];

char * display_handle_repr_r(Display_Handle * dref, char * buf, int bufsize) {
   assert(memcmp(dref->marker, DISPLAY_HANDLE_MARKER, 4) == 0);
   char * bufptr = dh_repr_buf;
   int    bufsz  = 100;
   if (buf) {
      bufptr = buf;
      bufsz  = bufsize;
   }

   if (dref->ddc_io_mode == DDC_IO_DEVI2C) {
      snprintf(bufptr, bufsz, "Display_Handle[i2c: fh=%d, busno=%d]", dref->fh, dref->busno);
   }
   else {
      snprintf(bufptr, bufsz, "Display_Handle[adl: display %d.%d]", dref->iAdapterIndex, dref->iDisplayIndex);
   }
   return bufptr;
}


char * display_handle_repr(Display_Handle * dh) {
   return display_handle_repr_r(dh,NULL,0);
}


void report_display_info(Display_Info * dinfo, int depth) {
   // TODO: implement depth
   printf("Display_Info at %p:\n", dinfo);
   if (dinfo) {
      printf("   dref=%p\n", dinfo->dref);
      if (dinfo->dref) {
         printf("      short name:   %s\n", display_ref_short_name(dinfo->dref));
      }
      printf("   edid=%p\n", dinfo->edid);
      if (dinfo->edid) {
         report_parsed_edid(dinfo->edid, false /* !verbose */, depth);
      }
   }
}

void report_display_info_list(Display_Info_List * pinfo_list, int depth) {
   // TODO: implement depth
   printf("Display_Info_List at %p\n", pinfo_list);
   if (pinfo_list) {
      printf("  Count:         %d\n", pinfo_list->ct);
      int ndx = 0;
      for (; ndx < pinfo_list->ct; ndx++) {
         Display_Info * dinfo = &pinfo_list->info_recs[ndx];
         report_display_info(dinfo, depth+1);
      }
   }
}


Video_Card_Info * create_video_card_info() {
   Video_Card_Info * card_info = calloc(1, sizeof(Video_Card_Info));
   memcpy(card_info->marker, VIDEO_CARD_INFO_MARKER, 4);
   return card_info;
}


