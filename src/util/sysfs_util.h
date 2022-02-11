/** \file sysfs_util.h
 * Functions for reading /sys file system
 */

// Copyright (C) 2016-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SYSFS_UTIL_H_
#define SYSFS_UTIL_H_

#include "config.h"

#include <stdbool.h>
#include <glib-2.0/glib.h>


char *
read_sysfs_attr(
      const char * dirname,
      const char * attrname,
      bool         verbose);

char *
read_sysfs_attr_w_default(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      bool         verbose);

char *
read_sysfs_attr_w_default_r(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      char *       buf,
      unsigned     bufsz,
      bool         verbose);

GByteArray *
read_binary_sysfs_attr(
      const char * dirname,
      const char * attrname,
      int          est_size,
      bool         verbose);

char *
get_rpath_basename(
      const char * path);

bool
set_rpt_sysfs_attr_silent(bool silent);

void
rpt_attr_output(
      int depth,
      const char * node,
      const char * op,
      const char * value);

bool
rpt_attr_text(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...);

#define RPT_ATTR_TEXT(depth, value_loc, fn_segment, ...) \
   rpt_attr_text(depth, value_loc, fn_segment, ##__VA_ARGS__, NULL)

#define GET_ATTR_TEXT(value_loc, fn_segment, ...) \
   rpt_attr_text(-1, value_loc, fn_segment, ##__VA_ARGS__, NULL)

bool
rpt_attr_binary(
      int           depth,
      GByteArray ** value_loc,
      const char *  fn_segment,
      ...);

bool
rpt_attr_edid(
      int           depth,
      GByteArray ** value_loc,
      const char *  fn_segment,
      ...);

#define RPT_ATTR_EDID(depth, value_loc, fn_segment, ...) \
   rpt_attr_edid(depth, value_loc, fn_segment, ##__VA_ARGS__, NULL)

#define GET_ATTR_EDID(value_loc, fn_segment, ...) \
   rpt_attr_edid(-1, value_loc, fn_segment, ##__VA_ARGS__, NULL)

bool
rpt_attr_realpath(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...);

#define RPT_ATTR_REALPATH(depth, value_loc, fn_segment, ...) \
   rpt_attr_realpath(depth, value_loc, fn_segment, ##__VA_ARGS__, NULL)

#define GET_ATTR_REALPATH(value_loc, fn_segment, ...) \
   rpt_attr_realpath(-1, value_loc, fn_segment, ##__VA_ARGS__, NULL)

bool
rpt_attr_realpath_basename(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...);

#define RPT_ATTR_REALPATH_BASENAME(depth, value_loc, fn_segment, ...) \
   rpt_attr_realpath_basename(depth, value_loc, fn_segment, ##__VA_ARGS__, NULL)

#define GET_ATTR_REALPATH_BASENAME(depth, value_loc, fn_segment, ...) \
   rpt_attr_realpath_basename(-1, value_loc, fn_segment, ##__VA_ARGS__, NULL)

typedef bool (*Fn_Filter)(const char * fn, const char * val);

bool
rpt_attr_single_subdir(
      int          depth,
      char **      value_loc,
      Fn_Filter    predicate_function,
      const char * predicate_value,
      const char * fn_segment,
      ...);

#define RPT_ATTR_SINGLE_SUBDIR(depth, value_loc, predicate_func, predicate_val, fn_segment, ...) \
   rpt_attr_single_subdir(depth, value_loc, predicate_func, predicate_val, fn_segment, ##__VA_ARGS__, NULL)

#define GET_ATTR_SINGLE_SUBDIR(value_loc, predicate_func, predicate_val, fn_segment, ...) \
   rpt_attr_single_subdir(-1, value_loc, predicate_func, predicate_val, fn_segment, ##__VA_ARGS__, NULL)

bool
rpt_attr_note_subdir(
      int          depth,
      char **      value_loc,
      const char * fn_segment,
      ...);

#define RPT_ATTR_NOTE_SUBDIR(depth, value_loc, fn_segment, ...) \
   rpt_attr_note_subdir(depth, value_loc, fn_segment,  ##__VA_ARGS__, NULL)

#endif /* SYSFS_UTIL_H_ */
