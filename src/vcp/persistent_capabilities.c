/** \file persistent_capabilities.c */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stddef.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"

#include "util/error_info.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/xdg_util.h"

#include "base/core.h"
#include "base/monitor_model_key.h"

#include "persistent_capabilities.h"


static DDCA_Trace_Group TRACE_GROUP  = DDCA_TRC_VCP;

GHashTable *  all_capabilities = NULL;

Error_Info * load_persistent_capabilities_file()
{
   bool debug = false;
   DBGMSF(debug, "Starting.  Initial all_capabilities:");
   if (debug)
      dbgrpt_capabilities_hash(1,NULL);
   Error_Info * errs = NULL;
   if (all_capabilities) {
      g_hash_table_destroy(all_capabilities);
   }
   else {
      all_capabilities = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
   }

   char * data_file_name = xdg_data_home_file("ddcutil", "capabilities");
   DBGTRC(debug, TRACE_GROUP, "data_file_name: %s", data_file_name);
   GPtrArray * linearray = g_ptr_array_new_with_free_func(free);
   errs = file_getlines_errinfo(data_file_name, linearray);
   if (!errs) {
      for (int ndx = 0; ndx < linearray->len; ndx++) {
         char * aline = strtrim(g_ptr_array_index(linearray, ndx));
         // DBGMSF(debug, "Processing line %d: %s", ndx+1, aline);
         if (strlen(aline) > 0 && aline[0] != '*' && aline[0] != '#') {
            char * colon = index(aline, ':');
            if (!colon) {
               // DBGMSG("Colon not found");
               if (!errs)
                  errs = errinfo_new(DDCRC_BAD_DATA, __func__);
               errinfo_add_cause(errs,
                                 errinfo_new2(DDCRC_BAD_DATA, __func__, "Line %d, No colon in %s",
                                       ndx+1, aline));
            }
            else {
               // DBGMSG("Colon found");
               *colon = '\0';
               g_hash_table_insert(all_capabilities, strdup(aline), strdup(colon+1));
            }
         }
         free(aline);
      }
      g_ptr_array_free(linearray, true);
   }
   DBGMSF(debug, "Done. Final all_capabilities:");
   if (debug)
      dbgrpt_capabilities_hash(1, NULL);

   return errs;
}


void save_persistent_capabilities_file()
{
   bool debug = true;
   char * data_file_name = xdg_data_home_file("ddcutil", "capabilities");
   DBGTRC(debug, TRACE_GROUP, "Starting. data_file_name=%s", data_file_name);

   FILE * fp = fopen(data_file_name, "w");
   if (all_capabilities) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, all_capabilities);

      int ndx = 1;
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         // DBGMSF(debug, "Writing line %d: %s:%s", ndx+1,key, value);
         fprintf(fp, "%s:%s\n", (char *) key, (char*) value);
         ndx++;
      }
   }
   fclose(fp);
   free(data_file_name);
   DBGTRC(debug, TRACE_GROUP, "Done.");
}


char * get_persistent_capabilities(DDCA_Monitor_Model_Key* mmk)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting.  mmk -> %s", mmk_repr(*mmk));
    if (!all_capabilities) {
      Error_Info * errs = load_persistent_capabilities_file();
      if (errs) {
         if (ERRINFO_STATUS(errs) == -ENOENT)
            errinfo_free(errs);
         else
            ERRINFO_FREE_WITH_REPORT(errs,true);
      }
   }
   char * mms = monitor_model_string(mmk);

   DBGMSF(debug,"Hash table before lookup:");
   if (debug)
      dbgrpt_capabilities_hash(2, NULL);
   DBGMSF(debug, "Looking for key: mms -> |%s|", mms);

   char * result = g_hash_table_lookup (all_capabilities, mms);
   free(mms);

   DBGTRC(debug, TRACE_GROUP, "Returning: %s", result);
   return result;
}


void set_persistent_capabilites(
        DDCA_Monitor_Model_Key * mmk,
        const char *             capabilities)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. mmk->%s, capabilities = %s",
          monitor_model_string(mmk), capabilities);
   char * mms = monitor_model_string(mmk);
   g_hash_table_insert(all_capabilities, mms, strdup(capabilities));
   save_persistent_capabilities_file();
   DBGTRC(debug, TRACE_GROUP, "Done");
}


void dbgrpt_capabilities_hash(int depth, const char * msg) {
   if (msg)
      rpt_label(depth, msg);
   if (!all_capabilities)
      rpt_label(depth, "No all_capabilities hash table");
   else {
      // rpt_label(depth, "all_capabilities hash table:");
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, all_capabilities);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         rpt_vstring(depth+1, "%s -> %s", (char *) key, (char*) value);
      }
      rpt_nl();
   }
}



