/* ddcg_display_identifier.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDCG_DISPLAY_IDENTIFIER_H_
#define DDCG_DISPLAY_IDENTIFIER_H_

// #include <glib.h>
#include <glib-object.h>
#include <glib-2.0/glib-object.h>
// #include <gio/gio.h>

#include "libmain/ddct_public.h"   // for ddcg_display_identifier_get_ddct_object()

G_BEGIN_DECLS

#define DDCG_TYPE_DISPLAY_IDENTIFIER (ddcg_display_identifier_get_type())
G_DECLARE_FINAL_TYPE(DdcgDisplayIdentifier, ddcg_display_identifier, DDCG, DISPLAY_IDENTIFIER, GObject)


#ifdef OLD
typedef struct _DdcgDisplayIdentifier            DdcgDisplayIdentifier;
typedef struct _DdcgDisplayIdentifierClass       DdcgDisplayIdentifierClass;
#endif

DdcgDisplayIdentifier *
ddcg_display_identifier_new(void);


DdcgDisplayIdentifier *
ddcg_display_identifier_create_busno_identifier(
      gint32                  busno,
      GError **               error);

DdcgDisplayIdentifier*
ddcg_display_identifier_create_adlno_identifier(
      gint32      adapter_index,
      gint32      display_index,
      gint32      busno,
      GError **   error);

DdcgDisplayIdentifier*
ddcg_display_identifier_create_model_sn_identifier(
      const gchar *  model,
      const gchar *  sn,
      GError **      error);

DdcgDisplayIdentifier*
ddcg_display_identifier_create_usb_identifier(
      gint32      bus,
      gint32      device,
      GError **   error);


DdcgDisplayIdentifier *
ddcg_display_identifier_create_dispno_identifier(
      gint32                  dispno,
      GError **               error);

gchar *
ddcg_display_identifier_repr(
      DdcgDisplayIdentifier * ddcg_did,
      GError**                error);

void
ddcg_display_identifier_report(
      DdcgDisplayIdentifier * ddcg_did,
      int                     depth);

DDCA_Display_Identifier
_ddcg_display_identifier_get_ddct_object(
      DdcgDisplayIdentifier * ddcg_did);

G_END_DECLS

#ifdef NO_SUBCLASS

typedef struct _DdcgBusnoDisplayIdentifier       DdcgBusnoDisplayIdentifier;
typedef struct _DdcgBusnoDisplayIdentifierClass  DdcgBusnoDisplayIdentifierClass;

struct _DdcgDisplayIdentifierClass {
   GObjectClass parent_class;

   /* private */
   gpointer padding[10];
};

#ifdef DEFINED_BY_G_DECLARE_DERIVABLE_TYPE
struct _DdcgDisplayIdentifier {
   GObject   parent_instance;
};
#endif


struct _DdcgBusnoDisplayIdentifierClass {
   DdcgDisplayIdentifierClass parent_class;
};

struct _DdcgBusnoDisplayIdentifier {
   DdcgDisplayIdentifier parent_instance;
   gchar                   busno;
};

#endif


#ifdef OLD
#define DDCG_TYPE_DISPLAY_IDENTIFIER (ddcg_display_identifier_get_type())
#define DDCG_DISPLAY_IDENTIFIER(o)           \
   (G_TYPE_CHECK_INSTANCE_CAST ((o), DDCG_TYPE_DISPLAY_IDENTIFIER, DdcgDisplayIdentifier))
#define DDCG_DISPLAY_IDENTIFIER_CLASS(k)     \
   (G_TYPE_CHECK_CLASS_CAST((k), DDCG_TYPE_DISPLAY_IDENTIFIER, DdcgDisplayIdentifierClass))
#define DDCG_IS_DISPLAY_IDENTIFIER(o)        \
   (G_TYPE_CHECK_INSTANCE_TYPE ((o), DDCG_TYPE_DISPLAY_IDENTIFIER))
#define DDCG_IS_DISPLAY_IDENTIFIER_CLASS(k)  \
   (G_TYPE_CHECK_CLASS_TYPE ((k), DDCG_TYPE_DISPLAY_IDENTIFIER))
#define DDCG_DISPLAY_IDENTIFIER_GET_CLASS(o) \
   (G_TYPE_INSTANCE_GET_CLASS ((o), DDCG_TYPE_DISPLAY_IDENTIFIER, DdcgDisplayIdentifierClass))
#define DDCG_DISPLAY_IDENTIFIER_ERROR    (ddcg_display_identifier_quark ())
#define DDCG_DISPLAY_IDENTIFIER_TYPE_ERROR  (ddcg_display_identifier_get_type ())
#endif


#ifdef NO_SUBCLASS
#define DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER (ddcg_busno_display_identifier_get_type())
#define DDCG_BUSNO_DISPLAY_IDENTIFIER(o)           \
   (G_TYPE_CHECK_INSTANCE_CAST ((o), DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER, DdcgDisplayIdentifier))
#define DDCG_BUSNO_DISPLAY_IDENTIFIER_CLASS(k)     \
   (G_TYPE_CHECK_CLASS_CAST((k), DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER, DdcgDisplayIdentifierClass))
#define DDCG_IS_BUSNO_DISPLAY_IDENTIFIER(o)        \
   (G_TYPE_CHECK_INSTANCE_TYPE ((o), DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER))
#define DDCG_IS_BUSNO_DISPLAY_IDENTIFIER_CLASS(k)  \
   (G_TYPE_CHECK_CLASS_TYPE ((k), DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER))
#define DDCG_BUSNO_DISPLAY_IDENTIFIER_GET_CLASS(o) \
   (G_TYPE_INSTANCE_GET_CLASS ((o), DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER, DdcgDisplayIdentifierClass))
#define DDCG_BUSNO_DISPLAY_IDENTIFIER_ERROR    (ddcg_busno_display_identifier_quark ())
#define DDCG_BUSNO_DISPLAY_IDENTIFIER_TYPE_ERROR  (ddcg_busno_display_identifier_get_type ())

#endif


#endif /* DDCG_DISPLAY_IDENTIFIER_H_ */
