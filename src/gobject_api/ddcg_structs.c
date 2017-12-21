/* ddcg_structs.c
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "base/core.h"

#include "ddcg_structs.h"


G_DEFINE_TYPE(DdcgDdcutilVersionSpec, ddcg_ddcutil_version_spec, G_TYPE_OBJECT)

static void ddcg_ddcutil_version_spec_class_init(DdcgDdcutilVersionSpecClass * cls);
static void ddcg_ddcutil_version_spec_init(DdcgDdcutilVersionSpec * ddcutil_version_spec);


static void ddcg_ddcutil_version_spec_class_init(DdcgDdcutilVersionSpecClass * cls) {
   DBGMSG("Starting");
}


static void ddcg_ddcutil_version_spec_init(DdcgDdcutilVersionSpec * ddcutil_version_spec) {
   DBGMSG("Starting");
   // initialize the instance
}


DdcgDdcutilVersionSpec * ddcg_ddcutil_version_spec_new(void) {
   return g_object_new(DDCG_TYPE_DDCUTIL_VERSION_SPEC, NULL);
}
