/** @file persistent_capabilities.c */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stddef.h>
#include <strings.h>
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
#include "base/parms.h"
#include "base/rtti.h"

#include "persistent_capabilities.h"

static DDCA_Trace_Group TRACE_GROUP  = DDCA_TRC_VCP;

static bool capabilities_cache_enabled = false;   // default set in parser
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


/** Deletes the capabilities cache file if it exists. */
void
delete_capabilities_file() {
   bool debug = false;
   char * fn = capabilities_cache_file_name();
   if (fn && regular_file_exists(fn)) {
      DBGMSF(debug, "Deleting file: %s", fn);
      int rc = unlink(fn);
      if (rc < 0) {
         // should never occur
         SEVEREMSG("Unexpected error deleting file %s: %s",
               fn, strerror(errno));
         fprintf(fout(), "Unexpected error deleting file %s: %s\n",
                         fn, strerror(errno));
      }
   }
   else {
      DBGMSF(debug, "File does not exist: %s", fn);
   }
   free(fn);
}


/** If capabilities caching is enabled and the capabilities cache file
 *  exists, load the cache file.
 *
 *  @param capabilities_hash_loc points to in-memory capabilities hash table
 *  @return Error_Info struct if errors, NULL if no errors
 *
 *  If *capabilities_cache_loc != NULL, the capabilities file has already
 *  been loaded.  Do nothing.
 *
 *  Otherwise, creates a hash table and sets *capabilities_hash_loc.
 *  If capabilities caching enabled, attempt to load it from the cache file.
 */
static Error_Info *
load_persistent_capabilities_file(GHashTable** capabilities_hash_loc) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "capabilities_hash:");
   if ( IS_DBGTRC(debug, TRACE_GROUP) )
      dbgrpt_capabilities_hash0(2,NULL);

   Error_Info * errs = NULL;
   if (!*capabilities_hash_loc) {
      *capabilities_hash_loc = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
      if (capabilities_cache_enabled) {
         char * data_file_name = capabilities_cache_file_name();
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "data_file_name: %s", data_file_name);
         if (!data_file_name) {
            SEVEREMSG("Unable to determine capabilities cache file name");
            SYSLOG2(DDCA_SYSLOG_ERROR, "Unable to determine capabilities cache file name");
            errs = ERRINFO_NEW(-ENOENT, "Unable to determine capabilities cache file name");
            goto bye;
         }

         GPtrArray * linearray = g_ptr_array_new_with_free_func(g_free);
         errs = file_getlines_errinfo(data_file_name, linearray);
         free(data_file_name);
         if (!errs) {
            for (int ndx = 0; ndx < linearray->len; ndx++) {
               char * aline = strtrim(g_ptr_array_index(linearray, ndx));
               // DBGMSF(debug, "Processing line %d: %s", ndx+1, aline);
               if (strlen(aline) > 0 && aline[0] != '*' && aline[0] != '#') {
                  char * colon = strchr(aline, ':');
                  if (!colon) {
                     if (!errs)
                        errs = errinfo_new(DDCRC_BAD_DATA, __func__, "Invalid capabilities file");
                     errinfo_add_cause(errs,
                        errinfo_new(DDCRC_BAD_DATA, __func__, "Line %d, No colon in %s", ndx+1, aline));
                  }
                  else {
                     *colon = '\0';
                     g_hash_table_insert(capabilities_hash, g_strdup(aline), g_strdup(colon+1));
                  }
               }
               free(aline);
            }
            g_ptr_array_free(linearray, true);
         }
         if (errs) {
            delete_capabilities_file();
            g_hash_table_remove_all(*capabilities_hash_loc);
         }
      }
   }
   assert(*capabilities_hash_loc);

bye:
   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, errs, "capabilities_hash:");
   if (IS_DBGTRC(debug, TRACE_GROUP) )
      dbgrpt_capabilities_hash0(2, NULL);

   return errs;
}


static void save_persistent_capabilities_file() {
   bool debug = false;
   char * data_file_name = xdg_cache_home_file("ddcutil", "capabilities");
   DBGTRC_STARTING(debug, TRACE_GROUP, "capabilities_cache_enabled: %s, data_file_name=%s",
                              sbool(capabilities_cache_enabled), data_file_name);

   if (capabilities_cache_enabled) {
      if (!data_file_name) {
         SEVEREMSG("Cannot determine capabilities cache file name");
         SYSLOG2(DDCA_SYSLOG_ERROR, "Cannot determine capabilities cache file name");
         goto bye1;
      }
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
               SYSLOG2(DDCA_SYSLOG_ERROR, "Error writing to file %s:%s", data_file_name, strerror(errno) );
               break;
            }
         }
      }
      fclose(fp);
   }

bye:
   free(data_file_name);
bye1:
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


static inline bool generic_model_name(char * model_name) {
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


/** Some manufacturers use generic model names and don't set a product code.
 *  (LG is a particularly bad offender.) In that case a #DDCA_Monitor_Model_Key
 *  is unsuitable for identifying a capabilities string.
 *
 *  \param  mmk   #Monitor_Model_Key value to check
 *  \return true if **mmk** does not uniquely identify a monitor model,
 *         false if it does
 */
static inline bool non_unique_model_id(Monitor_Model_Key* mmk)
{
   return ( generic_model_name(mmk->model_name) &&
            ( mmk->product_code == 0 || mmk->product_code == 0x0101) );
}


// Publicly visible functions

/** Emit a debug report of the capabilities hash table
 *
 *  \param depth  logical indentation depth
 *  \param msg    if non-null, emit this message before the report
 *
 *  \remark
 *  This operation is protected by the persistent capabilities mutex
 */
void dbgrpt_capabilities_hash(int depth, const char * msg) {
   g_mutex_lock(&persistent_capabilities_mutex);
   dbgrpt_capabilities_hash0(depth, msg);
   g_mutex_unlock(&persistent_capabilities_mutex);
}


/** Returns the name of the file that stores persistent capabilities
 *
 *  \return name of file, normally $HOME/.cache/ddcutil/capabilities
 */
/* caller is responsible for freeing returned value */
char * capabilities_cache_file_name() {
   return xdg_cache_home_file("ddcutil", CAPABILITIES_CACHE_FILENAME);
}


/** Enable saving capabilities strings in a file.
 *
 *  \param  newval   true to enable, false to disable
 *  \return old setting
 */
bool enable_capabilities_cache(bool newval) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "newval=%s", sbool(newval));
   g_mutex_lock(&persistent_capabilities_mutex);
   bool old = capabilities_cache_enabled;
   if (newval) {
      capabilities_cache_enabled = true;
   }
   else {
      capabilities_cache_enabled = false;
      // if (capabilities_hash) {
      //    g_hash_table_destroy(capabilities_hash);
      //    capabilities_hash = NULL;
      // }
      // delete_capabilities_file();
   }
   g_mutex_unlock(&persistent_capabilities_mutex);
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, old, "capabilities_cache_enabled has been set = %s",
         sbool(capabilities_cache_enabled));
   return old;
}


/** Look up the capabilities string for a monitor model.
 *
 *  The returned value is owned by the persistent capabilities
 *  hash table and should not be freed.
 *
 *  \param mmk monitor model key
 *  \return capabilities string, NULL if not found or capabilities
 *          caching disabled
 *
 *  \remark
 *  Returns NULL in case of a potentially ambiguous Monitor_Model_Key
 */
char * get_persistent_capabilities(Monitor_Model_Key* mmk)
{
   assert(mmk);
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "mmk -> %s, capabilities_cache_enabled=%s",
         mmk_repr(*mmk), sbool(capabilities_cache_enabled));

   char * result = NULL;
   if (capabilities_cache_enabled) {
      if (non_unique_model_id(mmk)) {
         SYSLOG2(DDCA_SYSLOG_WARNING, "Non unique Monitor_Model_Key %s", mmk_repr(*mmk));
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Non unique Monitor_Model_Key. Returning NULL");
      }
      else {
         g_mutex_lock(&persistent_capabilities_mutex);
         Error_Info * load_errs = NULL;
         DBGMSF(debug, "capabilities_hash = %p", capabilities_hash);
         if (!capabilities_hash) {  // if not yet loaded
            load_errs = load_persistent_capabilities_file(&capabilities_hash);
            if (load_errs) {
               if (ERRINFO_STATUS(load_errs) == -ENOENT)
                  errinfo_free(load_errs);
               else {
                  char * data_file_name = capabilities_cache_file_name();
                  SEVEREMSG("Error(s) loading persistent capabilities file %s", data_file_name);
                  free(data_file_name);
                  for (int ndx =0; ndx < load_errs->cause_ct; ndx++) {
                     Error_Info * cur = load_errs->causes[ndx];
                     SEVEREMSG("  %s", cur->detail);
                  }
                  BASE_ERRINFO_FREE_WITH_REPORT(load_errs,false);
               }
            }
         }
         assert(capabilities_hash);
         char * mms = g_strdup(monitor_model_string(mmk));
         if (debug) {
            DBGMSG("Hash table before lookup:");
            dbgrpt_capabilities_hash0(2, NULL);
            DBGMSG("Looking for key: mms -> |%s|", mms);
         }
         result = g_hash_table_lookup (capabilities_hash, mms);
         free(mms);
      }
      g_mutex_unlock(&persistent_capabilities_mutex);
   }

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %s", result);
   return result;
}


/** Saves a capabilities string in the capabilities lookup table and,
 *  if persistent capabilities are enabled, writes the string and its
 *  key to the table on the file system.
 *
 *  \param mmk            monitor model key
 *  \param capabilities   capabilities string
 *
 *  \remark
 *  The string arguments are copied into the hash table.
 */
void set_persistent_capabilites(
        Monitor_Model_Key * mmk,
        const char *             capabilities)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "capabilities_cache_enabled=%s. mmk->%s, capabilities = %s",
          sbool(capabilities_cache_enabled), monitor_model_string(mmk), capabilities);

   g_mutex_lock(&persistent_capabilities_mutex);
   if (capabilities_cache_enabled) {
      if (non_unique_model_id(mmk)) {
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                         "Not saving capabilities for non-unique Monitor_Model_Key.");
         SYSLOG2(DDCA_SYSLOG_WARNING,
               "Not saving capabilities for non-unique Monitor_Model_Key: %s",
               monitor_model_string(mmk));
      }
      else {
         char * mms = g_strdup(monitor_model_string(mmk));
         g_hash_table_insert(capabilities_hash, mms, g_strdup(capabilities));
         if (debug || IS_TRACING())
            dbgrpt_capabilities_hash0(2, "Capabilities hash after insert and before saving");
         save_persistent_capabilities_file();
      }
   }
   g_mutex_unlock(&persistent_capabilities_mutex);

   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void terminate_persistent_capabilities() {
   if (capabilities_hash)
      g_hash_table_destroy(capabilities_hash);
}


void init_persistent_capabilities() {
   RTTI_ADD_FUNC(enable_capabilities_cache);
   RTTI_ADD_FUNC(load_persistent_capabilities_file);
   RTTI_ADD_FUNC(save_persistent_capabilities_file);
   RTTI_ADD_FUNC(get_persistent_capabilities);
   RTTI_ADD_FUNC(set_persistent_capabilites);
}

