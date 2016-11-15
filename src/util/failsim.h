/* failsim.h
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef FAILSIM_H_
#define FAILSIM_H_

#include <stdbool.h>
#include <glib.h>


// Issue:  Where to send error messages?


// cf load dumpload load variants

bool fsim_load_control_from_gptrarray(GPtrArray * lines);

bool fsim_load_control_string(char * s);

bool fsim_load_control_file(char * fn);

int fsim_check_failure(char * fn, char * funcname);

#define FAILSIM() \
   do { \
      int __rcsim = fsim_check_failure(__FILE__, __FUNC__); \
      if (__rcsim)        \
         return __rcsim;  \
   } while{0};


#define FAILSIM_EXT(__addl_cmds) \
   do { \
      int __rcsim = fsim_check_failure(__FILE__, __FUNC__); \
      if (__rcsim) {      \
         __addl_cmds;     \
         return __rcsim;  \
      } \
   } while{0};



#endif /* FAILSIM_H_ */
