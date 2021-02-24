/** \file xdg_util.c
 *  Implement XDG Base Directory Specification
 *
 *  See https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

// Copyright (C) 2020-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <glib-2.0/glib.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <stdio.h>

#include "file_util.h"
#include "report_util.h"
#include "xdg_util.h"



/** Returns the name of the base data, configuration, or cache, directory.
 *  First the specified environment variable is checked.
 *  If no value is found the name is constructed from $HOME and
 *  the specified sub-directory.
 */
static char *
xdg_home_dir(
      const char * envvar_name,
      const char * home_subdir_name)
{
   bool debug = false;

   char * xdg_home = getenv(envvar_name);  // do not free
   if (xdg_home && strlen(xdg_home) > 0)
      xdg_home = strdup(xdg_home);
   else {
      char * home = getenv("HOME");
      if (home && strlen(home) > 0)
         xdg_home = g_strdup_printf("%s/%s/", home, home_subdir_name);
      else
         xdg_home = NULL;
   }
   if (debug)
      printf("(%s) Returning: %s\n", __func__, xdg_home);
   return xdg_home;
}

/** Returns the name of the xdg base directory for data files */
char * xdg_data_home_dir() {
   bool debug = false;
   char * result = xdg_home_dir("XDG_DATA_HOME", ".local/share");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

/** Returns the name of the xdg base directory for configuration files */
char * xdg_config_home_dir() {
   bool debug = false;
   char * result = xdg_home_dir("XDG_CONFIG_HOME", ".config");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

/** Returns the name of the xdg base directory for cache files */
char * xdg_cache_home_dir() {
   bool debug = false;
   char * result = xdg_home_dir("XDG_CACHE_HOME", ".cache");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


static char *
xdg_dirs(
      const char * envvar_name,
      const char * default_dirs)
{
   bool debug = false;

   char * xdg_dirs = getenv(envvar_name);  // do not free
   if (xdg_dirs && strlen(xdg_dirs) > 0)
      xdg_dirs = strdup(xdg_dirs);
   else {
      xdg_dirs = strdup(default_dirs);
   }
   if (debug)
      printf("(%s) Returning: %s\n", __func__, xdg_dirs);
   assert(xdg_dirs);
   return xdg_dirs;
}


char * xdg_data_dirs() {
   return xdg_dirs("XDG_DATA_DIRS",  "/usr/local/share/:/usr/share");
}

char * xdg_config_dirs() {
   return xdg_dirs("XDG_CONFIG_DIRS",  "etc/xdg"); }




#ifdef OLD

// Returns either

char * all_xdg_path_dirs(
      const char * envvar_xdg_home_dir,
      const char * default_xdg_home_dir_part,
      const char * envvar_xdg_dirs,
      const char * default_xdg_dirs)
{
   bool debug = false;
   if (debug) {
      printf("(%s) envvar_xdg_home_dir=%s, default_xdg_home_dir_part=%s\n ",
            __func__, envvar_xdg_home_dir, default_xdg_home_dir_part);
      printf("(%s) envvars_xdg_dirs=%s, default_xdg_dirs=%s\n",
            __func__, envvar_xdg_dirs, default_xdg_dirs);
   }
   char * home = getenv("HOME");
   char * xdg_home_dir = getenv(envvar_xdg_home_dir); // e.g. "XDG_DATA_HOME"
   if (xdg_home_dir)/** Finds a file in the application sub-directory of a base XDG directory. */
      xdg_home_dir = strdup(xdg_home_dir);
    if (!xdg_home_dir && home)
       xdg_home_dir = g_strdup_printf("%s/%s/", home, default_xdg_home_dir_part); // $HOME, ./local/share
    char * xdg_data_dirs = getenv(envvar_xdg_dirs);
    if (!xdg_data_dirs)
       xdg_data_dirs = strdup(default_xdg_dirs);   // e.g "usr/local/share/:/usr/share/";
    char * all_dirs = g_strdup_printf("%s:%s", xdg_home_dir, xdg_data_dirs);
    free(xdg_home_dir);
    free(xdg_data_dirs);
    if (debug)
       printf("(%s) Returning: %s\n", __func__, all_dirs);
    return all_dirs;
 }
#endif


char * xdg_data_path() {
   bool debug = false;
   char * result = NULL;
   char * home_dir = xdg_data_home_dir();
   char * dirs     = xdg_data_dirs();
   assert(dirs);
   if (home_dir) {
      result = g_strdup_printf("%s:%s", home_dir, dirs);
      free(home_dir);
   }
   else
      result = dirs;
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

char * xdg_config_path() {
   bool debug = false;
   char * result = NULL;
   char * home_dir = xdg_config_home_dir();
   char * dirs     = xdg_config_dirs();
   assert(dirs);
   if (home_dir) {
      result = g_strdup_printf("%s:%s", home_dir, dirs);
      free(home_dir);
   }
   else
      result = dirs;
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

char * xdg_cache_path() {
   return xdg_cache_home_dir();
}

/** Returns the fully qualified name of a file in the application
 *  sub-directory of $XDG_DATA_HOME.
 *  Does not check for the file's existence
 */
char * xdg_data_home_file(const char * application, const char * simple_fn)
{
   bool debug = false;
   char * result = NULL;
   char * dir = xdg_data_home_dir();
   if (dir && strlen(dir) > 0)
      result = g_strdup_printf("%s%s/%s", dir, application, simple_fn);
   free(dir);
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


/** Returns the fully qualified name of a file in the application
 *  sub-directory of $XDG_CONFIG_HOME.
 *  Does not check for the file's existence
 */
char * xdg_config_home_file(const char * application, const char * simple_fn)
{
   bool debug = false;
   char * result = NULL;
   char * dir = xdg_config_home_dir();
   if (dir && strlen(dir) > 0)
      result = g_strdup_printf("%s%s/%s", dir, application, simple_fn);
   free(dir);
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

/** Returns the fully qualified name of a file in the application
 *  sub-directory of $XDG_CACHE_HOME.
 *  Does not check for the file's existence
 */
char * xdg_cache_home_file(const char * application, const char * simple_fn)
{
   bool debug = false;
   char * result = NULL;
   char * dir = xdg_cache_home_dir();
   if (dir && strlen(dir) > 0)
      result = g_strdup_printf("%s%s/%s", dir, application, simple_fn);
   free(dir);
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


#ifdef OLD
   /** Finds a file in the application sub-directory of a base XDG directory. */
   static char *
   find_xdg_home_file(
         const char * dir,
         const char * application,
         const char * simple_fn)
   {
      assert(dir);
      assert(application);
      assert(simple_fn);

      bool debug = true;
      char * result = NULL;
      char * fqfn = g_strdup_printf("%s/%s/%s", dir, application, simple_fn);
      if (debug)
         printf("(%s) Checking: %s\n", __func__, fqfn);
     // if (access(fqfn, R_OK)) {
      if (regular_file_exists(fqfn)) {
         result = fqfn;
      }
      else
         free(fqfn);
      if (debug)
         printf("%s) dir=%s. application=%s, simple_fn=%s, returning: %s\n",
                __func__, dir, application, simple_fn, result);
      return result;
   }
#endif



#ifdef OLD
/** Returns a string containing the XDG_DATA_HOME directory, followed by the
 *  XDG_DAT_DIRS directories.
 */
char * xdg_data_path()
{
   bool debug = false;
   char * result = all_xdg_path_dirs(
                         "XDG_DATA_HOME",
                         "/.local/share/",
                         "XDG_DATA_DIRS",
                         "/usr/local/share/:/usr/share/");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


/** Returns a string containing the XDG_CONFIG_HOME directory, followed by the
 *  XDG_CONFIG_DIRS directories.
 */
char * xdg_config_path()
{
   bool debug = false;
   char * result = all_xdg_path_dirs(
                         "XDG_CONFIG_HOME",
                         "/.config/",
                         "XDG_CONFIG_DIRS",
                         "/etc/xdg/");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

#endif



typedef struct {
   char * iter_start;
   char * iter_end;
} Iter_State;

static void
xdg_dirs_iter_init(char * dir_list, Iter_State * state) {
   state->iter_start = dir_list;  // to avoid const warnings
   state->iter_end = state->iter_start + strlen(dir_list);
}

static char *
xdg_dirs_iter_next(Iter_State * state) {
   bool debug = false;
   if (state->iter_start >= state->iter_end)
      return NULL;
   char * p = state->iter_start;
   while (p < state->iter_end && *p != ':')
      p++;
   if (p == state->iter_end) {
      return NULL;
   }
   int len = p - state->iter_start;
   char * buf = calloc(len + 1, 1);
   memcpy(buf, state->iter_start, len);
   state->iter_start = p + 1;
   if (debug)
      printf("(%s) Returning: %s\n", __func__, buf);
   return buf;
}


/** Looks for a file first in the $XDG_DATA_HOME directory,
 *  the in the direct $XDG_DATA_DIRS directories.
 *  Returns fqfn, or NULL if not found.
 */

#ifdef OLD
char * find_xdg_data_file(
      const char * application,
      const char * simple_fn)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. application = %s, simple_fn=%s\n", __func__, application, simple_fn);
   Iter_State iter_state;
   char * dir_string = xdg_data_path();
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
   free(dir_string);
   if (debug)
      printf("(%s) Done. Returning: %s\n", __func__, fqfn);
   return fqfn;
}
#endif

static char *
find_xdg_path_file(
      const char * path,
      const char * application,
      const char * simple_fn)
{
   bool debug = false;
   if (debug) {
      printf("(%s) Starting. application = %s, simple_fn=%s\n", __func__, application, simple_fn);
      printf("(%s) Starting. path=%s\n", __func__, path);
   }
   if (!path)
      return NULL;

   Iter_State iter_state;
   char * next_dir = NULL;
   char * fqfn = NULL;
   xdg_dirs_iter_init(strdup(path), &iter_state);
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


/* Searches $XDG_DATA_HOME and then $XDG_DATA_DIRS for
 * a specified file in a particular application sub-directory.
 */
char *
find_xdg_data_file(
      const char * application,
      const char * simple_fn)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. application=%s, simple_fn=%s\n",
              __func__, application, simple_fn);
   char * result = NULL;
   char * path = xdg_data_path();
   result = find_xdg_path_file(
              path,
              application,
              simple_fn);
   free(path);
   if (debug)
      printf("(%s) Done.    Returning: %s\n",
              __func__, result);
   return result;
}


/* Searches $XDG_CONFIG_HOME and then $XDG_CONFIG_DIRS for
 * a specified file in a particular application sub-directory.
 */
char * find_xdg_config_file(
      const char * application,
      const char * simple_fn)
{
   bool debug = false;
   char * result = NULL;
   char * path = xdg_config_path();
   result = find_xdg_path_file(
                path,
                application,
                simple_fn);
   free(path);
   if (debug)
      printf("(%s) application=%s, simple_fn=%s, returning: %s\n",
              __func__, application, simple_fn, result);
   return result;

}

char * find_xdg_cache_file(
      const char * application,
      const char * simple_fn)
{
   bool debug = false;
   char * result = NULL;;
   char * path = xdg_cache_path();
   assert(path);   // will be null if $HOME not set, how to handle?
   result = find_xdg_path_file(
                path,
                application,
                simple_fn);
   free(path);
   if (debug)
      printf("(%s) application=%s, simple_fn=%s, returning: %s\n",
              __func__, application, simple_fn, result);
   return result;
}

#ifdef ALT
/** Finds a file in the application sub-directory of $XDG_CACHE_HOME */
char *
find_xdg_cache_file(
      const char * application,
      const char * simple_fn)
{
   char * result = NULL;
   char * dir = xdg_cache_home_dir();
   if (dir) {
      result = find_xdg_home_file(dir, application, simple_fn);
      free(dir);
   }
   return result;
}
#endif


void xdg_tests() {
   rpt_vstring(1, "xdg_data_home_dir():   %s", xdg_data_home_dir() );
   rpt_vstring(1, "xdg_data_config_dir(): %s", xdg_config_home_dir() );
   rpt_vstring(1, "xdg_data_cache_dir():  %s", xdg_cache_home_dir() );

   rpt_vstring(1, "xdg_data_dirs():       %s", xdg_data_dirs() );
   rpt_vstring(1, "xdg_config_dirs():     %s", xdg_config_dirs() );

   rpt_vstring(1, "xdg_data_path():   %s", xdg_data_path() );
   rpt_vstring(1, "xdg_config_path(): %s", xdg_config_path() );
   rpt_vstring(1, "xdg_cache_path():  %s", xdg_cache_path() );

   rpt_vstring(1, "xdg_data_home_file(\"ddcutil\", \"something.mccs\"): %s",
         xdg_data_home_file("ddcutil", "something.mccs"));
   rpt_vstring(1, "xdg_config_home_file(\"ddcutil\", \"ddcutilrc\"): %s",
         xdg_config_home_file("ddcutil", "ddcutilrc"));
   rpt_vstring(1, "xdg_cache_home_file(\"ddcutil\", \"capabilities\"): %s",
         xdg_cache_home_file("ddcutil", "capabilities"));

   rpt_vstring(1, "find xdg_data_file(\"ddcutil\", \"something.mccs\"): %s",
         find_xdg_data_file("ddcutil", "something.mccs"));
   rpt_vstring(1, "find_xdg_config_file(\"ddcutil\", \"ddcutilrc\"): %s",
         find_xdg_config_file("ddcutil", "ddcutilrc"));
   rpt_vstring(1, "find_xdg_cache_file(\"ddcutil\", \"capabilities\"): %s",
         find_xdg_cache_file("ddcutil", "capabilities"));


}

