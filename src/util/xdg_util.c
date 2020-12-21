/** \file xdg_util.c
 *  Implement XDG Base Directory Specification
 *
 *  See https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <unistd.h>
#include <glib-2.0/glib.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE
#include <stdio.h>

#include "file_util.h"
#include "xdg_util.h"


char * all_xdg_data_dirs()
{
   bool debug = true;
   char * home = getenv("HOME");
   char * xdg_data_home = getenv("XDG_DATA_HOME");
   if (xdg_data_home)
      xdg_data_home = strdup(xdg_data_home);
   if (!xdg_data_home && home)
      xdg_data_home = g_strdup_printf("%s/.local/share/", home);
   char * xdg_data_dirs = getenv("XDG_DATA_DIRS");
   if (!xdg_data_dirs)
      xdg_data_dirs = "usr/local/share/:/usr/share/";
   char * all_dirs = g_strdup_printf("%s:%s", xdg_data_home, xdg_data_dirs);
   if (debug)
      printf("(%s) Returning: %s\n", __func__, all_dirs);
   return all_dirs;
}


typedef struct {
   char * iter_start;
   char * iter_end;
} Iter_State;

void xdg_dirs_iter_init(char * dir_list, Iter_State * state) {
   state->iter_start = dir_list;
   state->iter_end = dir_list + strlen(dir_list);
}

char * xdg_dirs_iter_next(Iter_State * state) {
   bool debug = true;
   if (state->iter_start >= state->iter_end)
      return NULL;
   char * p = state->iter_start;
   while (p < state->iter_end && *p != ':')
      p++;
   if (p == state->iter_end)
      return NULL;
   int len = p - state->iter_start;
   char * buf = calloc(len + 1, 1);
   memcpy(buf, state->iter_start, len);
   state->iter_start = p + 1;
   if (debug)
      printf("(%s) Returning: %s\n", __func__, buf);
   return buf;
}


char * find_xdg_data_file(
      const char * application,
      const char * simple_fn)
{
   bool debug = true;
   if (debug)
      printf("(%s) Starting. application = %s, simple_fn=%s\n", __func__, application, simple_fn);
   Iter_State iter_state;
   char * dir_string = all_xdg_data_dirs();
   char * next_dir = NULL;
   char * fqfn = NULL;
   xdg_dirs_iter_init(dir_string, &iter_state);
   while ( !fqfn && (next_dir = xdg_dirs_iter_next(&iter_state)) ) {
      int lastndx = strlen(next_dir) - 1;
      if (next_dir[lastndx] == '/')
         next_dir[lastndx] = '\0';
      fqfn = g_strdup_printf("%s/%s/%s", next_dir, application, simple_fn);
      free(next_dir);
      if (debug)
         printf("(%s) Checking: %s\n", __func__, fqfn);
     // if (access(fqfn, R_OK)) {
      if (regular_file_exists(fqfn)) {
         continue;
      }
      free(fqfn);
      fqfn = NULL;
   }
   if (debug)
      printf("(%s) Done. Returning: %s\n", __func__, fqfn);
   return fqfn;
}

