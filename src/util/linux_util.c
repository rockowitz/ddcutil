/** \file linux_util.c
 * Miscellaneous Linux utilities
 */

// Copyright (C) 2020-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#define GNU_SOURCE    // for syscall()
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef LIBKMOD_H_SUBDIR_KMOD
#include <kmod/libkmod.h>
#else
#include <libkmod.h>
#endif

#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <syslog.h>
#endif
/** \endcond */

#include "file_util.h"
#include "string_util.h"

#include "linux_util.h"


/** Tests whether a file is readable by trying to read from it, as opposed to
  * considering all the rules re permissions, file type, links, etc.
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
   bool debug = false;
   if (debug)
      printf("(%s) Starting. module_alias=%s\n", __func__, module_alias);

   int result = 0;
   struct kmod_ctx * ctx = NULL;
   struct kmod_module *  mod;
   const char * filename;
   int rc = 0;

   ctx = kmod_new(
            NULL ,     // use default modules directory, /lib/modules/`uname -r`
            NULL);     // use default config file path: /etc/modprobe.d, /run/modprobe.d,
                       // /usr/local/lib/modprobe.d and /lib/modprobe.d.
   if (!ctx) {
      result = -errno;
      if (result == 0)     // review of kmond_new() code indicates should be impossible
         result = -999;    // .. but kmod_new() documentation does not guarantee
      goto bye;
   }

   if (debug) {
      const char *  s1 = kmod_get_dirname(ctx);
      printf("(%s) ctx->dirname = |%s|\n", __func__, s1);
   }

   // According to inline kmod doc, this is only a performance enhancer, but
   // without it valgrind reports a branch is taken based on an uninitialized
   // variable in kmod_module_new_from_lookup().  Execution does, however,
   // proceed. "valgrind modinfo" shows the same behavior.
   // If kmod_load_resources() is called, this bogus error message does
   // not occur.
   rc = kmod_load_resources(ctx);
   if (rc < 0) {
      if (debug)
         printf("(__func__) kmod_load_resources() returned %d\n", rc);
      result = rc;
      goto bye;
   }

   struct kmod_list* list = NULL;
   rc = kmod_module_new_from_lookup(ctx, module_alias, &list);
   if (rc < 0) {
      result = rc;
      goto bye;
   }

   if (list == NULL) {
      if (debug)
         printf("(%s) Module %s not found.\n", __func__, module_alias);
      result = KERNEL_MODULE_NOT_FOUND;   // or use result = -ENOENT ?
      goto bye;
   }

   struct kmod_list* itr;
   kmod_list_foreach(itr, list) {
      mod = kmod_module_get_module(itr);
      // would fail if 2 entries in list since filename is const
      // but this is how tools/modinfo.c does it, assumes only 1 entry
      filename = kmod_module_get_path(mod);
      const char *name = kmod_module_get_name(mod);
      if (debug) {
         printf("(%s) name = |%s|, path = |%s|\n", __func__, name, filename);
      }
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


/** Uses libkmod to check if a kernel module is loaded.
 *
 *  \param  module name
 *  \retval 0  not loaded
 *  \retval 1  is loaded
 *  \retval <0 -errno
 *
 *  \remark Adapted from kmod file tools/lsmod.c
 *
 *  \remark
 *  Similar to module_status_using_libkmod(), but calls
 *  kmod_module_new_from_loaded() instead of kmod_module_new_from_lookup().
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
       fprintf(stderr, "Error: could not get list of loaded modules: %s\n", strerror(-err));
       result = err;
       goto bye;
   }

   char * module_name1 = strdup(module_name);
   char * module_name2 = strdup(module_name);
   str_replace_char(module_name1, '-','_');
   str_replace_char(module_name2, '_', '-');
   struct kmod_list *itr;
   bool found = false;
   kmod_list_foreach(itr, list) {
       struct kmod_module *mod = kmod_module_get_module(itr);
       const char *name = kmod_module_get_name(mod);
       kmod_module_unref(mod);
       if (streq(name, module_name1) || streq(name, module_name2)) {
          found = true;
          break;
       }
   }
   free(module_name1);
   free(module_name2);
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


/** Gets the id number of the current thread
 *
 *  \return  thread number
 */
intmax_t get_thread_id() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);
#ifdef TARGET_BSD
   int tid = pthread_getthreadid_np();
#else
   pid_t tid = syscall(SYS_gettid);
#endif
   if (debug)
      printf("(%s) Done.    Returning %ld\n", __func__, (intmax_t) tid);
   return tid;
}


/** Gets the id number of the current process
 *
 *  \return  process number
 */
intmax_t get_process_id()
{
   pid_t pid = syscall(SYS_getpid);
   return pid;
}
