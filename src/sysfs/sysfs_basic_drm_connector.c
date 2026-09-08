/** \file sysfs_basic_drm_connector.c
 *  A reduced view of a DRM card connector: only what is needed to set the
 *  connector fields of an #I2C_Bus_Info.
 *
 *  The scan here reads three attributes per connector -- the connector id, the
 *  EDID, and the bus numbers -- where scan_sys_drm_connectors() also reads the
 *  realpath, enabled and status, and fills in fields describing the adapter
 *  behind the connector.  Nothing that sets a bus record's connector consults
 *  those, so this scan does not pay for them.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util/edid.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/rtti.h"

#include "sysfs_base.h"
#include "sysfs_basic_drm_connector.h"

static const DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_SYSFS;


/** Frees one instance.  Suitable as a #GDestroyNotify. */
void free_basic_drm_connector(void * connector) {
   if (connector) {
      Sys_Basic_Drm_Connector * cur = connector;
      free(cur->connector_name);
      free(cur->edid_bytes);
      free(cur);
   }
}


/** Frees an array returned by #scan_basic_drm_connectors(), and its contents. */
void free_basic_drm_connectors(GPtrArray * connectors) {
   if (connectors)
      g_ptr_array_free(connectors, true);
}


/** Reads one connector directory.
 *
 *  @param  dirname  /sys/class/drm
 *  @param  fn       connector directory name, e.g. card1-DP-1
 *  @param  depth    logical indentation depth, -1 for no reporting
 *  @return instance, caller frees
 */
static Sys_Basic_Drm_Connector * one_basic_drm_connector0(
      const char * dirname, const char * fn, int depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "dirname=%s, fn=%s", dirname, fn);
   int d0 = depth;

   Sys_Basic_Drm_Connector * cur = calloc(1, sizeof(Sys_Basic_Drm_Connector));
   cur->connector_name = g_strdup(fn);
   cur->connector_id   = -1;
   cur->i2c_busno      = -1;   // 0 is a valid bus number

   RPT_ATTR_INT(d0, &cur->connector_id, dirname, fn, "connector_id");

   // Forces the driver to re-probe the connector before the EDID is read, on
   // the drivers where that is needed and only when the user asked for it with
   // --f24.  Same call the full scan makes; skipping it would make this scan
   // quietly different rather than merely cheaper.
   POSSIBLY_WRITE_DETECT_TO_STATUS_BY_CONNECTOR_NAME(fn);

   GByteArray * edid_byte_array = NULL;
   RPT_ATTR_EDID(d0, &edid_byte_array, dirname, fn, "edid");
   if (edid_byte_array) {
      cur->edid_size  = edid_byte_array->len;
      cur->edid_bytes = g_byte_array_free(edid_byte_array, false);
   }

   // The bus number, where the driver publishes it.  Also yields a connector
   // id on drivers that expose it here rather than as an attribute, so it is
   // taken only when the attribute read above found nothing.
   Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
   get_connector_bus_numbers(dirname, fn, cbn);
   cur->i2c_busno = cbn->i2c_busno;
   if (cur->connector_id < 0)
      cur->connector_id = cbn->connector_id;
   free_connector_bus_numbers(cbn);

   DBGTRC_DONE(debug, TRACE_GROUP, "%s: i2c_busno=%d, connector_id=%d, edid_size=%d",
         cur->connector_name, cur->i2c_busno, cur->connector_id, (int) cur->edid_size);
   return cur;
}


/** Accumulator callback for #dir_filtered_ordered_foreach(). */
static void one_basic_drm_connector(
      const char * dirname, const char * fn, void * accumulator, int depth)
{
   Sys_Basic_Drm_Connector * cur = one_basic_drm_connector0(dirname, fn, depth);
   if (cur)
      g_ptr_array_add((GPtrArray *) accumulator, cur);
}


/** Reads /sys/class/drm once and returns the connectors found.
 *
 *  The array is returned rather than held in a global: nothing maintains it,
 *  and a caller that keeps one is responsible for its lifetime.  Free it with
 *  #free_basic_drm_connectors().
 *
 *  @param  depth  logical indentation depth for reporting, -1 for none
 *  @return array of #Sys_Basic_Drm_Connector, never NULL, possibly empty
 */
GPtrArray * scan_basic_drm_connectors(int depth)  {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "depth=%d", depth);

   GPtrArray * connectors = g_ptr_array_new_with_free_func(free_basic_drm_connector);
   dir_filtered_ordered_foreach(
         "/sys/class/drm",
         is_drm_connector,      // filter function
         NULL,                  // ordering function
         one_basic_drm_connector,
         connectors,            // accumulator
         depth);

   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %d connectors", connectors->len);
   return connectors;
}


/** Finds the connector a bus number belongs to.
 *
 *  @param  connectors  array to search
 *  @param  busno       I2C bus number
 *  @return pointer into the array, NULL if no connector names that bus.
 *          Valid only as long as the array is.
 */
Sys_Basic_Drm_Connector * find_basic_drm_connector_by_busno(
      GPtrArray * connectors, int busno)
{
   Sys_Basic_Drm_Connector * result = NULL;
   if (connectors && busno >= 0) {
      for (int ndx = 0; ndx < connectors->len; ndx++) {
         Sys_Basic_Drm_Connector * cur = g_ptr_array_index(connectors, ndx);
         if (cur->i2c_busno == busno) {
            result = cur;
            break;
         }
      }
   }
   return result;
}


/** Finds the connector showing an EDID.
 *
 *  @param  connectors  array to search
 *  @param  edid_bytes  128 bytes to compare
 *  @return pointer into the array, NULL if no connector shows it.
 *          Valid only as long as the array is.
 *
 *  @remark
 *  The first match wins.  Two connectors can show the same EDID -- one monitor
 *  reachable by two cables -- and there is nothing here to tell them apart.
 */
Sys_Basic_Drm_Connector * find_basic_drm_connector_by_edid(
      GPtrArray * connectors, Byte * edid_bytes)
{
   Sys_Basic_Drm_Connector * result = NULL;
   if (connectors && edid_bytes) {
      for (int ndx = 0; ndx < connectors->len; ndx++) {
         Sys_Basic_Drm_Connector * cur = g_ptr_array_index(connectors, ndx);
         if (cur->edid_bytes && cur->edid_size >= 128 &&
             memcmp(cur->edid_bytes, edid_bytes, 128) == 0)
         {
            result = cur;
            break;
         }
      }
   }
   return result;
}


void dbgrpt_basic_drm_connector(Sys_Basic_Drm_Connector * connector, int depth) {
   if (!connector) {
      rpt_label(depth, "Sys_Basic_Drm_Connector: NULL");
      return;
   }
   rpt_vstring(depth, "Sys_Basic_Drm_Connector at %p", (void *) connector);
   rpt_vstring(depth+1, "connector_name:  %s", connector->connector_name);
   rpt_vstring(depth+1, "connector_id:    %d", connector->connector_id);
   rpt_vstring(depth+1, "i2c_busno:       %d", connector->i2c_busno);
   // Reported as bytes rather than as a parsed summary: the summary helpers
   // live in the i2c layer, which sysfs must not depend on.
   if (connector->edid_bytes)
      rpt_vstring(depth+1, "edid:            %d bytes, starting %s",
            (int) connector->edid_size, hexstring_t(connector->edid_bytes, 8));
   else
      rpt_vstring(depth+1, "edid:            none");
}


void dbgrpt_basic_drm_connectors(GPtrArray * connectors, int depth) {
   if (!connectors) {
      rpt_label(depth, "No basic DRM connectors");
      return;
   }
   rpt_vstring(depth, "%d basic DRM connector(s):", connectors->len);
   for (int ndx = 0; ndx < connectors->len; ndx++)
      dbgrpt_basic_drm_connector(g_ptr_array_index(connectors, ndx), depth+1);
}


void init_sysfs_basic_drm_connector() {
   RTTI_ADD_FUNC(scan_basic_drm_connectors);
   RTTI_ADD_FUNC(one_basic_drm_connector0);
}
