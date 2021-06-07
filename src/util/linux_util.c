/** \file linux_util.c
 * Miscellaneous Linux utilities
 */

// Copyright (C) 2020-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define USE_LIBKMOD

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef USE_LIBKMOD
#include <libkmod.h>
#endif

#include "file_util.h"
#include "string_util.h"

#include "linux_util.h"


/** Tests whether a file is readable by trying to read from it,
  * as opposed to considering all the rules re permissions, file type,
  * links, etc.
  *
  * \param filename
  * \return true if file can be read from false if not
  */
bool is_readable_file(const char * filename)
{
   // avoid all the rules re permissions, file type, links, ls etc
   // just try to read from the file
   bool result = false;
   int fd = open(filename, O_RDONLY);
   if (fd > 0) {
      char buf;
      if (read(fd, &buf, 1) > 0)
         result = true;
      close(fd);
   }
   return result;
}


/** Gets the value of a kernel configuration parameter from file
  * /boot/config-KERNEL_RELEASE", where KERNEL_RELEASE is the kernel release name.
  *
  * \param     parm_name   parameter name
  * \param     buffer      buffer in which to return value
  * \param     bufsz       size of buffer
  * \param     1           configuration parm found, value is in buffer
  * \retval    0           configuration parm not found
  * \retval    < 0         error reading configuration file
  */

int get_kernel_config_parm(const char * parm_name, char * buffer, int bufsz)
{
   bool debug = false;
   if (debug)
      printf("(%s) Staring. parm_name=%s, buffer=%p, bufsz=%d\n",
             __func__, parm_name, buffer, bufsz);
   buffer[0] = '\0';

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char config_fn[100];
   snprintf(config_fn, 100, "/boot/config-%s", utsbuf.release);

   char search_str[40];
   snprintf(search_str, 40, "%s=", parm_name);
   if (debug)
      printf("(%s) search_str=|%s|, len=%ld\n", __func__, search_str, (unsigned long) strlen(search_str));

   GPtrArray * lines = g_ptr_array_new_full(15000, g_free);
   char * terms[2];
   terms[0] = search_str;
   terms[1] = NULL;
   int unfiltered_ct = read_file_with_filter(lines, config_fn, terms, false, 0);
   if (debug)
      printf("(%s) read_file_with_filter() returned %d, lines->len=%d\n",
             __func__, unfiltered_ct, lines->len);
   if (unfiltered_ct < 0) {
      rc = unfiltered_ct;  // -errno
   }
   else if (lines->len == 0) {   // count after filtering
      rc = 0;
   }
   else {
      assert(lines->len == 1);
      char * aline = g_ptr_array_index(lines, 0);
      char * value = aline + strlen(search_str);
      if (debug)
         printf("(%s) strlen(search_str)=%ld aline=%p->|%s|, value=%p->|%s|\n",
                __func__, (unsigned long)strlen(search_str), aline, aline, value, value);
      assert(strlen(value) < bufsz);
      // snprintf(buffer, bufsz, "%s", value);
      strcpy(buffer, value);
      rc = 1;
   }
   g_ptr_array_free(lines, true);

   if (debug)
      printf("(%s) rc=%d, strlen(buffer) = %ld, buffer=|%s|\n",
             __func__, rc, (unsigned long) strlen(buffer), buffer);

    ASSERT_IFF(rc==1, strlen(buffer) > 0);
    if (debug)
       printf("(%s) Done. parm=%s, returning %d, result=%s\n",
              __func__,  parm_name, rc, buffer);
    return rc;
}


/** Uses libkmod to determine if a kernel module exists and
  * if so whether it is built into the kernel or a loadable file.
  *
  * \param   module_alias
  * \retval  0=KERNEL_MODULE_NOT_FOUND      not found  (or use -ENOENT?)
  * \retval  1=KERNEL_MODULE_BUILTIN        module is built into kernel
  * \retval  2=KERNEL_MODULE_LOADABLE_FILE  module is is loadable file
  * \retval  < 0                            -errno
  *
  * \remark Adapted from kmod file tools/modinfo.c
  */
int module_status_using_libkmod(const char * module_alias)
{
   bool debug = true;
   if (debug)
      printf("(%s) Starting. module_alias=%s\n", __func__, module_alias);

   int result = 0;
   struct kmod_ctx * ctx;
   struct kmod_module *  mod;
   int err;
   const char * filename;

   ctx = kmod_new(NULL, NULL);
   if (!ctx) {
      result = -errno;
      if (result == 0)     // review of kmond_new() code indicates should be impossible
         result = -999;    // .. but kmod_new() documentation does not guarantee
      goto bye;
   }

   struct kmod_list *l, *list = NULL;
   err = kmod_module_new_from_lookup(ctx, module_alias, &list);
   if (err < 0) {
      result = err;
      goto bye;
   }

   if (list == NULL) {
      if (debug)
         printf("(%s) Module %s not found.\n", __func__, module_alias);
      result = KERNEL_MODULE_NOT_FOUND;   // or use result = -ENOENT ?
      goto bye;
   }

   kmod_list_foreach(l, list) {
      mod = kmod_module_get_module(l);
      // would fail if 2 entries in list since filename is const
      // but this is how tools/modinfo.c does it, assumes only 1 entry
      filename = kmod_module_get_path(mod);
      kmod_module_unref(mod);
   }
   kmod_module_unref_list(list);

   result = (filename) ? KERNEL_MODULE_LOADABLE_FILE : KERNEL_MODULE_BUILTIN;

bye:
   if (ctx)
      kmod_unref(ctx);

   if (debug)
      printf("(%s) Done.     module_alias=%s, returning %d\n",__func__,  module_alias, result);
   return result;
}


// transitional
int is_module_builtin2(const char * module_name) {
   int rc = module_status_using_libkmod(module_name);
   int result = 0;
   switch(rc)
   {
   case 0: result = 0;    break;     // not found
   case 1: result = 1;    break;     // builtin
   case 2: result = 0;    break;     // is loadable file
   default: result = rc;  break;     // -errno
   }
   return result;
}


// transitional
bool is_module_loadable2(const char * module_name) {
   int rc = module_status_using_libkmod(module_name);
   bool result = false;
   switch(rc)
   {
   case 0: result = false;  break;     // not found
   case 1: result = false;    break;     // builtin
   case 2: result = true;    break;     // is loadable file
   default: result = false;  break;     // -errno   ???
   }
   return result;
}


/** Reads the modules.builtin file to see a module is built into the kernel.
  *
  * \param  module_name  simple module name, as it appears in the file system, e.g. i2c-dev
  * \retval 1   is built in
  * \retval 0   not built in
  * \retval < 0 error reading the modules.builtin file, value is -errno
  */
int is_module_builtin(char * module_name)
{
   bool debug = false;
   int result = -1;

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   const char * libdirs[] = {
                  "lib",
                  "lib64",
                  "lib32",
                  "usr/lib",  // needed for arch?
                  NULL};

   int ndx = 0;
   for (; libdirs[ndx] && result == -1; ndx++) {
      char modules_builtin_fn[100];
      snprintf(modules_builtin_fn, 100, "/%s/modules/%s/modules.builtin",
                                        libdirs[ndx], utsbuf.release);
      if ( !is_readable_file(modules_builtin_fn) ) {
         if (debug)
            printf("(%s) File %s not readable\n", __func__, modules_builtin_fn);
      }
      else {
         if (debug)
            printf("(%s) Found readable file %s\n", __func__, modules_builtin_fn);
         char ko_name[40];
         snprintf(ko_name, 40, "%s.ko", module_name);
         char * filter_terms[2];
         filter_terms[0] = ko_name;
         filter_terms[1] = NULL;
         GPtrArray * lines = g_ptr_array_sized_new(400);
         g_ptr_array_set_free_func(lines, g_free);
         int unfiltered_ct = read_file_with_filter(lines, modules_builtin_fn, filter_terms, false, 0);
         if (unfiltered_ct < 0) {
           result = unfiltered_ct;   // -errno
         }
         else if (lines->len == 0) {
            result = 0;
         }
         else {
            assert(lines->len == 1);
            result = 1;
         }
         g_ptr_array_free(lines, true);
      }
   }
   if (debug)
      printf("(%s) module_name=%s, returning %d\n",__func__,  module_name, result);
   return result;
}


/** Uses libkmod to check if a kernel module is loaded.
 *
 *  \param  module name
 *  \retval 0  not loaded
 *  \retval 1  is loaded
 *  \retval <0 -errno
 *
 *  \remark Adapted from kmod file tools/lsmod.c
 */
int is_module_loaded_using_libkmod(const char * module_name) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. module_name=%s\n", __func__, module_name);

   int result = 0;
   int err = 0;

   struct kmod_ctx * ctx = kmod_new(NULL, NULL);
   if (!ctx) {
      result = -errno;
      if (result == 0)  // kmond_new() doc does not guarantee that errno set
         result = -999;
      goto bye;
   }

    struct kmod_list *list = NULL;
    err = kmod_module_new_from_loaded(ctx, &list);
    if (err < 0) {
       fprintf(stderr, "Error: could not get list of modules: %s\n", strerror(-err));
       result = err;
       goto bye;
    }

    struct kmod_list *itr;
    bool found = false;
    kmod_list_foreach(itr, list) {
       struct kmod_module *mod = kmod_module_get_module(itr);
       const char *name = kmod_module_get_name(mod);
       kmod_module_unref(mod);
       if (streq(name, module_name)) {
          found = true;
          break;
       }
    }
    kmod_module_unref_list(list);

    result = (found) ? 1 : 0;

bye:
   if (ctx) {
      kmod_unref(ctx);
   }

   if (debug)
      printf("(%s) Done.     Returning: %d\n", __func__, result);
   return result;
}


/** Checks if a loadable module exists
 *
 *  \param module_name  simple module name, as it appears in the file system,
 *                      e.g. i2c-dev, without .ko, .ko.xz
 *  \return             true/false
 */
bool is_module_loadable(char * module_name) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. module_name=%s\n", __func__, module_name);

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   // module_name = "i2c-stub";   // for testing something that will be found
   char module_name_ko[100];
   g_snprintf(module_name_ko, 100, "%s.ko", module_name);
   if (debug)
      printf("(%s) machine: %s", __func__, utsbuf.machine);

   char * libdirs[3];
   libdirs[0] = "lib";
   if (streq(utsbuf.machine, "amd_64")){
      libdirs[1] = "lib64";
      libdirs[2] = NULL;
   }
   else
      libdirs[1] = NULL;

   int libsndx = 0;
   bool found = false;
   for ( ;libdirs[libsndx] && !found; libsndx++ ) {
      char dirname[PATH_MAX];
      g_snprintf(dirname, PATH_MAX, "/%s/modules/%s/kernel/drivers/i2c", libdirs[libsndx], utsbuf.release);
      if (debug)
         printf("(%s) Checking %s\n", __func__, dirname);

      struct dirent *dent;
      DIR           *d;
      d = opendir(dirname);
      if (!d) {
         if (debug)
            printf("(%s) Unable to open directory %s: %s\n", __func__, dirname, strerror(errno));
      }
      else {
      // rpt_vstring(depth, "Examining directory: %s", dirname);
         while ((dent = readdir(d)) != NULL) {
            if (debug)
               printf("(%s) dent->d_name: %s, module_name_ko=%s",
                      __func__, dent->d_name, module_name_ko);
            if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
               if (str_starts_with(dent->d_name, module_name_ko)) {   // may be e.g. i2c-dev.ko.xz
                  found = true;
                  if (debug)
                     printf("(%s) Found\n", __func__);
                  break;
               }
            }
         } // while
         closedir(d);
      }  // else
   }

   if (debug)
      printf("(%s) Done. Returning: %s", __func__, sbool(found));
   return found;
}

