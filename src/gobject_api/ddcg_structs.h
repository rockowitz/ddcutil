/* ddcg_structs.h
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

#ifndef DDCG_STRUCTS_H_
#define DDCG_STRUCTS_H_

#include <glib-2.0/glib.h>
#include <glib-2.0/glib-object.h>

#include "ddcg_types.h"



//
// Build Information
//

// /** ddcutil version */

typedef struct _DdcgDdcutilVersionSpec {
   GObject    parent_instance;
   uint8_t    major;
   uint8_t    minor;
   uint8_t    micro;
} DdcgDdcutilVersionSpec;    //  DDCA_Ddcutil_Version_Spec;


typedef struct {
   uint8_t   v[3];
} DdcgDdcutilVersionSpec2;

// /**
//  * DDCA_Ddcutil_Version_Spec: (rename-to DdcgDdcutilVersionSpec);
//  *
//  */


// typedef DDCA_Ddcutil_Version_Spec DdcgDdcutilVersionSpec;



G_BEGIN_DECLS



#define DDCG_TYPE_DDCUTIL_VERSION_SPEC (ddcg_ddcutil_version_spec_get_type())

G_DECLARE_FINAL_TYPE(DdcgDdcutilVersionSpec, ddcg_ddcutil_version_spec, DDCG, DDCUTIL_VERSION_SPEC, GObject)

// struct _DdcgDdcutilVersionSpecClass {
//    GObjectClass parent_class;
// };


#define DDCG_DDCUTIL_VERSION_SPEC_ERROR       (ddcg_ddcutil_version_spec_quark ())
#define DDCG_DDCUTIL_VERSION_SPEC_TYPE_ERROR  (ddcg_ddcutil_version_spec_get_type ())

DdcgDdcutilVersionSpec * ddcg_ddcutil_version_spec_new(void);



G_END_DECLS




#endif /* DDCG_STRUCTS_H_ */
