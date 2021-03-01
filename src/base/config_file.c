// config_file.c

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#include <assert.h>
#include <ctype.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stddef.h>

#include "public/ddcutil_status_codes.h"

#include "util/error_info.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/xdg_util.h"

#include "base/core.h"

#include "base/config_file.h"


static
bool is_comment(char * s) {
   bool debug = false;
   bool result = false;
   if (strlen(s) == 0)
      result = true;
   else {
      char ch = s[0];
      // DBGMSF(debug, "ch=%c=%d, 0x%02x", ch, (int)ch, ch);
      // DBGMSF(debug, " %c=%d 0x%02x", ';', (int)';', ';');
      if (ch == ';' || ch == '*' || ch == '#') {
         // DBGMSF(debug, "WTF!");
         result = true;
      }
   }
   DBGMSF(debug, "s: %s, Returning %s", s, sbool(result));
   return result;
}

static
bool is_segment(char * s, char ** seg_name_loc) {
   bool debug = false;
   bool result = false;
   if (strlen(s) > 0 && *s == '[' && s[strlen(s)-1] == ']') {
      char * untrimmed = substr(s, 1, strlen(s)-2);
      DBGMSF(debug, "untrimmed=|%s|", untrimmed);
      char * seg_name = strtrim(untrimmed);
      for (char * p = seg_name; *p; p++) {*p = tolower(*p);}
      DBGMSF(debug, "seg_name=|%s|", seg_name);
      if (strlen(seg_name) > 0) {
         *seg_name_loc = seg_name;
         result = true;
      }
      else
         free(seg_name);
      free(untrimmed);
   }
   DBGMSF(debug, "s: %s, Returning %s", s, sbool(result));
   return result;
}

static
bool is_kv(char * s, char ** key_loc, char ** value_loc) {
   bool debug = false;
   DBGMSF(debug, "Starting. s->|%s|", s);
   bool result = false;
   char * colon = index(s,':');
   if (colon) {
      char * untrimmed_key = substr(s, 0, colon-s);
      char * key = strtrim( untrimmed_key );
      for (char *p = key; *p; p++) {*p=tolower(*p);}
      DBGMSF(debug, "untrimmed_key = |%s|, key = |%s|", untrimmed_key, key);
      char * s_end = s + strlen(s);
      char * v_start = colon+1;
      char * untrimmed_value = substr(v_start, 0, s_end-v_start);
      char * value = strtrim( untrimmed_value)  ;
      DBGMSF(debug, "untrimmed_value = |%s|, value = |%s|", untrimmed_value, value);
      DBGMSF(debug, "key=|%s|, value=|%s|", key, value);

      if (strlen(key) > 0) {
         *key_loc   = key;
         *value_loc = value;
         result = true;
      }
      else {
         free(key);
         free(value);
      }
      free(untrimmed_key);
      free(untrimmed_value);
   }
   DBGMSF(debug, "s: |%s|, Returning %s", s, sbool(result));
   return result;
}


static GHashTable * ini_file_hash = NULL;
static char * config_file_name = NULL;

char * get_config_file_name() {
   return config_file_name;
}

char * get_config_value(char * segment, char * id) {
   assert(segment);
   assert(id);
   bool debug = false;
   char * result = NULL;
   if (ini_file_hash) {
      char * full_key = g_strdup_printf("%s/%s", segment, id);
      result = g_hash_table_lookup(ini_file_hash, full_key);
   }
   DBGMSF(debug, "segment=%s, id=%s, returning: %s", segment, id, result);
   return result;
}


/** Loads the ddcutil configuration file, located as per the XDG specification
 *
 * \param verbose  if true, issue error meesages
 * \retval DDCRC_NOT_FOUND Configuration file not found
 * \retval DDCRC_BAD_DATA
 */
Error_Info *
load_configuration_file( bool verbose ) {
   bool debug = false;
   Error_Info * errs = NULL;

   char * cur_segment = NULL;
   assert(!ini_file_hash);
   char * config_fn = find_xdg_config_file("ddcutil", "ddcutilrc");
   if (!config_fn) {
      errs = errinfo_new2(DDCRC_NOT_FOUND, __func__,
                          "Configuration file not found: ddcutilrc");
   }
   else {
      config_file_name = config_fn;
      GPtrArray * config_lines = g_ptr_array_new_with_free_func(free);
      errs = file_getlines_errinfo(config_fn, config_lines);
      if (errs) {
         if (verbose) {
            fprintf(stderr, "Error reading configuration file %s: %s",
                            config_fn,
                            errinfo_summary(errs));
         }
      }  // error reading lines
      else {  //process the lines
         ini_file_hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
         GPtrArray * causes = g_ptr_array_new_with_free_func(free);
         for (int ndx = 0; ndx < config_lines->len; ndx++) {
            char * line = g_ptr_array_index(config_lines, ndx);
            DBGMSF(debug, "Processing line %d: |%s|", ndx+1, line);
            char * trimmed = trim_in_place(line);

            char * seg_name;
            char * key;
            char * value;

            if (is_comment(trimmed))
               continue;

            if (is_segment(trimmed, &seg_name)) {
               if (cur_segment)
                  free(cur_segment);
               cur_segment = seg_name;
               continue;
            }

            DBGMSF(debug, "before is_kv, line=%d. trimmed=|%s|", ndx+1, trimmed);
            if ( is_kv(trimmed, &key, &value) ) {
               if (cur_segment) {
                  char * full_key = g_strdup_printf("%s/%s", cur_segment, key);
                  DBGMSF(debug, "Inserting %s -> %s", full_key, value);
                  g_hash_table_insert(ini_file_hash, full_key, value);
                  continue;
               }
               else {
                  DBGMSF(debug, "trimmed: |%s|", trimmed);
                  if (verbose)
                     rpt_vstring(1, "Line %d invalid before section header", ndx+1);
                  Error_Info * cause = errinfo_new2(
                                          DDCRC_BAD_DATA,
                                          __func__,
                                          "Line %d invalid before section header: %s",
                                          ndx+1, trimmed);
                  g_ptr_array_add(causes, cause);
               }
               free(key);
               free(value);
               continue;
            }

            if (verbose)
               rpt_vstring(1, "Line %d invalidd: %s", ndx+1, trimmed);
            Error_Info * cause = errinfo_new2(
                                       DDCRC_BAD_DATA,
                                       __func__,
                                       "Line %d invalidd: %s",
                                       ndx+1, trimmed);
            g_ptr_array_add(causes, cause);

         } // for loop

         g_ptr_array_free(config_lines, true);
         if (causes->len > 0) {
            errs = errinfo_new_with_causes3(
                  DDCRC_BAD_DATA, (struct error_info **) causes->pdata, causes->len,
                  __func__, "Error(s) reading configuration file %s", config_fn);
            g_ptr_array_free(causes, false);
         }
      } // process the lines
   }  // config file exists
   if (cur_segment)
      free(cur_segment);

   // if (errs && verbose)
   //    errinfo_report(errs, 0);
   DBGMSF(debug, "Returning: %s", errinfo_summary(errs));
   return errs;
}


void dbgrpt_ini_hash(int depth) {
   rpt_label(depth, "ini file hash table:");

   if (ini_file_hash) {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init(&iter, ini_file_hash);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         rpt_vstring(depth+1, "%s -> %s", (char *) key, (char *) value);
      }
   }
   else
      rpt_label(depth, "Configuration file not loaded");
}

