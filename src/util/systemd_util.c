/* systemd_util.c
 *
 * Created on: Nov 4, 2017
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

#define _GNU_SOURCE

#include <assert.h>
#include <glib-2.0/glib.h>
#include <systemd/sd-journal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <linux/uuid.h>

#include "file_util.h"
#include "string_util.h"

#include "systemd_util.h"


char * del_hyphens(const char * src) {
   char * result = calloc(1, strlen(src) + 1);
   int destndx = 0;
   for (int srcndx = 0; srcndx <= strlen(src); srcndx++) {   // n. <= to copy trailing null
     if (src[srcndx] != '-')
        result[destndx++] = src[srcndx];
   }
   return result;
}

// uint64_t
// char[16] :q
char * get_current_boot_id() {

   char* boot_id_s =  file_get_first_line("/proc/sys/kernel/random/boot_id", /*verbose*/ true);
   // printf("(%s) boot_id_s = %s\n", __func__, boot_id_s);
   char * boot_id_nohyphens = del_hyphens(boot_id_s);
   free(boot_id_s);
   return boot_id_nohyphens;
}


// Belongs in some more general file.   where?  string_util.c, data_structures.c?
// Idea: allow for option to treat terms as regular expressions
// convert parm ignore_case into a flags byte of options

bool apply_filter_terms(const char * text, char ** terms, bool ignore_case) {
   assert(text);
   bool debug = false;
   bool result = true;
   char ** term = NULL;
   if (terms) {
       //  printf("(%s) filter_terms:\n", __func__);
       //  ntsa_show(terms);
      result = false;
      term = terms;
      while (*term) {
         // printf("(%s) Comparing |%s|\n", __func__, *term);
         if (ignore_case) {
            if (strcasestr(text,*term)) {
               result = true;
               break;
            }
         }
         else {
            if (strstr(text, *term)) {
               result = true;
               break;
            }
         }
         term++;
      }

   }

   if (debug) {
      if (result) {
         printf("(%s) text=|%s|, term=|%s|, Returning:  true\n", __func__, text, *term);
      }
      else {
         printf("(%s) text=|%s|, Returning: false\n", __func__, text);
      }
   }

   return result;

}


GPtrArray * get_current_boot_messages(char ** filter_terms, bool ignore_case, int limit) {
   bool debug = false;
   if (debug) {
      if (filter_terms) {
         printf("(%s) filter_terms:\n", __func__);
         ntsa_show(filter_terms);
      }
   }

   sd_journal * j;
   int r = sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY);
   if (r < 0) {
      fprintf(stderr, "Failed to open journal: %s\n", strerror(-r));
      return NULL;
   }
   char * cur_boot_id = get_current_boot_id();

   char b0[50];
   snprintf(b0, 50, "_BOOT_ID=%s", cur_boot_id);
   sd_journal_add_match(j,b0, 0);

   GPtrArray * lines = g_ptr_array_new_full(1000, free);
   // int ct = 0;

   SD_JOURNAL_FOREACH(j) {
      const char *d;
      size_t l;

      r = sd_journal_get_data(j, "MESSAGE", (const void **)&d, &l);
      if (r < 0) {
         fprintf(stderr, "Failed to read message field: %s\n", strerror(-r));
         continue;
      }

      // printf("%.*s\n", (int) l, d);
      int prefix_size = strlen("MESSAGE=");
      int adj_size = l-prefix_size;
      // printf("l=%zd, prefix_size=%d, adj_size=%d\n", l,prefix_size,adj_size);
      char * s = malloc( (l-prefix_size)+1);
      memcpy(s,d+prefix_size,adj_size);
      s[l-prefix_size] = '\0';
      bool keep = true;
      if (filter_terms)
         keep = apply_filter_terms(s, filter_terms, ignore_case);
      if (keep) {
         g_ptr_array_add(lines, s);
         // printf("(%s) Copied line |%s|\n", __func__, s);
      }
      else {
         // printf("(%s) Rejected line |%s|\n", __func__, s);
         free(s);
      }
      // if (ct++ > 10)
      //    break;
   }
   sd_journal_close(j);

   // printf("(%s) Found %d lines\n", __func__, lines->len);

   return lines;
}



