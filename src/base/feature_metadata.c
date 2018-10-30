/* @file feature_metadata.c
 * Consolidated data structures for internal representation of
 * display-specific feature metadata.
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE     // for asprintf in stdio.h

#include <assert.h>

#include <stdio.h>
#include <glib-2.0/glib.h>
#include <stddef.h>
#include <stdlib.h>

#include "util/report_util.h"

#include "base/displays.h"
#include "base/vcp_version.h"

#include "base/feature_metadata.h"




// copied from dynamic_features.c, make it thread safe

char * interpret_ddca_feature_flags2(DDCA_Version_Feature_Flags flags) {
   char * buffer = NULL;
   int rc = asprintf(&buffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
       flags & DDCA_RO             ? "Read-Only, "                   : "",
       flags & DDCA_WO             ? "Write-Only, "                  : "",
       flags & DDCA_RW             ? "Read-Write, "                  : "",
       flags & DDCA_STD_CONT       ? "Continuous (standard), "       : "",
       flags & DDCA_COMPLEX_CONT   ? "Continuous (complex), "        : "",
       flags & DDCA_SIMPLE_NC      ? "Non-Continuous (simple), "     : "",
       flags & DDCA_COMPLEX_NC     ? "Non-Continuous (complex), "    : "",
       flags & DDCA_NC_CONT        ? "Non-Continuous with continuous subrange, " :"",
       flags & DDCA_WO_NC          ? "Non-Continuous (write-only), " : "",
       flags & DDCA_NORMAL_TABLE   ? "Table (readable), "            : "",
       flags & DDCA_WO_TABLE       ? "Table (write-only), "          : "",
       flags & DDCA_DEPRECATED     ? "Deprecated, "                  : "",
       flags & DDCA_USER_DEFINED   ? "User-defined, "                : "",
       flags & DDCA_SYNTHETIC      ? "Synthesized, "                 : ""
       );
   assert(rc >= 0);   // real life code would check for malloc() failure in asprintf()
   // remove final comma and blank
   if (strlen(buffer) > 0)
      buffer[strlen(buffer)-2] = '\0';

   return buffer;
}

// if move to dir vcp or dynvcp can easily pick up function names

void
dbgrpt_display_feature_metadata(
      Display_Feature_Metadata * meta,
      int                        depth)
{
   rpt_vstring(depth, "Display_Feature_Metadata at %p", meta);
   if (meta) {
      assert(memcmp(meta->marker, DISPLAY_FEATURE_METADATA_MARKER, 4) == 0);
      int d1 = depth+1;
      rpt_vstring(d1, "display_ref:     %s", dref_repr_t(meta->display_ref));
      rpt_vstring(d1, "feature_code:    0x%02x", meta->feature_code);
      rpt_vstring(d1, "vcp_version:     %d.%d = %s",
                      meta->vcp_version.major, meta->vcp_version.minor, format_vspec(meta->vcp_version));
      rpt_vstring(d1, "feature_name:   %s", meta->feature_name);
      rpt_vstring(d1, "feature_desc:    %s", meta->feature_desc);
      char * s = interpret_ddca_feature_flags2(meta->flags);
      rpt_vstring(d1, "flags:           0x%04x = %s", meta->flags, s);
      free(s);
      rpt_vstring(d1, "nontable_formatter:           %p - %s",
                      meta->nontable_formatter,
                      get_func_name_by_addr(meta->nontable_formatter)) ;
      rpt_vstring(d1, "nontable_formatter_sl:        %p - %s",
                      meta->nontable_formatter_sl,
                      get_func_name_by_addr(meta->nontable_formatter_sl));
      rpt_vstring(d1, "nontable_formatter_universal: %p - %s",
                      meta->nontable_formatter_universal,
                      get_func_name_by_addr(meta->nontable_formatter_universal)) ;   // the future
      rpt_vstring(d1, "table_formatter:              %p - %s",
                      meta->table_formatter,
                      get_func_name_by_addr(meta->table_formatter));
   }

}


void
free_display_feature_metadata(
      Display_Feature_Metadata * meta)
{
   if (meta) {
      assert(memcmp(meta->marker, DISPLAY_FEATURE_METADATA_MARKER, 4) == 0);
      meta->marker[3] = 'x';
      free(meta->feature_name);
      free(meta->feature_desc);
      free(meta);
   }
}


Display_Feature_Metadata *
dfm_new(
      DDCA_Vcp_Feature_Code feature_code)
{
   Display_Feature_Metadata * result = calloc(1, sizeof(Display_Feature_Metadata));
   memcpy(result->marker, DISPLAY_FEATURE_METADATA_MARKER, 4);
   result->feature_code = feature_code;
   return result;
}


void dfm_set_feature_name(Display_Feature_Metadata * meta, const char * feature_name)
{
   assert(meta);
   if (meta->feature_name)
      free(meta->feature_name);
   meta->feature_name = strdup(feature_name);
}

void dfm_set_feature_desc(Display_Feature_Metadata * meta, const char * feature_desc)
{
   assert(meta);
   if (meta->feature_desc)
      free(meta->feature_desc);
   meta->feature_name = strdup(feature_desc);
}

DDCA_Feature_Metadata *
dfm_to_ddca_feature_metadata(
      Display_Feature_Metadata * dfm)
{
   DDCA_Feature_Metadata * ddca_meta = calloc(1, sizeof(DDCA_Feature_Metadata));
   memcpy(ddca_meta->marker, DDCA_FEATURE_METADATA_MARKER, 4);
   ddca_meta->feature_code = dfm->feature_code;
   ddca_meta->feature_flags = dfm->flags;
   ddca_meta->feature_name = (dfm->feature_name) ? strdup(dfm->feature_name) : NULL;
   ddca_meta->feature_desc = (dfm->feature_desc) ? strdup(dfm->feature_desc) : NULL;
   return ddca_meta;
}

Display_Feature_Metadata *
dfm_from_ddca_feature_metadata(
      DDCA_Feature_Metadata * ddca_meta)
{
   assert(ddca_meta);
   assert(memcmp(ddca_meta->marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0);


   Display_Feature_Metadata * dfm = calloc(1, sizeof(Display_Feature_Metadata));
   memcpy(dfm->marker, DISPLAY_FEATURE_METADATA_MARKER, 4);
   dfm->display_ref = NULL;
   dfm->feature_code = ddca_meta->feature_code;
   dfm->feature_desc = (ddca_meta->feature_desc) ? strdup(ddca_meta->feature_desc) : NULL;
   dfm->feature_name = (ddca_meta->feature_name) ? strdup(ddca_meta->feature_name) : NULL;
   dfm->flags = ddca_meta->feature_flags;
   dfm->nontable_formatter = NULL;
   dfm->nontable_formatter_sl = NULL;
   dfm->table_formatter = NULL;
   dfm->vcp_version =  DDCA_VSPEC_UNQUERIED;
   dfm->sl_values = ddca_meta->sl_values;      // OR DUPLICATE?
   return dfm;
}




#ifdef DVFI

DDCA_Version_Feature_Info *
dfm_to_ddca_version_feature_info(
      Display_Feature_Metadata * dfm)
{
   DDCA_Version_Feature_Info * vfi = calloc(1, sizeof(DDCA_Version_Feature_Info));
   memcpy(vfi->marker, VCP_VERSION_SPECIFIC_FEATURE_INFO_MARKER, 4);

   vfi->feature_name  = (dfm->feature_name) ? strdup(dfm->feature_name) : NULL;
   vfi->desc          = (dfm->feature_desc) ? strdup(dfm->feature_desc) : NULL;
   vfi->feature_code  = dfm->feature_code;
   vfi->feature_flags = dfm->flags;

   vfi->sl_values     = dfm->sl_values;    // NEED TO COPY?
   vfi->vspec         = dfm->vcp_version;
   // vfi->version_id = ??
   return vfi;
}
#endif

GHashTable * func_name_table = NULL;

void dbgrpt_func_name_table(int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Function name table at %p", func_name_table);
   GHashTableIter iter;
   gpointer key, value;
   g_hash_table_iter_init(&iter, func_name_table);
   while (g_hash_table_iter_next(&iter, &key, &value)) {
      rpt_vstring(d1, "%p: %s", key, value);
   }
}

char * get_func_name_by_addr(void * ptr) {
   char * result = "";
   if (ptr) {
      result = g_hash_table_lookup(func_name_table, ptr);
      if (!result)
         result = "<Not Found>";
   }

   return result;
}

void func_name_table_add(void * func_addr, char * func_name) {
   // assert(vcp_feature_codes_initialized);
   g_hash_table_insert(func_name_table, func_addr, func_name);
}



