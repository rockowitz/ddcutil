/** \file persistent_capabilities.c */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stddef.h>
#include <unistd.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"

#include "util/error_info.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/xdg_util.h"

#include "base/core.h"
#include "base/monitor_model_key.h"
#include "base/rtti.h"

#include "persistent_capabilities.h"

static DDCA_Trace_Group TRACE_GROUP  = DDCA_TRC_VCP;

static bool capabilities_cache_enabled = false;
static GHashTable *  capabilities_hash = NULL;

static GMutex persistent_capabilities_mutex;


static void dbgrpt_capabilities_hash0(int depth, const char * msg) {

   int d = depth;
   if (msg) {
      rpt_label(depth, msg);
      d = depth+1;
   }
   if (!capabilities_hash)
      rpt_label(d, "No capabilities hash table");
   else {
      if (g_hash_table_size(capabilities_hash) == 0)
         rpt_label(d, "Empty capabilities hash table");
      else {
         // rpt_label(depth, "capabilities_hash hash table:");
         GHashTableIter iter;
         gpointer key, value;
         g_hash_table_iter_init(&iter, capabilities_hash);
         while (g_hash_table_iter_next(&iter, &key, &value)) {
            rpt_vstring(d, "%s : (%p -> |%s|)", (char *) key, value, (char*) value);
         }
      }
      // rpt_nl();
   }

}


void dbgrpt_capabilities_hash(int depth, const char * msg) {
   g_mutex_lock(&persistent_capabilities_mutex);
   dbgrpt_capabilities_hash0(depth, msg);
   g_mutex_unlock(&persistent_capabilities_mutex);
}


/* caller is responsible for freeing returned value */
char * get_capabilities_cache_file_name() {
   return xdg_cache_home_file("ddcutil", "capabilities");
}


static void delete_capabilities_file() {
   bool debug = false;
   char * fn = xdg_cache_home_file("ddcutil", "capabilities");
   if (regular_file_exists(fn)) {
      DBGMSF(debug, "Deleting file: %s", fn);
      int rc = unlink(fn);
      if (rc < 0) {
         // should never occur
         fprintf(fout(), "Unexpected error deleting file %s: %s\n",
                         fn, strerror(errno));
      }
   }
   else {
      DBGMSF(debug, "File does not exist: %s", fn);
   }
   free(fn);
}


bool enable_capabilities_cache(bool onoff) {
   bool debug = false;
   DBGMSF(debug, "onoff=%s", sbool(onoff));
   g_mutex_lock(&persistent_capabilities_mutex);
   bool old = capabilities_cache_enabled;
   if (onoff) {
      capabilities_cache_enabled = true;
   }
   else {
      capabilities_cache_enabled = false;
      if (capabilities_hash) {
         g_hash_table_destroy(capabilities_hash);
         capabilities_hash = NULL;
      }
      delete_capabilities_file();
   }
   g_mutex_unlock(&persistent_capabilities_mutex);
   DBGMSF(debug, "capabilities_cache_enabled=%s. returning: %s",
         sbool(capabilities_cache_enabled), sbool(old));
   return old;
}


static Error_Info * load_persistent_capabilities_file()
{
   bool debug = false;
   if (debug || IS_TRACING()) {
      DBGTRC_STARTING(debug, TRACE_GROUP, "capabilities_hash:");
      dbgrpt_capabilities_hash0(2,NULL);
   }
   Error_Info * errs = NULL;
   if (capabilities_cache_enabled) {
      if (capabilities_hash) {
         g_hash_table_destroy(capabilities_hash);
      }
      // else {
         capabilities_hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
      // }

      char * data_file_name = get_capabilities_cache_file_name();
      // char * data_file_name = xdg_cache_home_file("ddcutil", "capabilities");
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "data_file_name: %s", data_file_name);
      GPtrArray * linearray = g_ptr_array_new_with_free_func(g_free);
      errs = file_getlines_errinfo(data_file_name, linearray);
      free(data_file_name);
      if (!errs) {
         for (int ndx = 0; ndx < linearray->len; ndx++) {
            char * aline = strtrim(g_ptr_array_index(linearray, ndx));
            // DBGMSF(debug, "Processing line %d: %s", ndx+1, aline);
            if (strlen(aline) > 0 && aline[0] != '*' && aline[0] != '#') {
               char * colon = index(aline, ':');
               if (!colon) {
                  if (!errs)
                     errs = errinfo_new(DDCRC_BAD_DATA, __func__);
                  errinfo_add_cause(errs, errinfo_new2(DDCRC_BAD_DATA, __func__,
                                                       "Line %d, No colon in %s",
                                                        ndx+1, aline));
               }
               else {
                  *colon = '\0';
                  g_hash_table_insert(capabilities_hash, strdup(aline), strdup(colon+1));
               }
            }
            free(aline);
         }
         g_ptr_array_free(linearray, true);
      }
   }
   else {
      if (capabilities_hash) {
         g_hash_table_destroy(capabilities_hash);
         capabilities_hash = NULL;
      }

      delete_capabilities_file();
   }

   if (debug || IS_TRACING()) {
      DBGTRC_DONE(debug, TRACE_GROUP, "capabilities_hash:");
      dbgrpt_capabilities_hash0(2, NULL);
   }

   return errs;
}


static void save_persistent_capabilities_file()
{
   bool debug = false;
   char * data_file_name = xdg_cache_home_file("ddcutil", "capabilities");
   DBGTRC_STARTING(debug, TRACE_GROUP, "capabilities_cache_enabled: %s, data_file_name=%s",
                              sbool(capabilities_cache_enabled), data_file_name);

   if (capabilities_cache_enabled) {
      FILE * fp = NULL;
      fopen_mkdir(data_file_name, "w", ferr(), &fp);
      if (!fp) {
         // SEVEREMSG("Error opening %s: %s", data_file_name, strerror(errno));   // handled by fopen_mdkr()
         goto bye;
      }
      if (capabilities_hash) {
         GHashTableIter iter;
         gpointer key, value;
         g_hash_table_iter_init(&iter, capabilities_hash);

         for (int line_ctr=1; g_hash_table_iter_next(&iter, &key, &value); line_ctr++) {
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Writing line %d: %s:%s", line_ctr, key, value);
            int ct = fprintf(fp, "%s:%s\n", (char *) key, (char*) value);
            if (ct < 0) {
               SEVEREMSG("Error writing to file %s:%s", data_file_name, strerror(errno) );
               break;
            }
         }
      }
      fclose(fp);
   }

bye:
   free(data_file_name);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


static inline bool
generic_model_name(char * model_name) {
   char * generic_names[] = {
         "LG IPS FULLHD",
         "LG UltraFine",
         "LG Ultrawide",
         "LG UltraWide",
         "Samsung Syncmaster"};
   int namect = ARRAY_SIZE(generic_names);
   bool result = false;
   for (int ndx = 0; ndx < namect; ndx++) {
      if ( streq(model_name, generic_names[ndx]) ) {
         result = true;
         break;
      }
   }
   return result;
}


static inline bool
non_unique_model_id(DDCA_Monitor_Model_Key* mmk)
{
   return ( generic_model_name(mmk->model_name) &&
            ( mmk->product_code == 0 || mmk->product_code == 0x0101) );
}


/** Looks up the capabilities string for a monitor model.
 *
 *  The returned value is owned by the persistent capabilities
 *  table and should not be freed.
 *
 *  \param mmk
 *  \return capabilities string, NULL if not found
 *
 */
char * get_persistent_capabilities(DDCA_Monitor_Model_Key* mmk)
{
   assert(mmk);
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "mmk -> %s", mmk_repr(*mmk));
   g_mutex_lock(&persistent_capabilities_mutex);

   char * result = NULL;
   if (non_unique_model_id(mmk)) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Non unique Monitor_Model_Key. Returning NULL");
      goto bye;
   }

   if (capabilities_cache_enabled) {
      if (!capabilities_hash) {  // if not yet loaded
         Error_Info * errs = load_persistent_capabilities_file();
         if (errs) {
            if (ERRINFO_STATUS(errs) == -ENOENT)
               errinfo_free(errs);
            else
               ERRINFO_FREE_WITH_REPORT(errs,true);
         }
      }

      if (mmk) {
         char * mms = strdup(monitor_model_string(mmk));

         if (debug) {
            DBGMSG("Hash table before lookup:");
            dbgrpt_capabilities_hash0(2, NULL);
            DBGMSG("Looking for key: mms -> |%s|", mms);
         }

         result = g_hash_table_lookup (capabilities_hash, mms);
         free(mms);
     }
   }

bye:
   g_mutex_unlock(&persistent_capabilities_mutex);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", result);
   return result;
}


/** Saves a capabilities string in the capabilities lookup table and,
 *  if persistent capabilities are enabled, writes the string and its
 *  key to the table on the file system.
 *
 *  \param mmk            monitor model id
 *  \param capabilities   capabilities string
 *
 *  \remark
 *  The string arguments are copied into the hash table.
 */
void set_persistent_capabilites(
        DDCA_Monitor_Model_Key * mmk,
        const char *             capabilities)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "capabilities_cache_enabled=%s. mmk->%s, capabilities = %s",
          sbool(capabilities_cache_enabled), monitor_model_string(mmk), capabilities);

   g_mutex_lock(&persistent_capabilities_mutex);
   if (capabilities_cache_enabled) {
      if (non_unique_model_id(mmk))
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                         "Not saving capabilities for non-unique Monitor_Model_Key.");
      else {
         char * mms = strdup(monitor_model_string(mmk));
         g_hash_table_insert(capabilities_hash, mms, strdup(capabilities));
         if (debug || IS_TRACING())
            dbgrpt_capabilities_hash0(2, "Capabilities hash after insert and before saving");
         save_persistent_capabilities_file();
      }
   }
   g_mutex_unlock(&persistent_capabilities_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void init_persistent_capabilities() {
   RTTI_ADD_FUNC(load_persistent_capabilities_file);
   RTTI_ADD_FUNC(save_persistent_capabilities_file);
   RTTI_ADD_FUNC(get_persistent_capabilities);
   RTTI_ADD_FUNC(set_persistent_capabilites);
}

