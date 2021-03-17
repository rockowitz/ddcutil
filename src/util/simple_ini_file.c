/** \file simple_ini_file.c
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

#include "simple_ini_file.h"

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
   if (!colon)
      colon = index(s,'=');
   if (colon) {
      char * untrimmed_key = substr(s, 0, colon-s);  // allocates untrimmed_key
      char * key = strtrim( untrimmed_key );         // allocates key
      for (char *p = key; *p; p++) {*p=tolower(*p);}
      // DBGMSF(debug, "untrimmed_key = |%s|, key = |%s|", untrimmed_key, key);
      char * s_end = s + strlen(s);
      char * v_start = colon+1;
      char * untrimmed_value = substr(v_start, 0, s_end-v_start); // allocates untrimmed_value
      char * value = strtrim( untrimmed_value);                   // allocates value
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


static
void emit_error_msg(char * msg, GPtrArray * errmsgs, bool verbose) {
   if (verbose)
      printf("%s\n", msg);

   if (errmsgs)
      g_ptr_array_add(errmsgs, msg);
   else
      free(msg);
}


/** Loads an INI style configuration file into a newly allocated #Parsed_Ini_File.
 *  Keys of the hash table in the struct have the form <segment name>/<key>.
 *
 * \param   config_file_name  file name
 * \param   errmsgs           if non-null, collects per-line error messages
 * \param   verbose           if true, write error messages to terminal
 * \param   parsed_ini_loc    where to return newly allocated parsed ini file
 * \retval  0                 success
 * \retval -ENOENT            configuration file not found
 * \retval -EBADMSG           errors parsing configuration file
 * \retval < 0                errors reading configuration file
 *
 * If the configuration file is not found (-ENOENT), or there are errors reading
 * or parsing the configuration file, *hash_table_loc is NULL.
 *
 * If errors occur reading or interpreting the file, messages will be added
 * to **errmsgs**.
 *
 * \remark
 * There's really no appropriate errno value for errors parsing the file,
 * which is a form of bad data.  EBADMSG has been hijacked for this purpose.
 */
int ini_file_load(
           const char *      ini_file_name,
           GPtrArray*        errmsgs,
           bool              verbose,
           Parsed_Ini_File** parsed_ini_loc)
{
   bool debug = false;
   if (debug)
      verbose = true;
   assert(ini_file_name);

   int result = 0;
   *parsed_ini_loc = NULL;

   char * cur_segment = NULL;
   GHashTable * ini_file_hash = NULL;

   GPtrArray * config_lines = g_ptr_array_new_with_free_func(free);
   int getlines_rc = file_getlines(ini_file_name, config_lines, verbose);
   if (getlines_rc < 0) {
      result = getlines_rc;
      if (getlines_rc != -ENOENT) {
         char * msg = g_strdup_printf("Error reading configuration file %s: %s",
               ini_file_name,
               strerror(-getlines_rc) );
         emit_error_msg(msg, errmsgs, verbose);
      }
   }  // error reading lines
   else {  //process the lines
      ini_file_hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
      int error_ct = 0;
      for (guint ndx = 0; ndx < config_lines->len; ndx++) {
         char * line = g_ptr_array_index(config_lines, ndx);
         if (debug)
            printf("(%s) Processing line %d: |%s|\n", __func__, ndx+1, line);
         char * trimmed = trim_in_place(line);
         // DBGMSF(debug, "line=%d. trimmed=|%s|", ndx+1, trimmed);

         char * seg_name;
         char * key;
         char * value;

         if (is_comment(trimmed)) {
         }

         else if (is_segment(trimmed, &seg_name)) {
            if (cur_segment)
               free(cur_segment);
            cur_segment = seg_name;
         }

         else if ( is_kv(trimmed, &key, &value) ) {
            if (cur_segment) {
               char * full_key = g_strdup_printf("%s/%s", cur_segment, key); // allocates full_key
               if (debug)
                  printf("(%s) Inserting %s -> %s\n", __func__, full_key, value);
               g_hash_table_insert(ini_file_hash, full_key, value);
            }
            else {
               if (debug)
                  printf("(%s) trimmed: |%s|\n", __func__, trimmed);
               char * msg = g_strdup_printf("Line %d: Invalid before section header: %s",
                                       ndx+1, trimmed);
               emit_error_msg(msg, errmsgs, verbose);
               error_ct++;
               free(value);
            }
            free(key);
         }

         else {
            char * msg = (cur_segment)
                         ? g_strdup_printf("Line %d: invalid: %s", ndx+1, trimmed)
                         : g_strdup_printf("Line %d: invalid before section header: %s",
                                           ndx+1, trimmed);
            emit_error_msg(msg, errmsgs, verbose);
            error_ct++;
         }
      } // for loop
      g_ptr_array_free(config_lines, true);
      if (cur_segment)
         free(cur_segment);
      if ( error_ct > 0 ) {
         result = -EBADMSG;
         g_hash_table_destroy(ini_file_hash);
         ini_file_hash = NULL;
      }
   } // process the lines

   if (debug) {
      if (errmsgs && errmsgs->len > 0) {
         for (guint ndx = 0; ndx < errmsgs->len; ndx++)
            printf("   %s\n", (char *) g_ptr_array_index(errmsgs, ndx));
      }
   }

   ASSERT_IFF(result==0, ini_file_hash);

   if (result == 0) {
      Parsed_Ini_File * ini_file = calloc(1, sizeof(Parsed_Ini_File));
      memcpy(ini_file->marker, PARSED_INI_FILE_MARKER, 4);
      ini_file->config_fn  = strdup(ini_file_name);
      ini_file->hash_table = ini_file_hash;
      *parsed_ini_loc = ini_file;
   }

   if (debug) {
      printf("(%s) Done.*parsed_ini_loc=%p, returning %d\n", __func__, *parsed_ini_loc, result);
      fflush(stdout);
   }
   ASSERT_IFF(result==0, *parsed_ini_loc);
   return result;
}


void ini_file_dump(Parsed_Ini_File * parsed_ini_file) {
   printf("(%s) Parsed_Ini_File at %p:\n", __func__, parsed_ini_file);

   if (parsed_ini_file) {
      assert(memcmp(parsed_ini_file->marker, PARSED_INI_FILE_MARKER, 4) == 0);
      printf("(%s) File name:   %s\n", __func__, parsed_ini_file->config_fn);
      if (parsed_ini_file->hash_table) {
         GHashTableIter iter;
         gpointer key, value;

         g_hash_table_iter_init(&iter, parsed_ini_file->hash_table);
         while (g_hash_table_iter_next(&iter, &key, &value)) {
            printf("   %s -> %s\n", (char *) key, (char *) value);
         }
      }
   }
}


char * ini_file_get_value(
      Parsed_Ini_File * parsed_ini_file,
      const char *      segment,
      const char *      id)
{
   bool debug = false;
   assert(parsed_ini_file);
   assert(memcmp(parsed_ini_file->marker, PARSED_INI_FILE_MARKER, 4) == 0);
   assert(segment);
   assert(id);

   char * result = NULL;
   if (parsed_ini_file->hash_table) {
      char * full_key = strlower(g_strdup_printf("%s/%s", segment, id));
      result = g_hash_table_lookup(parsed_ini_file->hash_table, full_key);
      free(full_key);
   }
   if (debug)
      printf("(%s) segment=%s, id=%s, returning: %s\n", __func__, segment, id, result);
   return result;
}


void ini_file_free(Parsed_Ini_File * parsed_ini_file) {
   if (parsed_ini_file) {
      assert(memcmp(parsed_ini_file->marker, PARSED_INI_FILE_MARKER, 4) == 0);
      if (parsed_ini_file->config_fn)
         free(parsed_ini_file->config_fn);
      if (parsed_ini_file->hash_table)
         g_hash_table_destroy(parsed_ini_file->hash_table);
      parsed_ini_file->marker[3] = 'x';
      free(parsed_ini_file);
   }
}

