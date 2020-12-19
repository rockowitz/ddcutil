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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "file_util.h"
#include "report_util.h"
#include "string_util.h"

#include "sysfs_util.h"


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



char *
get_rpath_basename(
      const char * path)
{
   char * result = NULL;
   char resolved_path[PATH_MAX];
   char * rpath = realpath(path, resolved_path);
   // printf("(%s) rpath=|%s|\n", __func__, rpath);
   if (rpath) {
      result = g_path_get_basename(rpath);
   }
   // printf("(%s) busno=%d, returning %s\n", __func__, busno, driver_name);
   return result;
}


//
// Functions for probing /sys
//

static bool rpt2_silent = false;

bool
set_rpt_sysfs_attr_silent(
      bool onoff)
{
   bool old = rpt2_silent;
   rpt2_silent = onoff;
   return old;
}


void
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


bool
rpt2_attr_binary(
      int           depth,
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


bool
rpt2_attr_edid(
       int           depth,
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
      const char * fn_segment,
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


bool
rpt2_attr_single_subdir(
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


bool
rpt2_attr_note_subdir(
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

