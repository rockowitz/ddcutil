/** @file drm_connector_state.c */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 
#define _GNU_SOURCE

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <fcntl.h>    // for all_displays_drm2()
#include <inttypes.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for close() used by probe_dri_device_using_drm_api
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
/** \endcond */

#include "coredefs_base.h"
#include "data_structures.h"
#include "debug_util.h"
#include "drm_common.h"
#include "edid.h"
#include "file_util.h"
#include "libdrm_util.h"
#include "subprocess_util.h"
#include "report_util.h"
#include "string_util.h"
#include "sysfs_filter_functions.h"
#include "sysfs_util.h"

#include "drm_connector_state.h"



 char * get_busid_from_fd(int fd) {
    int depth = 0;
    int d1 = depth+1;
    int d2 = depth+2;
    bool debug = true;
    DBGF(debug, "Starting. fd=%d", fd);

    char * busid = NULL;
    struct _drmDevice * ddev;
    // gets information about the opened DRM device
    // returns 0 on success, negative error code otherwise
    int rc = drmGetDevice(fd, &ddev);
    if (rc < 0) {
        rpt_vstring(depth, "drmGetDevice() returned %d", rc);
              // interpreted as error code: %s",  rc, linux_errno_desc(-rc));
    }
    else {
       rpt_vstring(d1, "Device information:");
       rpt_vstring(d2, "bustype:                %d - %s",
              ddev->bustype, drm_bus_type_name(ddev->bustype));
       busid = g_strdup_printf("%s:%04x:%02x:%02x.%d",
              drm_bus_type_name(ddev->bustype),
              // "PCI",
              ddev->businfo.pci->domain,
              ddev->businfo.pci->bus,
              ddev->businfo.pci->dev,
              ddev->businfo.pci->func);

       if (debug) {
           rpt_vstring(d2, "domain:bus:device.func: %s", busid);
           rpt_vstring(d2, "vendor    vid:pid:      0x%04x:0x%04x",
                 ddev->deviceinfo.pci->vendor_id,
                 ddev->deviceinfo.pci->device_id);
           rpt_vstring(d2, "subvendor vid:pid:      0x%04x:0x%04x",
                 ddev->deviceinfo.pci->subvendor_id,
                 ddev->deviceinfo.pci->subdevice_id);
           rpt_vstring(d2, "revision id:            0x%04x",
                 ddev->deviceinfo.pci->revision_id);
       }
       drmFreeDevice(&ddev);
   }
    DBGF(debug, "Returning: %s", busid);
    return busid;
 }


typedef struct {
   char *  name;
   int     count;
   int *   values;
   char ** value_names;
} Enum_Metadata;


char * get_enum_value_name(Enum_Metadata * meta, int value) {
   char * result = "UNRECOGNIZED";
   for (int i = 0; i < meta->count; i++) {
      if (meta->values[i] == value) {
         result = meta->value_names[i];
         break;
      }
   }
   return result;
}


static const int EDID_PROP_ID         =  1;
static const int DPMS_PROP_ID         =  2;
static const int LINK_STATUS_PROP_ID  =  5;
static const int SUBCONNECTOR_PROP_ID = 69;
// int type_prop_id         = 0;

// metadata, need only collect once
static drmModePropertyRes * edid_metadata = NULL;
static Enum_Metadata *      subconn_metadata;
static Enum_Metadata *      dpms_metadata = NULL;
static Enum_Metadata *      link_status_metadata = NULL;


void free_enum_metadata(Enum_Metadata * meta) {
   if (meta) {
      free(meta->name);
      if (meta->values) {
         free(meta->values);
      }
      free(meta);
   }
}


void dbgrpt_enum_metadata(int depth, Enum_Metadata * meta) {
   rpt_structure_loc("Enum_Metatdata", meta, depth);
   int d1 = depth+1;
   rpt_vstring(d1, "Name:  %s", meta->name);
   for (int ndx = 0; ndx < meta->count; ndx++)
      rpt_vstring(d1, "%2d  %s", meta->values[ndx], meta->value_names);
}


 Enum_Metadata * drmModePropertyRes_to_enum_metadata(drmModePropertyRes * prop) {
    bool debug = false;
    DBGF(debug, "Starting.  prop=%p", prop);
    Enum_Metadata * meta = calloc(1, sizeof(Enum_Metadata));
    meta->name = strdup(prop->name);

    DBGF(debug, "prop->name = %s", prop->name);
    DBGF(debug, "prop->count_enums = %d", prop->count_enums);
    DBGF(debug, "prop->count_values = %d", prop->count_values);
    meta->count = prop->count_enums;
    meta->values = calloc(prop->count_enums, sizeof(uint64_t));
    meta->value_names = calloc(prop->count_enums, sizeof(char*));
    for (int ndx = 0; ndx < meta->count; ndx++) {
       struct drm_mode_property_enum * dmpe = prop->enums;
       DBGF(debug, "prop->enums == dmpe = %p", prop->enums);
       // DBGF(debug, "prop->values[%d] = %s", ndx, prop->values[ndx]);
       meta->values[ndx] = dmpe->value;
       meta->value_names[ndx] = strdup(dmpe->name);
    }
    DBGF(debug, "Done.   Returning %p", meta);
    return meta;
 }


 void free_drm_connector_state(void * cs) {
    Drm_Connector_State * cstate = (Drm_Connector_State*) cs;
    if (cstate) {
       if (cstate->edid)
          free_parsed_edid(cstate->edid);
       free(cstate);
    }
 }


void store_property_value(
      int                    fd,
      Drm_Connector_State *  connector_state,
      drmModePropertyRes  *  prop_ptr,
      uint64_t               prop_value)
{
   bool debug = false;
   DBGF(debug, "Starting.  fd=%d. connector_state=%p, connector_state->connector_id=%d, prop_ptr=%p, prop_ptr->prop_id=%d, prop_value=%d",
          fd, connector_state, connector_state->connector_id, prop_ptr, prop_ptr->prop_id, prop_value);

   int d1 = 1;
   switch(prop_ptr->prop_id) {
   case EDID_PROP_ID:
      assert( prop_ptr->flags & DRM_MODE_PROP_BLOB);
      if (!edid_metadata) edid_metadata = prop_ptr;
      int blob_id = prop_value;
      drmModePropertyBlobPtr blob_ptr = drmModeGetPropertyBlob(fd, blob_id);
      if (!blob_ptr) {
         if (debug)
         rpt_vstring(d1, "Blob not found");
       }
       else {
          if (debug)
             rpt_hex_dump(blob_ptr->data, blob_ptr->length, d1);

          if (blob_ptr->length >= 128) {
             connector_state->edid = create_parsed_edid2(blob_ptr->data, "DRM");
          }
          else {
             rpt_vstring(d1, "invalid edid length: %d", blob_ptr->length);
          }

          drmModeFreePropertyBlob(blob_ptr);
       }
      break;

   case SUBCONNECTOR_PROP_ID:
      assert( prop_ptr->flags & DRM_MODE_PROP_ENUM);
      if (!subconn_metadata)
         subconn_metadata = drmModePropertyRes_to_enum_metadata(prop_ptr);
      connector_state->subconnector = prop_value;
      break;

   case DPMS_PROP_ID:
      assert( prop_ptr->flags & DRM_MODE_PROP_ENUM);
      if (!dpms_metadata)
         dpms_metadata = drmModePropertyRes_to_enum_metadata(prop_ptr);
      connector_state->dpms = prop_value;
      break;

   case LINK_STATUS_PROP_ID:
      assert( prop_ptr->flags & DRM_MODE_PROP_ENUM);
      if (!link_status_metadata)
         link_status_metadata = drmModePropertyRes_to_enum_metadata(prop_ptr);
      connector_state->link_status = prop_value;
      break;

   default:
      break;
   }
   DBGF(debug, "Done");
}


// Returns array of DRM_Connector_State for one card
int get_connector_state_array(int fd, int cardno, GPtrArray* collector) {
   bool debug = false;
   DBGF(debug, "Starting.  fd=%d, cardno=%d, collector=%p", fd, cardno, collector);
   // int depth = 0;
   int d1 = 1;
   int d2 = 2;

   DBGF(debug, "Retrieving DRM resources...");
   drmModeResPtr res = drmModeGetResources(fd);
   DBGF(debug,"res=%p", (void*)res);
   if (!res) {
      int errsv = errno;
      rpt_vstring(d1, "Failure retrieving DRM resources, errno=%d=%s", errsv, strerror(errsv));
      if (errsv == EINVAL)
         rpt_vstring(d1,"Driver apparently does not provide needed DRM ioctl calls");
      DBGF(debug, "Done.   Returning %d", errsv);
      return errsv;
   }
   if (debug)
      report_drmModeRes(res, d2);

   DBGF(debug, "Scanning connectors for card %d ...", cardno);
   for (int i = 0; i < res->count_connectors; ++i) {
      DBGF(debug, "calling drmModeGetConnector for id %d", res->connectors[i]);

      /* Doc for drmModeGetConnector in xf86drmMode.h:
       *
       * Retrieve all information about the connector connectorId. This will do a
       * forced probe on the connector to retrieve remote information such as EDIDs
       * from the display device.
       */
      drmModeConnector * conn = drmModeGetConnector(fd, res->connectors[i]);
      if (!conn) {
         rpt_vstring(d1, "Cannot retrieve DRM connector id %d errno=%s",
                         res->connectors[i], strerrorname_np(errno));
         continue;
      }

      if (debug)
        report_drmModeConnector(fd, conn, d1) ;
      Drm_Connector_State * connector_state = calloc(1,sizeof(Drm_Connector_State));
      connector_state->cardno       = cardno;
      connector_state->connector_id = res->connectors[i];
      if (debug) {
         int depth=2;
         rpt_structure_loc("drmModeConnector", conn, depth);
         rpt_vstring(d1, "%-20s %d",       "connector_id:", conn->connector_id);
         rpt_vstring(d1, "%-20s %d - %s",  "connector_type:",    conn->connector_type,  connector_type_name(conn->connector_type));
         rpt_vstring(d1, "%-20s %d",       "connector_type_id:", conn->connector_type_id);
         rpt_vstring(d1, "%-20s %d - %s",  "connection:",        conn->connection, connector_status_name(conn->connection));
      }
      connector_state->connector_type = conn->connector_type;;
      connector_state->connector_type_id = conn->connector_type_id;
      connector_state->connection = conn->connection;
      drmModeConnector * p = conn;
      if (debug)
         rpt_vstring(d1, "%-20s %d",  "count_props", p->count_props);
      for (int ndx = 0; ndx < p->count_props; ndx++) {
           uint64_t curval = p->prop_values[ndx];   // coverity workaround
           if (debug) {
              rpt_vstring(d2, "index=%d, property id (props)=%" PRIu32 ", property value (prop_values)=%" PRIu64 ,
                               ndx, p->props[ndx], curval);
           }
           int id = p->props[ndx];
           if (id == EDID_PROP_ID         ||      //  1
               id == DPMS_PROP_ID         ||      //  2
               id == LINK_STATUS_PROP_ID  ||      //  5
               id == SUBCONNECTOR_PROP_ID)        // 69
           {
              drmModePropertyPtr metadata_ptr = drmModeGetProperty(fd, p->props[ndx]);
              if (metadata_ptr) {
                 uint64_t  prop_value = p->prop_values[ndx];
                 if (debug)
                    report_property_value(fd, metadata_ptr, prop_value, d2);
                 store_property_value(fd, connector_state, metadata_ptr, prop_value);
                 drmModeFreeProperty(metadata_ptr);
              }
           }
      } // for
      g_ptr_array_add(collector, connector_state);
   }
   return 0;
}



void dbgrpt_connector_state(Drm_Connector_State * state, int depth
      ) {
   rpt_structure_loc("Drm_Connector_State", state, depth);
   int d1 = depth+1;
   int d2 = depth+2;

   rpt_vstring(d1, "cardno:               %d", state->cardno);
   rpt_vstring(d1, "connector_id:         %d", state->connector_id);

   // rpt_vstring(d1, "%-20s %d",       "connector_id:", conn->connector_id);
   rpt_vstring(d1, "%-20s %d - %s",  "connector_type:",    state->connector_type,  connector_type_name(state->connector_type));
   rpt_vstring(d1, "%-20s %d",       "connector_type_id:", state->connector_type_id);
   rpt_vstring(d1, "%-20s %d - %s",  "connection:",        state->connection, connector_status_name(state->connection));

   rpt_vstring(d2, "Properties:");
   char * vname = get_enum_value_name(dpms_metadata, state->dpms);
   rpt_vstring(d2, "dpms:              %d - %s", state->dpms, vname);

   vname = get_enum_value_name(link_status_metadata, state->link_status);
   rpt_vstring(d2, "link_status:       %d - %s", state->link_status, vname);

   vname = get_enum_value_name(subconn_metadata, state->subconnector);
   rpt_vstring(d2, "subconnector:      %d- %s", state->subconnector, vname);

   if (state->edid) {
      rpt_vstring(d1, "edid:");
      report_parsed_edid(state->edid, true, d1);
   }
   rpt_nl();

}

void dbgrpt_connector_states(GPtrArray* states) {
   assert(states);
   rpt_structure_loc("GPtrArray", states, 0);
   for (int ndx = 0; ndx < states->len; ndx++) {
      Drm_Connector_State * cur =  g_ptr_array_index(states, ndx);
      dbgrpt_connector_state(cur, 1);
   }
}


int get_drm_connector_states_by_fd(int fd, int cardno, GPtrArray* collector) {
   bool debug = false;
   bool replace_fd = false;
   bool verbose = false;
   int result = 0;
   DBGF(debug, "Starting.  fd=%d, cardno=%d, collector=%p, replace_busid=%s",
         fd, cardno, collector, sbool(replace_fd));

   // returns null string if open() instead of drmOpen(,busid) was used to to open
   // uses successive DRM_IOCTL_GET_UNIQUE calls
   char* busid = drmGetBusid(fd);
   if (busid && (verbose || debug) ) {
      rpt_vstring(1, "drmGetBusid() returned: |%s|", busid);
      // drmFreeBusid(busid);  // requires root
      free(busid);
   }
   else {
      if (verbose || debug)
         rpt_vstring(1, "Error calling drmGetBusid().  errno=%s", strerrorname_np(errno));
   }

   if (replace_fd) {
      busid = get_busid_from_fd(fd);
      DBGF(debug, "get_busid_from_fd() returned: %s", busid);
      close(fd);
      fd = drmOpen(NULL, busid);
      if (fd < 0) {
         result = -errno;
         DBGF(debug, "drmOpen(NULL, %s) failed. fd=%d, errno=%d - %s", busid, fd, errno, strerror(errno));
      }
      else {
         DBGF(debug, "drmOpen() succeeded");
      }
   }

   if (fd >= 0) {
      // try to set master
      int rc = drmSetMaster(fd);
      if (rc < 0 && (verbose || debug))
         rpt_vstring(1,"(%s) drmSetMaster() failed, errno = %d - %s",
                       __func__, errno, strerror(errno));

      get_connector_state_array(fd, cardno, collector);
   }
   DBGF(debug, "Returning:   %d", result);
   return result;
}


Drm_Connector_State * get_drm_connector_state_by_fd(int fd, int cardno, int connector_id) {
   bool debug = false;
   DBGF(debug, "Starting.  fd=%d, connector_id=%d", fd, connector_id);

   GPtrArray * connector_state_array = g_ptr_array_new();
   g_ptr_array_set_free_func(connector_state_array, free_drm_connector_state);
   get_drm_connector_states_by_fd(fd, cardno, connector_state_array);
    // todo report connector_state_array

   Drm_Connector_State * result = NULL;
   if (connector_state_array)  {
      for (int ndx = 0; ndx < connector_state_array->len; ndx++) {
          Drm_Connector_State * cur = g_ptr_array_index(connector_state_array, ndx);
          if (cur->connector_id == connector_id) {
             result = cur;     // TODO: copy, then free array
             g_ptr_array_remove_index(connector_state_array, ndx);
             break;
          }
       }
      g_ptr_array_free(connector_state_array, true);
    }
   DBGF(debug, "Done.  Returning %p", result);
   return result;
}


int extract_cardno(const char * devname) {
   int cardno = -1;
   char * bn = g_path_get_basename(devname);
   if (!bn || strlen(bn) < 5 || memcmp(bn, "card", 4) != 0 || !g_ascii_isdigit(bn[4]) ) {
      rpt_vstring(1, "Invalid device name: %s", devname);
      free(bn);
   }
   else {
      cardno = g_ascii_digit_value(bn[4]);
   }
   free(bn);
   return cardno;
}

#ifdef UNUSED
Drm_Connector_State * get_drm_connector_state_by_devname(const char * devname, int connector_id) {
   // validate that devmame looks like /dev/dri/cardN
   int cardno = extract_cardno(devname);
   if (cardno < 0) {
      rpt_vstring(1, "Invalid device name: %s", devname);
      return NULL;
   }

   int fd  = open(devname,O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      rpt_vstring(1, "Error opening device %s using open(), errno=%s",
                                 devname, strerrorname_np(errno));
      return NULL;
   }

   return get_drm_connector_state_by_fd(fd, connector_id);
 }
#endif


int get_drm_connector_states_by_devname(const char * devname, bool verbose, GPtrArray * collector) {
   bool debug = false;
   DBGF(debug, "Starting.  devname=%s, verbose=%s, collector=%p", devname, sbool(verbose), collector);
   // validate that devmame looks like /dev/dri/cardN
   int cardno = extract_cardno(devname);
   if (cardno < 0) {
      rpt_vstring(1, "Invalid device name: %s", devname);
      return -EINVAL;
   }

   int fd  = open(devname,O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      int errsv = errno;
      rpt_vstring(1, "Error opening device %s using open(), errno=%s",
                                 devname, strerrorname_np(errsv));
      return -errsv;
   }

   DBGF(debug, "Calling get_drm_connector_states_by_fd():");
   int rc = get_drm_connector_states_by_fd(fd, cardno, collector);
   if (rc == 0  && (verbose || debug))
      dbgrpt_connector_states(collector);
   DBGF(debug, "Done.    collectors=%p", collector);
   return rc;
 }


GPtrArray* all_card_connector_states = NULL;


GPtrArray * drm_get_all_connector_states() {
   bool verbose = false;
   GPtrArray * devnames = get_dri_device_names_using_filesys();
   GPtrArray * allstates = g_ptr_array_new();
   g_ptr_array_set_free_func(allstates, free_drm_connector_state);
   for (int ndx = 0; ndx < devnames->len; ndx++) {
      char * driname = g_ptr_array_index(devnames, ndx);
      get_drm_connector_states_by_devname(driname, verbose, allstates);
   }
   return allstates;
}


void empty_drm_connector_states(GPtrArray * cstates) {
   if (cstates) {
      g_ptr_array_remove_range(cstates, 0, cstates->len);
   }
}


void free_drm_connector_states(GPtrArray * cstates) {
   if (cstates) {
      g_ptr_array_free(cstates, true);
      cstates = NULL;
   }
}

void redetect_drm_connector_states() {
   if (all_card_connector_states)
      free_drm_connector_states(all_card_connector_states);
   all_card_connector_states = drm_get_all_connector_states();
}