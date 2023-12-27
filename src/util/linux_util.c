/** @file linux_util.c
 *
 *  Miscellaneous Linux utilities
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#define _GNU_SOURCE    // for syscall()
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <sys/utsname.h>
#include <unistd.h>

#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <syslog.h>
#endif
/** \endcond */

#include "file_util.h"
#include "report_util.h"
#include "string_util.h"
#include "subprocess_util.h"

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
   int unfiltered_ct = read_file_with_filter(lines, config_fn, terms, false, 0, true);
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


/** Checks whether a module file exists for the current kernel.
 *
 *  Name variants using underscores (_) and hyphens (-) are both checked.
 *
 *  Allows for extension .ko.xz etc. as well as .ko.
 *
 *  @param  module_name  name of module
 *  @retval true  file exists
 *  @retval false file does not exist
 */
bool find_module_ko(const char * module_name) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. module_name: %s\n", __func__, module_name);

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char * module_name1 = strdup(module_name);
   char * module_name2 = strdup(module_name);
   str_replace_char(module_name1, '-','_');
   str_replace_char(module_name2, '_','-');

   bool result = false;
   char cmd[200];
   g_snprintf(cmd, 200, "find /lib/modules/%s -name \"%s.ko*\" -o -name \"%s.ko*\"",
         utsbuf.release, module_name1, module_name2);
   if (debug)
      printf("(%s) cmd |%s|\n", __func__, cmd);
   GPtrArray * cmd_result = execute_shell_cmd_collect(cmd);
   if (cmd_result) {
      if (debug)
         printf("(%s) len=%d\n", __func__, cmd_result->len);
      if (cmd_result->len > 0) {
         if (debug)
             printf("(%s) Found: %s\n", __func__, (char*) g_ptr_array_index(cmd_result,0));
         result = true;
      }
      g_ptr_array_free(cmd_result,true);
   }
   free(module_name1);
   free(module_name2);

   if (debug)
      printf("(%s) Done.  Returning %s\n", __func__, sbool(result));
   return result;
}


/** Examines file /lib/modules/<kernel release>/modules/builtin to determine
 *  whether a module is built into the kernel.
 *
 *  Name variants using underscores (_) and hyphens (-) are both checked.
 *
 *  Allows for extension .ko.xz etc. as well as .ko.
 *
 *  @param  module_name  name of module
 *  @retval true  module is built in
 *  @retval false module is not built in, or modules/builtin file not found
 *
 *  @remark
 *  It is possible that modules/builtin does not exist for some incorrectly
 *  built kernel.
 */
bool is_module_built_in(const char * module_name) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. module_name = |%s|\n", __func__, module_name);

   // Look for name variants with either "-" or "_"
   char * module_name1 = g_strdup_printf("%s.ko", module_name);
   char * module_name2 = g_strdup_printf("%s.ko", module_name);
   str_replace_char(module_name1, '-','_');
   str_replace_char(module_name2, '_','-');

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char builtin_fn[PATH_MAX];
   g_snprintf(builtin_fn, PATH_MAX, "/lib/modules/%s/modules.builtin", utsbuf.release);
   bool found = false;
#ifdef ALT
   if ( !regular_file_exists(builtin_fn) ) {
      fprintf(stderr, "File not found: %s\n", builtin_fn);
   }
   else {
      char cmd[200];
      // not everything is under kernel/drivers e.g. fbdev.ko is under kernel/arch/x86/video
      g_snprintf(cmd, 200, "grep  -e \"^kernel/.*/%s\" -e \"^kernel/.*/%s\"  %s ",
                 module_name1, module_name2, builtin_fn);  // allow for .ko.xz etc.
      if (debug)
         printf("(%s) cmd |%s|\n", __func__, cmd);
      GPtrArray * cmd_result = execute_shell_cmd_collect(cmd);
      if (cmd_result) {
         if (debug)
            printf("(%s) len=%d\n", __func__, cmd_result->len);
         if (cmd_result->len > 0) {
            found = true;
         }
         g_ptr_array_free(cmd_result,true);
      }
   }
#else
    GPtrArray * lines = g_ptr_array_new_full(400, g_free);
    char * terms[3];
    terms[0] = module_name1;
    terms[1] = module_name2;  // probably same as module_name1, but not worth optimizing
    terms[2] = NULL;
    int unfiltered_ct = read_file_with_filter(lines, builtin_fn, terms, false, 0, false);
    if (unfiltered_ct < 0) {   //  = -errno
       fprintf(stderr, "Error reading file %s: %s\n", builtin_fn, strerror(errno));
       fprintf(stderr, "Assuming module %s is not built in to kernel\n", module_name);
    }
    else {
       found = (lines->len == 1);
    }
    g_ptr_array_free(lines, true);
#endif

   free(module_name1);
   free(module_name2);

   if (debug)
      printf("(%s) Done.    module_name=%s, Returning %s\n", __func__, module_name, sbool(found));
   return found;
}


char * kernel_module_types[] = {
      "KERNEL_MODULE_NOT_FOUND",          // 0
      "KERNEL_MODULE_BUILTIN",            // 1
      "KERNEL_MODULE_LOADABLE_FILE"};     // 2

int module_status_by_modules_builtin_or_existence(const char * module_name) {
   bool debug = false;
   int result = KERNEL_MODULE_NOT_FOUND;
   if ( is_module_built_in(module_name) )
      result = KERNEL_MODULE_BUILTIN;
   else {
      bool found = find_module_ko(module_name);
      if (found) {
         result = KERNEL_MODULE_LOADABLE_FILE;
      }
   }
   if (debug)
      printf("(%s) Executed. module_name=%s, returning %d = %s\n",
             __func__, module_name, result, kernel_module_types[result]);
   return result;
}


/** Examines file /boot/config-<kernel version> to determine whether module
 *  i2c-dev exists and if so whether it is built into the kernel or is a
 *  loadable module.
 *
 *  @retval y   built into kernel
 *  @retval m   built as loadable module
 *  @retval n   not built
 *  @retval X   /boot/config file not found, or CONFIG_I2C_CHARDEV line not found
 */
char i2c_dev_status_by_boot_config_file() {
   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char config_fn[PATH_MAX];
   g_snprintf(config_fn, PATH_MAX, "/boot/config-%s", utsbuf.release);

   char status = 'X';
   if ( !regular_file_exists(config_fn) ) {
      fprintf(stderr, "Kernel configuration file not found: %s\n", config_fn);
   }
   else {
      char cmd[100];
      g_snprintf(cmd, 100, "grep CONFIG_I2C_CHARDEV= /boot/config-%s", utsbuf.release);
      int pos = strlen("CONFIG_I2C_CHARDEV=");

      char * cmd_result = execute_shell_cmd_one_line_result(cmd);
      if (!cmd_result) {
         fprintf(stderr, "CONFIG_I2C_CHARDEV not found in %s\n", config_fn);
      }
      else {
         status = cmd_result[pos];
         free(cmd_result);
      }
   }
   return status;
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


void rpt_lsof(const char * fqfn, int depth) {
   // rpt_vstring(depth, "Programs with %s open:");
   char cmd[PATH_MAX+20];
   g_snprintf(cmd, PATH_MAX+20, "lsof %s", fqfn);
   execute_shell_cmd_rpt(cmd, depth);
}


// to do: tailor the output to what is useful
GPtrArray* rpt_lsof_collect(const char * fqfn) {
   char cmd[PATH_MAX+20];
   g_snprintf(cmd, PATH_MAX+20, "lsof %s", fqfn);
   return execute_shell_cmd_collect(cmd);
}



