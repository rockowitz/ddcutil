/** @file sysfs_util.c
  *
  * Functions for reading /sys file system
  */

// Copyright (C) 2016-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

//* \cond */
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "file_util.h"
#include "report_util.h"
#include "string_util.h"

#include "sysfs_util.h"

#ifdef ENABLE_TARGETBSD
#define SYS "/compat/linux/sys"
#else
#define SYS "/sys"
#endif

/** Reads a /sys attribute file, which is 1 line of text
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  verbose    if true, write message to stderr if unable to open file
 * \return pointer to attribute value string, caller is responsible for freeing
 */
char *
read_sysfs_attr(
      const char * dirname,
      const char * attrname,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   return file_get_first_line(fn, verbose);
}


/** Reads a /sys attribute file, which is 1 line of text.
 *  If the attribute is not found, returns a default value
 *
 * \param  dirname        directory name
 * \param  attrname       attribute name, i.e. file name
 * \param  default_value  default value, duplicated
 * \param  verbose        if true, write message to stderr if unable to open file
 * \return pointer to attribute value string, caller is responsible for freeing
 */
char *
read_sysfs_attr_w_default(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   char * result = file_get_first_line(fn, verbose);
   if (!result)
      result = strdup(default_value);  // strdup() so caller can free any result
   return result;
}

char *
read_sysfs_attr_w_default_r(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      char *       buf,
      unsigned     bufsz,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   char * result = file_get_first_line(fn, verbose);
   if (result) {
      g_strlcpy(buf, result, bufsz);
      free(result);
   }
   else {
      g_strlcpy(buf, default_value, bufsz);
   }
   return buf;
}



/** Reads a binary /sys attribute file
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  est_size  estimated size
 * \param  verbose   if open fails, write message to stderr
 * \return if successful, a **GByteArray** of bytes, caller is responsible for freeing
 *          if failure, then NULL
 */
GByteArray *
read_binary_sysfs_attr(
      const char * dirname,
      const char * attrname,
      int          est_size,
      bool         verbose)
{
   assert(dirname);
   assert(attrname);

   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);

   return read_binary_file(fn, est_size, verbose);
}


/** Looks in the /sys file system to check if a module is loaded.
 *
 * \param  module_name    module name
 * \return true if the module is loaded, false if not
 */
bool
is_module_loaded_using_sysfs(
      const char * module_name)
{
   bool debug = false;

   struct stat statbuf;
   char   module_fn[100];
   bool   found = false;

   snprintf(module_fn, sizeof(module_fn), "/sys/module/%s", module_name);
   int rc = stat(module_fn, &statbuf);
   if (rc < 0) {
      // will be ENOENT (2) if file not found
      found = false;
   }
   else {
      // if (S_ISDIR(statbuf.st_mode))   // pointless
         found = true;
   }

   if (debug)
      printf("(%s) module_name = %s, returning %d", __func__, module_name, found);
   return found;
}


// The following functions are not really generic sysfs utilities, and more
// properly belong in a file in subdirectory base, but to avoid yet more file
// proliferation are included here.

/** Gets the sysfs name of an I2C device,
 *  i.e. the value of /sys/bus/i2c/devices/i2c-n/name
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing attribute value,
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char *
get_i2c_device_sysfs_name(int busno) {
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return name;
}


char * get_rpath_basename(const char * path) {
   char * result = NULL;
   char resolved_path[PATH_MAX];
   char * rpath = realpath(path, resolved_path);
   // printf("(%s) rpath=|%s|\n", __func__, rpath);
   if (rpath) {

      // printf("realpath returned %s\n", rpath);
      // printf("%s --> %s\n",workfn, resolved_path);
      result = g_path_get_basename(rpath);
   }
   // printf("(%s) busno=%d, returning %s\n", __func__, busno, driver_name);
   return result;
}



/** Gets the driver name of an I2C device,
 *  i.e. the basename of /sys/bus/i2c/devices/i2c-n/device/driver/module
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing driver name
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char *
get_i2c_device_sysfs_driver(int busno) {
   char * driver_name = NULL;
   char workbuf[100];
   snprintf(workbuf, 100, SYS"/bus/i2c/devices/i2c-%d/device/driver/module", busno);
   driver_name = get_rpath_basename(workbuf);
   if (!driver_name) {
      snprintf(workbuf, 100, SYS"/bus/i2c/devices/i2c-%d/device/device/device/driver/module", busno);
      driver_name = get_rpath_basename(workbuf);
   }
   printf("(%s) busno=%d, returning %s\n", __func__, busno, driver_name);
   return driver_name;
}


#ifdef UNUSED
static bool is_smbus_device_using_sysfs(int busno) {
#ifdef OLD
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
#endif
   char * name = get_i2c_device_sysfs_name(busno);

   bool result = false;
   if (name && str_starts_with(name, "SMBus"))
      result = true;
   free(name);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return result;
}
#endif


static bool
ignorable_i2c_device_sysfs_name(const char * name, const char * driver) {
   bool result = false;
   const char * ignorable_prefixes[] = {
         "SMBus",
         "Synopsys DesignWare",
         "soc:i2cdsi",   // Raspberry Pi
         "smu",          // Mac G5, probing causes system hang
         "mac-io",       // Mac G5
         "u4",           // Mac G5
         NULL };
   if (name) {
      if (starts_with_any(name, ignorable_prefixes) >= 0)
         result = true;
      else if (streq(driver, "nouveau")) {
         if ( !str_starts_with(name, "nvkm-") ) {
            result = true;
            // printf("(%s) name=|%s|, driver=|%s| - Ignore\n", __func__, name, driver);
         }
      }
   }
   // printf("(%s) name=|%s|, driver=|%s|, returning: %s\n", __func__, name, driver, sbool(result));
   return result;
}


/** Checks if an I2C bus cannot be a DDC/CI connected monitor
 *  and therefore can be ignored, e.g. if it is an SMBus device.
 *
 *  \param  busno  I2C bus number
 *  \return true if ignorable, false if not
 *
 *  \remark
 *  This function avoids unnecessary calls to i2cdetect, which can be
 *  slow for SMBus devices and fills the system logs with errors
 */
bool
sysfs_is_ignorable_i2c_device(int busno) {
   bool result = false;
   char * name = get_i2c_device_sysfs_name(busno);
   char * driver = get_i2c_device_sysfs_driver(busno);
   if (name)
      result = ignorable_i2c_device_sysfs_name(name, driver);

   // printf("(%s) busno=%d, name=|%s|, returning: %s\n", __func__, busno, name, bool_repr(result));
   free(name);    // safe if NULL
   free(driver);  // ditto
   return result;
}


//
// Functions for probing /sys
//

static bool rpt2_silent = true;

bool
set_rpt_sysfs_attr_silent(bool onoff) {
   bool old = rpt2_silent;
   rpt2_silent = onoff;
   return old;
}


static void
rpt_attr_output(
      int depth,
      const char * node,
      const char * op,
      const char * value)
{
   if (!rpt2_silent) {
      int offset = 70;
      if (depth >= 0)
         rpt_vstring(depth, "%-*s%-2s %s", offset, node, op, value);
   }
}


static inline char *
read_sysfs_attr0(
      const char * fq_attrname,
      bool         verbose)
{
   return file_get_first_line(fq_attrname, verbose);
}


typedef bool (*Fn_Filter)(const char * fn, const char * val);

static char *
get_single_subdir_name(
      const char * dirname,
      Fn_Filter    filter,
      const char * val)
{
   // bool debug = false;
   int d1 = 1;
   DIR* dir2 = opendir(dirname);
   char * result = NULL;
   if (!dir2) {
      rpt_vstring(d1, "Unexpected error. Unable to open sysfs directory %s: %s\n",
                      dirname, strerror(errno));
     }
     else {
        struct dirent *dent2;
        while ((dent2 = readdir(dir2)) != NULL) {
           // DBGMSF(debug, "%s", dent2->d_name);
           if (!str_starts_with(dent2->d_name, ".")) {
              if (!filter || filter(dent2->d_name, val)) {
                 result = strdup(dent2->d_name);
                 break;
              }
           }
        }
        closedir(dir2);
     }
   // DBGMSF(debug, "directory: %s, first subdir: %s", dirname, result);
   return result;
}


static char *
assemble_sysfs_path2(
      char *        buffer,
      int           bufsz,
      const char *  fn_segment,
      va_list       ap)
{
   g_strlcpy(buffer, fn_segment, bufsz);
   while(true) {
      char * segment = va_arg(ap, char*);
      if (!segment)
         break;
      g_strlcat(buffer, "/", bufsz);
      g_strlcat(buffer, segment, bufsz);
   }
   return buffer;
}


bool
rpt2_attr_text(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...)
{
   // DBGMSG("Starting. fn_segment=%s", fn_segment);
   bool found = false;
   if (value_loc)
      *value_loc = NULL;
   char pb1[PATH_MAX];

   va_list ap;
   va_start(ap, fn_segment);
   assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
   va_end(ap);
   // DBGMSG("pb1=%s", pb1);

   char * val = read_sysfs_attr0(pb1, false);
   if (val) {
      found = true;
      rpt_attr_output(depth, pb1, "=", val);
      if (value_loc)
         *value_loc = val;
      else
         free(val);
  }
  else {
     rpt_attr_output(depth, pb1, ": ", "Not Found");
  }
  // DBGMSG("Done");
  return found;
}


bool rpt2_attr_binary(
      int          depth,
      GByteArray ** value_loc,
      const char *  fn_segment,
      ...)
{
   char pb1[PATH_MAX];
   va_list ap;
   va_start(ap, fn_segment);
   assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
   va_end(ap);
   // DBGMSG("pb1=%s", pb1);

   bool found = false;
   if (value_loc)
      *value_loc = NULL;
   GByteArray * bytes = read_binary_file(pb1, 256, true);
   if (bytes && bytes->len > 0) {
      found = true;
      rpt_attr_output(depth, pb1, ":", "Found");
      // rpt_vstring(depth, "%-*s =  %s", offset, pb1, val);
      if (value_loc)
         *value_loc = bytes;
      else
         g_byte_array_free(bytes, true);
   }
   else
      rpt_attr_output(depth, pb1, ": ", "Not Found");
   return found;
}


bool rpt2_attr_edid(
       int           depth,
       GByteArray ** value_loc,
       const char *        fn_segment,
       ...)
 {
    char pb1[PATH_MAX];
    va_list ap;
    va_start(ap, fn_segment);
    assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
    va_end(ap);
    // DBGMSG("pb1=%s", pb1);

    bool found = false;
    if (value_loc)
       *value_loc = NULL;
    GByteArray * edid = NULL;
    found = rpt2_attr_binary(depth, &edid, pb1, NULL);
    if (edid) {
       assert(found);
       if (depth >= 0)
          rpt_hex_dump(edid->data, edid->len, depth+4);
       if (value_loc)
          *value_loc = edid;
       else {
          g_byte_array_free(edid, true);
       }
    }

    return found;
 }


bool
rpt2_attr_realpath(
      int          depth,
      char **      value_loc,
      const char *       fn_segment,
      ...)
{
   if (value_loc)
      *value_loc = NULL;
   char pb1[PATH_MAX];

   va_list ap;
   va_start(ap, fn_segment);
   assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
   va_end(ap);
   //DBGMSG("pb1=%s", pb1);

   char * result = realpath(pb1, NULL);
   bool found = (result);
   if (result) {
      rpt_attr_output(depth, pb1, "->", result);
      if (value_loc)
         *value_loc = result;
      else
         free(result);
   }
   else {
      rpt_attr_output(depth, pb1, "->", "Invalid path");
   }
   return found;
}


bool
rpt2_attr_realpath_basename(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...)
{
   char pb1[PATH_MAX];
   va_list ap;
   va_start(ap, fn_segment);
   assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
   va_end(ap);

   bool found = false;
   if (value_loc)
      *value_loc = NULL;
   char pb2[PATH_MAX];
   char * rpath = realpath(pb1, pb2);  // without assignment, get warning that return value unused
   if (rpath) {
      char * bpath = basename(rpath);
      if (bpath) {
         found = true;
         rpt_attr_output(depth, pb1, "->", bpath);
         if (value_loc)
            *value_loc = strdup(bpath);
      }
   }
   if (!found) {
      rpt_attr_output(depth, pb1, "->", "Invalid path");
   }
   return found;
}


bool rpt2_attr_single_subdir(
      int          depth,
      char **      value_loc,
      Fn_Filter    predicate_function,
      const char * predicate_value,
      const char * fn_segment,
      ...)
{
   char pb1[PATH_MAX];
   va_list ap;
   va_start(ap, fn_segment);
   assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
   va_end(ap);

   if (value_loc)
      *value_loc = NULL;
   char * subdir_name = get_single_subdir_name(pb1, predicate_function, predicate_value);
   bool found = false;
   if (subdir_name) {
      char buf[PATH_MAX+100];
      g_snprintf(buf, PATH_MAX+100, "Found subdirectory = %s", subdir_name);
      rpt_attr_output(depth, pb1, ":", buf);
      if (value_loc)
         *value_loc = subdir_name;
      else
         free(subdir_name);
      found = true;
   }
   else {
      char buf[PATH_MAX+100];
      g_snprintf(buf, PATH_MAX+100, "No %s subdirectory found", predicate_value);
      rpt_attr_output(depth, pb1, ":", buf);
   }
   return found;
}


bool rpt2_attr_note_subdir(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...)
{
   char pb1[PATH_MAX];
   va_list ap;
   va_start(ap, fn_segment);
   assemble_sysfs_path2(pb1, PATH_MAX, fn_segment, ap);
   va_end(ap);
   // DBGMSG("pb1: %s", pb1);

   if (value_loc)
      *value_loc = NULL;

   bool found = directory_exists(pb1);

   if (found)
      rpt_attr_output(depth, pb1, ":", "Subdirectory");
   else
      rpt_attr_output(depth, pb1, ":", "No such subdirectory");

   return found;
}





