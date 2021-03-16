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
#include <sys/stat.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <stdio.h>

#include "xdg_util.h"


/** Checks if a regular file exists.
 *
 * @param fqfn fully qualified file name
 * @return     true/false
 * @remark
 * Trivial function copied from file_util.c to avoid dependency.
 */
static bool
regular_file_exists(const char * fqfn) {
   bool result = false;
   struct stat stat_buf;
   int rc = stat(fqfn, &stat_buf);
   if (rc == 0) {
      result = S_ISREG(stat_buf.st_mode);
   }
   return result;
}


/** Returns the name of the base data, configuration, or cache, directory.
 *  First the specified environment variable is checked.
 *  If no value is found the name is constructed from $HOME and
 *  the specified sub-directory.
 *
 *  Caller is responsible for freeing the returned memory.
 */
static char * xdg_home_dir(
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


/** Returns the name of the xdg base directory for data files
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * xdg_data_home_dir() {
   bool debug = false;
   char * result = xdg_home_dir("XDG_DATA_HOME", ".local/share");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


/** Returns the name of the xdg base directory for configuration files
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * xdg_config_home_dir() {
   bool debug = false;
   char * result = xdg_home_dir("XDG_CONFIG_HOME", ".config");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


/** Returns the name of the xdg base directory for cached files
*
*  Caller is responsible for freeing the returned memory.
*/
char * xdg_cache_home_dir() {
   bool debug = false;
   char * result = xdg_home_dir("XDG_CACHE_HOME", ".cache");
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}

/** Returns the value of the specified environment variable,
 *  If the value is blank, return default_dirs.
 *
 *  Caller is responsible for freeing the returned memory.
 */
static char * xdg_dirs(
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


/** Returns the value of $XDG_DATA_DIRS or the default "/usr/local/share:/usr/share"
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * xdg_data_dirs() {
   return xdg_dirs("XDG_DATA_DIRS",  "/usr/local/share/:/usr/share");
}


/** Returns the value of $XDG_CONFIG_DIRS, or the default "/etc/xdg"
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * xdg_config_dirs() {
   return xdg_dirs("XDG_CONFIG_DIRS",  "/etc/xdg"); }



/** Returns a path string containing value of the XDG data home directory,
 *  followed by the XDG data dirs string.
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * xdg_data_path() {
   bool debug = false;
   char * result = NULL;
   char * home_dir = xdg_data_home_dir();
   char * dirs     = xdg_data_dirs();
   assert(dirs);
   if (home_dir) {
      result = g_strdup_printf("%s:%s", home_dir, dirs);
      free(home_dir);
      free(dirs);
   }
   else
      result = dirs;
   if (debug)
      printf("(%s) Returning: %s\n", __func__, result);
   return result;
}


/** Returns a path string containing value of the XDG configuration home directory,
 *  followed by the XDG config dirs string.
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * xdg_config_path() {
   bool debug = false;
   char * result = NULL;
   char * home_dir = xdg_config_home_dir();
   char * dirs     = xdg_config_dirs();
   assert(dirs);
   if (home_dir) {
      result = g_strdup_printf("%s:%s", home_dir, dirs);
      free(home_dir);
      free(dirs);
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
 *
 *  Caller is responsible for freeing the returned memory.
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
 *
 *  Caller is responsible for freeing the returned memory.
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
 *
 *  Caller is responsible for freeing the returned memory.
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


typedef struct {
   char * iter_start;
   char * iter_end;
} Iter_State;


static void xdg_dirs_iter_init(char * dir_list, Iter_State * state) {
   state->iter_start = dir_list;  // to avoid const warnings
   state->iter_end = state->iter_start + strlen(dir_list);
}


static char * xdg_dirs_iter_next(Iter_State * state) {
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


/* Caller is responsible for freeing the returned memory. */
static char * find_xdg_path_file(
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
   char *path2 = strdup(path);
   xdg_dirs_iter_init(path2, &iter_state);
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
   free(path2);
   if (debug)
      printf("(%s) Done. Returning: %s\n", __func__, fqfn);
   return fqfn;
}


/** Looks for a file first in the $XDG_DATA_HOME directory,
 *  then in the $XDG_DATA_DIRS directories.
 *
 *  \param  application   subdirectory name
 *  \param  simple_fn     file name within subdirectory
 *  \return fully qualified file name, or NULL if not found.
 *
 *  Caller is responsible for freeing the returned memory.
 */
char * find_xdg_data_file(
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


/** Searches $XDG_CONFIG_HOME and then $XDG_CONFIG_DIRS for
 *  a specified file in a particular application sub-directory.
 *
 *  \param  application   subdirectory name
 *  \param  simple_fn     file name within subdirectory
 *  \return fully qualified file name, or NULL if not found.
 *
 *  Caller is responsible for freeing the returned string.
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


/** Looks for a file in the specified subdirectory of $XDG_CACHE_HOME
 *
 *  \param  application   subdirectory name
 *  \param  simple_fn     file name within subdirectory
 *  \return fully qualified file name, or NULL if not found.
 *
 *  Caller is responsible for freeing the returned string.
 */
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


#ifdef TESTS
void xdg_tests() {
   printf( "xdg_data_home_dir():   %s\n", xdg_data_home_dir() );
   printf( "xdg_data_config_dir(): %s\n", xdg_config_home_dir() );
   printf( "xdg_data_cache_dir():  %s\n", xdg_cache_home_dir() );

   printf( "xdg_data_dirs():       %s\n", xdg_data_dirs() );
   printf( "xdg_config_dirs():     %s\n", xdg_config_dirs() );

   printf( "xdg_data_path():       %s\n", xdg_data_path() );
   printf( "xdg_config_path():     %s\n", xdg_config_path() );
   printf( "xdg_cache_path():      %s\n", xdg_cache_path() );

   printf( "xdg_data_home_file(\"ddcutil\", \"something.mccs\"): %s",
         xdg_data_home_file("ddcutil", "something.mccs"));
   printf( "xdg_config_home_file(\"ddcutil\", \"ddcutilrc\"   ): %s",
         xdg_config_home_file("ddcutil", "ddcutilrc"));
   printf( "xdg_cache_home_file(\"ddcutil\", \"capabilities\" ): %s",
         xdg_cache_home_file("ddcutil", "capabilities"));

   printf( "find xdg_data_file(\"ddcutil\", \"something.mccs\"): %s",
         find_xdg_data_file("ddcutil", "something.mccs"));
   printf( "find_xdg_config_file(\"ddcutil\", \"ddcutilrc\"):    %s",
         find_xdg_config_file("ddcutil", "ddcutilrc"));
   printf( "find_xdg_cache_file(\"ddcutil\", \"capabilities\"):  %s",
         find_xdg_cache_file("ddcutil", "capabilities"));
}
#endif

