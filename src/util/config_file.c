/** \file config_file.c
 *
 *  Reads an INI style configuration file
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#include "file_util_base.h"
#include "string_util.h"
#include "xdg_util.h"

#include "config_file.h"


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
   if (debug)
      printf("(%s) s: %s, Returning %s\n", __func__, s, SBOOL(result));
   return result;
}

static
bool is_segment(char * s, char ** seg_name_loc) {
   bool debug = false;
   bool result = false;
   if (strlen(s) > 0 && *s == '[' && s[strlen(s)-1] == ']') {
      char * untrimmed = substr(s, 1, strlen(s)-2);
      // DBGMSF(debug, "untrimmed=|%s|", untrimmed);
      char * seg_name = strtrim(untrimmed);
      for (char * p = seg_name; *p; p++) {*p = tolower(*p);}
      // DBGMSF(debug, "seg_name=|%s|", seg_name);
      if (strlen(seg_name) > 0) {
         *seg_name_loc = seg_name;
         result = true;
      }
      else
         free(seg_name);
      free(untrimmed);
   }
   if (debug)
      printf("(%s) s: %s, Returning %s\n", __func__, s, SBOOL(result));
   return result;
}

static
bool is_kv(char * s, char ** key_loc, char ** value_loc) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. s->|%s|\n", __func__, s);
   bool result = false;
   char * colon = index(s,':');
   if (colon) {
      char * untrimmed_key = substr(s, 0, colon-s);
      char * key = strtrim( untrimmed_key );
      for (char *p = key; *p; p++) {*p=tolower(*p);}
      // DBGMSF(debug, "untrimmed_key = |%s|, key = |%s|", untrimmed_key, key);
      char * s_end = s + strlen(s);
      char * v_start = colon+1;
      char * untrimmed_value = substr(v_start, 0, s_end-v_start);
      char * value = strtrim( untrimmed_value)  ;
      // DBGMSF(debug, "untrimmed_value = |%s|, value = |%s|", untrimmed_value, value);
      // DBGMSF(debug, "key=|%s|, value=|%s|", key, value);

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
   if (debug)
      printf("(%s) s: |%s|, Returning %s\n", __func__, s, SBOOL(result));
   return result;
}


char * get_config_value(GHashTable * ini_file_hash, const char * segment, const char * id) {
   bool debug = false;
   assert(segment);
   assert(id);
   char * result = NULL;
   if (ini_file_hash) {
      char * full_key = strlower(g_strdup_printf("%s/%s", segment, id));
      result = g_hash_table_lookup(ini_file_hash, full_key);
      free(full_key);
   }
   if (debug)
      printf("(%s) segment=%s, id=%s, returning: %s\n", __func__, segment, id, result);
   return result;
}

/** Loads an INI style configuration file into a newly allocated
 *  hash table.  Keys of the table have the form <segment name>/<key>.
 *
 * \param  config_file_name  file name
 * \param  hash_table_loc    where to return newly allocated hash table
 * \param  errmsgs           stores per-line error messages if non-null
 * \param  verbose           if true, write error messages to terminal
 * \retval  0                success
 * \retval -ENOENT           configuration file not found
 * \retval -EBADMSG          errors parsing configuration file
 * \retval < 0               errors reading configuration file
 *
 * If errors occur interpreting the file, **errmsgs** will be non-empty
 *
 * \remark
 * There's really no errno value for errors parsing the file, which is
 * a form of bad data.  EBADMSG has been hijacked for this purpose.
 */

int load_configuration_file(
      char *         config_file_name,
      GHashTable **  hash_table_loc,
      GPtrArray *    errmsgs,
      bool           verbose)
{
   bool debug = false;
   assert(config_file_name);

   int result = 0;
   char * cur_segment = NULL;
   GHashTable * ini_file_hash = NULL;

   GPtrArray * config_lines = g_ptr_array_new_with_free_func(free);
   int getlines_rc = file_getlines(config_file_name, config_lines, verbose);
   if (getlines_rc < 0) {
      result = getlines_rc;
      if (getlines_rc != -ENOENT) {
         char * msg = g_strdup_printf("Error reading configuration file %s: %s",
               config_file_name,
               strerror(-getlines_rc) );
         if (verbose)
            fprintf(stderr, "%s/n", msg);
         if (errmsgs)
            g_ptr_array_add(errmsgs, msg);
         else
            free(msg);
      }
   }  // error reading lines
   else {  //process the lines
      ini_file_hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
      *hash_table_loc = ini_file_hash;
      int error_ct = 0;
      for (int ndx = 0; ndx < config_lines->len; ndx++) {
            char * line = g_ptr_array_index(config_lines, ndx);
            if (debug)
               printf("(%s) Processing line %d: |%s|\n", __func__, ndx+1, line);
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

            // DBGMSF(debug, "before is_kv, line=%d. trimmed=|%s|", ndx+1, trimmed);
            if ( is_kv(trimmed, &key, &value) ) {
               if (cur_segment) {
                  char * full_key = g_strdup_printf("%s/%s", cur_segment, key);
                  if (debug)
                     printf("(%s) Inserting %s -> %s\n", __func__, full_key, value);
                  g_hash_table_insert(ini_file_hash, full_key, value);
                  continue;
               }
               else {
                  if (debug)
                     printf("(%s) trimmed: |%s|\n", __func__, trimmed);
                  char * msg = g_strdup_printf("Line %d invalid before section header: %s",
                                          ndx+1, trimmed);
                  error_ct++;
                  if (verbose)
                     printf("%s\n", msg);
                  if (errmsgs)
                     g_ptr_array_add(errmsgs, msg);
                  else
                     free(msg);
               }
               free(key);
               free(value);
               continue;
            }

            char * msg = (cur_segment)
                            ? g_strdup_printf("Line %d invalid: %s", ndx+1, trimmed)
                            : g_strdup_printf("Line %d invalid before section header: %s",
                                              ndx+1, trimmed);
            error_ct++;
            if (verbose)
               printf("%s\n", msg);
            if (errmsgs)
               g_ptr_array_add(errmsgs, msg);
            else
               free(msg);
      } // for loop
      g_ptr_array_free(config_lines, true);
      if (cur_segment)
         free(cur_segment);
      if ( error_ct > 0 )
         result = -EBADMSG;
   } // process the lines

   if (debug) {
      if (errmsgs && errmsgs->len > 0) {
         for (int ndx = 0; ndx < errmsgs->len; ndx++)
            printf("   %s\n", (char *) g_ptr_array_index(errmsgs, ndx));
      }
      printf("(%s) Returning: %d\n", __func__, result);
   }
   return result;
}


void dump_ini_hash(GHashTable * ini_file_hash) {
   printf("(%s) ini file hash table:\n", __func__);

   if (ini_file_hash) {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init(&iter, ini_file_hash);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         printf("   %s -> %s\n", (char *) key, (char *) value);
      }
   }
   else
      printf("Configuration file not loaded\n");
}

