/* subprocess_util.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file subprocess_util.h
* Functions to execute shell commands
*/

#ifndef SUBPROCESS_UTIL_H_
#define SUBPROCESS_UTIL_H_

/** \cond */
#include <glib.h>
#include <stdbool.h>
/** \endcond */

bool execute_shell_cmd(char * shell_cmd);
bool execute_shell_cmd_rpt(char * shell_cmd, int depth);
GPtrArray * execute_shell_cmd_collect(char * shell_cmd);
bool is_command_in_path(char * cmd);

#endif /* SUBPROCESS_UTIL_H_ */
