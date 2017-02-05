/* libdrm_util.c
 *
 * Created on: Feb 4, 2017
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <stdio.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "util/libdrm_util.h"

//
// Identifier name tables
//

#ifdef EXAMPLE

Value_Name_Title descriptor_type_table[] = {
      VN2( LIBUSB_DT_DEVICE,                "Device"),            // 0x01
      VN2( LIBUSB_DT_HUB,                   "Hub"),               // 0x29,
      VN2( LIBUSB_DT_SUPERSPEED_HUB,        "SuperSpeed Hub"),    // 0x2a,
      VN2( LIBUSB_DT_SS_ENDPOINT_COMPANION, "SuperSpeed Endpoint Companion"),  // 0x30
      VN_END2
};

#endif

Value_Name connector_type_table[] = {
   VALUE_NAME(DRM_MODE_CONNECTOR_Unknown   ), //   0
   VALUE_NAME(DRM_MODE_CONNECTOR_VGA       ), //   1
   VALUE_NAME(DRM_MODE_CONNECTOR_DVII      ), //   2
   VALUE_NAME(DRM_MODE_CONNECTOR_DVID      ), //   3
   VALUE_NAME(DRM_MODE_CONNECTOR_DVIA       ), //  4
   VALUE_NAME(DRM_MODE_CONNECTOR_Composite   ), // 5
   VALUE_NAME(DRM_MODE_CONNECTOR_SVIDEO     ), //  6
   VALUE_NAME(DRM_MODE_CONNECTOR_LVDS      ), //   7
   VALUE_NAME(DRM_MODE_CONNECTOR_Component ), //   8
   VALUE_NAME(DRM_MODE_CONNECTOR_9PinDIN    ), //  9
   VALUE_NAME(DRM_MODE_CONNECTOR_DisplayPort ), // 10
   VALUE_NAME(DRM_MODE_CONNECTOR_HDMIA      ), //  11
   VALUE_NAME(DRM_MODE_CONNECTOR_HDMIB      ), //  12
   VALUE_NAME(DRM_MODE_CONNECTOR_TV   ), // 13
   VALUE_NAME(DRM_MODE_CONNECTOR_eDP     ), // 14
   VALUE_NAME(DRM_MODE_CONNECTOR_VIRTUAL    ), //  15
   VALUE_NAME(DRM_MODE_CONNECTOR_DSI      ), //    16
   VALUE_NAME_END
};

// from libdrm/drm.h
#define DRM_MODE_PROP_BITMASK   (1<<5) /* bitmask of enumerated types */
// #define DRM_MODE_PROP_ATOMIC        0x80000000

/* extended-types: rather than continue to consume a bit per type,
 * grab a chunk of the bits to use as integer type id.
 */
#define DRM_MODE_PROP_EXTENDED_TYPE     0x0000ffc0
#define DRM_MODE_PROP_TYPE(n)           ((n) << 6)
// #define DRM_MODE_PROP_OBJECT            (DRM_MODE_PROP_TYPE(1))
// #define DRM_MODE_PROP_SIGNED_RANGE      (DRM_MODE_PROP_TYPE(2))




Value_Name drm_property_flag_table[] = {
   VALUE_NAME(DRM_MODE_PROP_PENDING),    // (1<<0) == x01
   VALUE_NAME(DRM_MODE_PROP_RANGE),      // (1<<1) == x02
   VALUE_NAME(DRM_MODE_PROP_IMMUTABLE),  // (1<<2) == x04
   VALUE_NAME(DRM_MODE_PROP_ENUM),       // (1<<3) == x88 /* enumerated type with text strings */
   VALUE_NAME(DRM_MODE_PROP_BLOB),       // (1<<4) == x10
   VALUE_NAME(DRM_MODE_PROP_BITMASK),   // defined in libdrm/drm.h
//   VALUE_NAME(DRM_MODE_PROP_ATOMIC), // defined in libdrm/drm.h   64 bit value
   VALUE_NAME_END
};



char * connector_type_name(Byte val) {
   return vn_name(connector_type_table, val);
}

// doesn't handle extended property types, inc DRM_MODE_PROP_OOBJECT, DRM_MODE_PROP_SIGNED_RANGE
// see libdrm/drm.h
char * interpret_property_flags_r(uint32_t flags, char * buffer, int bufsz) {
   interpret_named_flags(
            drm_property_flag_table,
            flags,
            buffer,
            bufsz,
            ", ");      //        sepstr);

   uint32_t extended_type = flags & DRM_MODE_PROP_EXTENDED_TYPE;
   if (extended_type) {
      char * extended_name = "other extended type";
      if (extended_type == DRM_MODE_PROP_OBJECT)
         extended_name = "DRM_MODE_PROP_OBJECT";
      else if (extended_type == DRM_MODE_PROP_OBJECT)
         extended_name = "DRM_MODE_PROP_SIGNED_RANGE";
      sbuf_append(buffer, bufsz, ", ", extended_name);
   }
   if (flags & DRM_MODE_PROP_ATOMIC)
      sbuf_append(buffer, bufsz, ", ", "DRM_MODE_PROP_ATOMIC");
   return buffer;

}

char * interpret_property_flags(uint32_t flags) {
   int bufsz = 150;
   static char property_flags_string[150];
   return interpret_property_flags_r(flags, property_flags_string, bufsz);
}



#ifdef EXAMPLE
char * endpoint_direction_title(Byte val) {
   return vn_title(endpoint_direction_table, val);
}
#endif

#ifdef REF
typedef enum {
   DRM_MODE_CONNECTED         = 1,
   DRM_MODE_DISCONNECTED      = 2,
   DRM_MODE_UNKNOWNCONNECTION = 3
} drmModeConnection;
#endif


   Value_Name drmModeConnection_table[] = {
      VALUE_NAME(DRM_MODE_CONNECTED   ), //   1
      VALUE_NAME(DRM_MODE_DISCONNECTED ), //   2
      VALUE_NAME(DRM_MODE_UNKNOWNCONNECTION), //   2
      VALUE_NAME_END
   };

  char * connector_status_name(drmModeConnection val) {
      return vn_name(drmModeConnection_table, val);
  }


//
// Report functions for libdrm data structures
//

#ifdef REF
typedef struct _drmModeRes {

    int count_fbs;
    uint32_t *fbs;

    int count_crtcs;
    uint32_t *crtcs;

    int count_connectors;
    uint32_t *connectors;

    int count_encoders;
    uint32_t *encoders;

    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
} drmModeRes, *drmModeResPtr;
#endif


char * join_ids(char * buf, int bufsz, uint32_t* vals, int ct) {
   buf[0] = '\0';
   if (ct > 0 && vals) {
      strncpy(buf, " -> ", bufsz);
      for (int ndx = 0; ndx < ct; ndx++) {
         char b2[20];
         snprintf(b2, 20, "%d", vals[ndx]);
         sbuf_append(buf, bufsz, " ",  b2) ;
      }
   }
   return buf;
}


void report_drmModeRes(drmModeResPtr  res, int depth) {
   int d1 = depth+1;
   char buf[200]; int bufsz=200;
   rpt_structure_loc("drmModeRes", res, depth);

   rpt_vstring(d1, "%-20s %d",   "count_fbs", res->count_fbs);
   join_ids(buf, bufsz, res->fbs, res->count_fbs);
   rpt_vstring(d1, "%-20s %p%s",   "fbs", res->fbs, buf);

   rpt_vstring(d1, "%-20s %d",  "count_crtcs", res->count_crtcs);
   join_ids(buf, bufsz, res->crtcs, res->count_crtcs);
   rpt_vstring(d1, "%-20s %p%s",   "crtcs", res->crtcs, buf);

   rpt_vstring(d1, "%-20s %d",  "count_connectors", res->count_connectors);
   join_ids(buf, bufsz, res->connectors, res->count_connectors);
   rpt_vstring(d1, "%-20s %p%s",  "connectors", res->connectors, buf);

   rpt_vstring(d1, "%-20s %d",  "count_encoders", res->count_encoders);
   join_ids(buf, bufsz, res->encoders, res->count_encoders);
   rpt_vstring(d1, "%-20s %p%s", "encoders", res->encoders, buf);

   rpt_vstring(d1, "%-20s %d",             "min_width", res->min_width);
   rpt_vstring(d1, "%-20s %d",             "max_width", res->max_width);
   rpt_vstring(d1, "%-20s %d",             "min_height", res->min_height);
   rpt_vstring(d1, "%-20s %d",             "max_height", res->max_height);

}


#ifdef REF
typedef struct _drmModeModeInfo {
   uint32_t clock;
   uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
   uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

   uint32_t vrefresh;

   uint32_t flags;
   uint32_t type;
   char name[DRM_DISPLAY_MODE_LEN];
} drmModeModeInfo, *drmModeModeInfoPtr;
#endif

void summarize_drmmModeModeInfo(drmModeModeInfo * p, int depth) {
    // int d1 = depth+1;

    rpt_vstring(depth, "mode: %s", p->name);
    // rpt_vstring(d1, "flags:  0x%08x - %s", p->flags, "");
    // rpt_vstring(d1, "type:   %d = %s", p->type, "");
}




#ifdef NO
// For struct in drm.h

void report_drm_mode_card_res(struct drm_mode_card_res * res, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("drm_mode_card_res", res, depth);
   rpt_vstring(d1, "%-20s, %ld 0x%08x, %p", "fb_id_ptr:", res->fb_id_ptr, res->fb_id_ptr, res->fb_id_ptr);
   rpt_vstring(d1, "%-20s, %d",             "count_fbs", res->count_fbs);
   rpt_vstring(d1, "%-20s, %d",             "count_crtcs", res->count_crtcs);
   rpt_vstring(d1, "%-20s, %d",             "count_connectors", res->count_connectors);
   rpt_vstring(d1, "%-20s, %d",             "count_encoders", res->count_encoders);
   rpt_vstring(d1, "%-20s, %d",             "min_width", res->min_width);
   rpt_vstring(d1, "%-20s, %d",             "max_width", res->max_width);
   rpt_vstring(d1, "%-20s, %d",             "min_height", res->min_height);
   rpt_vstring(d1, "%-20s, %d",             "max_height", res->max_height);

}
#endif

#ifdef NO
// This is struct in drm.h, not wrapper libdrm.h

#ifdef REF
struct drm_mode_get_connector {

   __u64 encoders_ptr;
   __u64 modes_ptr;
   __u64 props_ptr;
   __u64 prop_values_ptr;

   __u32 count_modes;
   __u32 count_props;
   __u32 count_encoders;

   __u32 encoder_id; /**< Current Encoder */
   __u32 connector_id; /**< Id */
   __u32 connector_type;
   __u32 connector_type_id;

   __u32 connection;
   __u32 mm_width;  /**< width in millimeters */
   __u32 mm_height; /**< height in millimeters */
   __u32 subpixel;

   __u32 pad;
};
#endif

void report_drm_mode_get_connector(struct drm_mode_get_connector * p, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("drm_mode_get_connector", p, depth);
   rpt_vstring(d1, "%-20s, %ld 0x%08x, %p", "props_ptr:", p->props_ptr, p->props_ptr, p->props_ptr);
   rpt_vstring(d1, "%-20s, %u",             "count_modes", p->count_modes);
   rpt_vstring(d1, "%-20s, %u",             "count_props", p->count_props);
   rpt_vstring(d1, "%-20s, %u",             "count_encoders", p->count_encoders);
   rpt_vstring(d1, "%-20s, %u",             "encouder_id", p->encoder_id);   // current encoder
   rpt_vstring(d1, "%-20s, %d",             "connector_id", p->connector_id);
   rpt_vstring(d1, "%-20s, %d",             "connector_type", p->connector_type);
   rpt_vstring(d1, "%-20s, %d",             "connector_type_id", p->connector_type_id);
   rpt_vstring(d1, "%-20s, %d",             "connection", p->connection);
   rpt_vstring(d1, "%-20s, %d",             "mm_width", p->mm_width);
   rpt_vstring(d1, "%-20s, %d",             "mm_height", p->mm_height);
   rpt_vstring(d1, "%-20s, %d",             "subpixel", p->subpixel);
  // rpt_vstring(d1, "%-20s, %d",             "pad", p->pad);

}
#endif

#ifdef REF
typedef struct _drmModeConnector {
   uint32_t connector_id;
   uint32_t encoder_id; /**< Encoder currently connected to */
   uint32_t connector_type;
   uint32_t connector_type_id;
   drmModeConnection connection;
   uint32_t mmWidth, mmHeight; /**< HxW in millimeters */
   drmModeSubPixel subpixel;

   int count_modes;
   drmModeModeInfoPtr modes;

   int count_props;
   uint32_t *props; /**< List of property ids */
   uint64_t *prop_values; /**< List of property values */

   int count_encoders;
   uint32_t *encoders; /**< List of encoder ids */
} drmModeConnector, *drmModeConnectorPtr;

#endif

void report_drmModeConnector( int fd, drmModeConnector * p, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   char buf[200];  int bufsz=200;
   rpt_structure_loc("drmModeConnector", p, depth);
   rpt_vstring(d1, "%-20s %d",       "connector_id:", p->connector_id);
   rpt_vstring(d1, "%-20s %d - %s",  "connector_type:",    p->connector_type,  connector_type_name(p->connector_type));
   rpt_vstring(d1, "%-20s %d",       "connector_type_id:", p->connector_type_id);

   rpt_vstring(d1, "%-20s %u",       "encoder_id", p->encoder_id);   // current encoder
#ifdef OLD
   rpt_vstring(d1, "%-20s %d",       "count_encoders", p->count_encoders);
   buf[0] = '\0';
   if (p->count_encoders > 0) {
      strncpy(buf,  " -> ", 100);
      for (int ndx = 0; ndx < p->count_encoders; ndx++) {
         snprintf(buf+strlen(buf), 100-strlen(buf), "%d  ", p->encoders[ndx]);
      }
   }
   rpt_vstring(d1, "%-20s %p%s",  "encoders", p->encoders, buf);
#endif

   rpt_vstring(d1, "%-20s %d",  "count_encoderrs", p->count_encoders);
   join_ids(buf, bufsz, p->encoders, p->count_encoders);
   rpt_vstring(d1, "%-20s %p%s",  "encoders", p->encoders, buf);


   rpt_vstring(d1, "%-20s %d",  "count_props", p->count_props);
   for (int ndx = 0; ndx < p->count_props; ndx++) {
       rpt_vstring(d2, "index=%d, property id (props)=%u, property value (prop_values)=%u  0x%08x",
                        ndx, p->props[ndx], p->prop_values[ndx], p->prop_values[ndx]);

       drmModePropertyPtr prop_ptr = drmModeGetProperty(fd, p->props[ndx]);
       if (prop_ptr) {
          report_property_value(fd, prop_ptr, p->prop_values[ndx], d2);
          drmModeFreeProperty(prop_ptr);
       }
       else {
          rpt_vstring(d2, "Unrecognized property id: %d, value=%u", p->props[ndx], p->prop_values[ndx]);
       }
   }

   rpt_nl();
   rpt_vstring(d1, "%-20s %d",  "count_modes", p->count_modes);
   for (int ndx = 0; ndx < p->count_modes; ndx++) {
      // p->nodes is a pointer to an array of struct _drmModeModeInfo, not an array of pointers
      summarize_drmmModeModeInfo(  p->modes+ndx, d2);
   }
#ifdef NO
   buf[0] = '\0';
   if (p->count_modes > 0) {
      strncpy(buf,  " -> ", bufsz);
      for (int ndx = 0; ndx < p->count_modes; ndx++) {
         snprintf(buf+strlen(buf), bufsz-strlen(buf), "%p  ", p->modes[ndx]);
      }
   }
   rpt_vstring(d1, "%-20s %p%s",  "modes", p->modes, buf);
#endif

   rpt_vstring(d1, "%-20s %d - %s",  "connection:", p->connection, connector_status_name(p->connection));
   rpt_vstring(d1, "%-20s %d",       "mm_width:", p->mmWidth);
   rpt_vstring(d1, "%-20s %d",       "mm_height:", p->mmHeight);
   rpt_vstring(d1, "%-20s %d",       "subpixel:", p->subpixel);

   rpt_nl();
}


#ifdef REF
typedef struct _drmModeProperty {
   uint32_t prop_id;
   uint32_t flags;
   char name[DRM_PROP_NAME_LEN];
   int count_values;
   uint64_t *values; /* store the blob lengths */
   int count_enums;
   struct drm_mode_property_enum *enums;
   int count_blobs;
   uint32_t *blob_ids; /* store the blob IDs */
} drmModePropertyRes, *drmModePropertyPtr;
#endif



void report_drmModePropertyBlob(drmModePropertyBlobPtr blob_ptr, int depth) {
   rpt_vstring(depth, "blob id: %u\n", blob_ptr->id);
   rpt_hex_dump(blob_ptr->data, blob_ptr->length, depth);
}

void report_property_value(
        int                  fd,
        drmModePropertyRes * prop_ptr,
        uint64_t             prop_value,
        int                  depth)
{
   int d1 = depth+1;
   rpt_vstring(depth, "Property id:   %d", prop_ptr->prop_id);
   rpt_vstring(d1, "Name:          %s", prop_ptr->name);
   rpt_vstring(d1, "Flags:         0x%04x - %s", prop_ptr->flags, interpret_property_flags(prop_ptr->flags) );
   rpt_vstring(d1, "prop_value:    %d  0x%08x", prop_value, prop_value);

   if (prop_ptr->flags & DRM_MODE_PROP_ENUM) {
      for (int i = 0; i < prop_ptr->count_enums; i++) {
         if (prop_ptr->enums[i].value == prop_value) {
            rpt_vstring(d1, "Property value(enum) = %d - %s", prop_value, prop_ptr->enums[i].name);
            break;
         }
      }
   }
   else if (prop_ptr->flags & DRM_MODE_PROP_BITMASK) {
      char buf[200] = "";  int bufsz=200;
      bool not_truncated = true;
      for (int i = 0; i < prop_ptr->count_enums && not_truncated; i++) {
           if (prop_ptr->enums[i].value & prop_value) {
              not_truncated = sbuf_append(buf, bufsz, ", ", prop_ptr->enums[i].name);

           }
        }
      rpt_vstring(d1, "Property value(bitmask) = 0x%04x - %s", prop_value, buf);
   }

   if (prop_ptr->flags & DRM_MODE_PROP_RANGE) {
      if (prop_ptr->count_values != 2) {
         printf("Missing min or max value\n");
         rpt_vstring(d1, "Property value = %d, Missing range", prop_value);
      }
      else {
         rpt_vstring(d1, "Property value(range) = %d, min=%d, max=%d",
                         prop_value, prop_ptr->values[0], prop_ptr->values[1]);
      }
   }

   if (prop_ptr->flags & DRM_MODE_PROP_BLOB) {
      drmModePropertyBlobPtr blob_ptr = drmModeGetPropertyBlob(fd, prop_value);
      if (!blob_ptr) {
         printf("Blob not found\n");
      }
      else {
        report_drmModePropertyBlob(blob_ptr, d1);

         drmModeFreePropertyBlob(blob_ptr);
      }
   }

   else if (prop_ptr->flags & DRM_MODE_PROP_EXTENDED_TYPE) {
      uint32_t extended_type = prop_ptr->flags & DRM_MODE_PROP_EXTENDED_TYPE;
      if (extended_type == DRM_MODE_PROP_OBJECT) {
         rpt_vstring(d1, "Object type, name = %s, value=%d",
                         prop_ptr->name, prop_value);
      }
      else if (extended_type == DRM_MODE_PROP_SIGNED_RANGE) {
         if (prop_ptr->count_values != 2) {
            printf("Missing min or max value\n");
            rpt_vstring(d1, "Signed property value = %d, Missing range", prop_value);
         }
         else {
            rpt_vstring(d1, "Property value(range) = %d, min=%d, max=%d",
                            (int64_t) prop_value, (int64_t) prop_ptr->values[0], (int64_t) prop_ptr->values[1]);
         }

      }
      else {
         rpt_vstring(d1, "Other extended type %d, value=%d",
                         extended_type, prop_value);
      }
   }
}

#ifdef UGH


// this belongs in property description, not in value dump

      if (prop->flags & DRM_MODE_PROP_BLOB) {
         printf("\t\tblobs:\n");
         for (i = 0; i < prop->count_blobs; i++)
            dump_blob(dev, prop->blob_ids[i]);
         printf("\n");
      } else {
         assert(prop->count_blobs == 0);
      }

      printf("\t\tvalue:");
      if (prop->flags & DRM_MODE_PROP_BLOB)
         dump_blob(dev, value);
      else
   printf(" %"PRIu64"\n", value);


#endif




void report_drm_modeProperty(drmModePropertyRes * p, int depth) {
   rpt_structure_loc("drmModePropertyRes", p, depth);
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(d1, "%-20s %d",          "prop_id:", p->prop_id);
   rpt_vstring(d1, "%-20s 0x%08x - %s", "flags:", p->flags, interpret_property_flags(p->flags));
   rpt_vstring(d1, "%-20s %s",          "name:", p->name);     // null terminated?
   rpt_vstring(d1, "%-20s %d",          "count_values:", p->count_values);
   for (int ndx = 0; ndx < p->count_values; ndx++) {
      rpt_vstring(d2, "values[%d] = %lu", ndx, p->values[ndx]);
   }
   rpt_vstring(d1, "%-20s %d",          "count_enums:", p->count_enums);
   for (int ndx = 0; ndx < p->count_enums; ndx++) {
      rpt_vstring(d2, "enums[%d] = %u: %s", ndx, p->enums[ndx].value,  p->enums[ndx].name);
   }
   rpt_vstring(d1, "%-20s %d",          "count_blobs:", p->count_blobs);
   for (int ndx = 0; ndx < p->count_blobs; ndx++) {
      rpt_vstring(d2, "blob_ids[%d] = %u", ndx, p->blob_ids[ndx]);
   }
}

void summarize_drm_modeProperty(drmModePropertyRes * p, int depth) {
   rpt_vstring(depth, "Property %2d:  %-20s flags: 0x%08x - %s",
                      p->prop_id,
                      p->name,
                      p->flags,
                      interpret_property_flags(p->flags)
              );
}

