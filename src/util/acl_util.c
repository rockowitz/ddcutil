/** @file acl_util.c
 *
 *  Access Control List utilities
 */

// Copyright (C) 2020-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <acl/libacl.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <syslog.h>
#endif
#include <unistd.h>
/** \endcond */

#include "common_inlines.h"
#include "debug_util.h"
#include "linux_basic_util.h"
#include "report_util.h"
#include "string_util.h"
#include "syslog_util.h"

#include "acl_util.h"



/** Collects information about the access control list (ACL) for a file,
 *  using the libacl API, and returns it as an array of lines.
 *
 *  The output is similar to, but not identical to, that of command **facl**.
 *  In particular, the effective permissions, after mask application, are
 *  not reported.
 *
 *  @param  fqfn       file name
 *  @param  collector  if NULL, allocate new GPtrArray
 *  @param  depth      logical indentation depth for formatting lines
 *  @return GPtrArray of lines
 */
GPtrArray * rpt_facl_collect0(
      const char * fqfn,
      GPtrArray *  collector,
      int          depth)
{
   if (!collector)
      collector = g_ptr_array_new_with_free_func(g_free);

   acl_t acl = acl_get_file(fqfn, ACL_TYPE_ACCESS);
   if (acl == NULL) {
      char * s = g_strdup_printf("acl_get_file(\"%s\") failed. errno=%d", fqfn, errno);
      g_ptr_array_add(collector, s);
      goto bye;
    }

    char *text;
    ssize_t len;
    text = acl_to_text(acl, &len);
    if (text == NULL) {
        char * s = g_strdup_printf("acl_to_text() failed. errno=%d", errno);
        g_ptr_array_add(collector, s);
        acl_free(acl);
        goto bye;
    }

    Null_Terminated_String_Array ntsa = strsplit(text, "\n\r");
    GPtrArray * lines = ntsa_to_g_ptr_array(ntsa);
    // DBG("lines->len = %d", lines->len);
    for (int ndx = 0; ndx < lines->len; ndx++) {
       char * s = g_strdup_printf("%*s%s", depth, " ", ntsa[ndx]);
       // DBG("s: %s", s);
       g_ptr_array_add(collector, s);
    }
    ntsa_free(ntsa,  true);
    g_ptr_array_free(lines, false);

    // free(text); // invalid free(), sample code  had acl_free_text()
    acl_free(acl);

bye:
   return collector;
}



#ifdef OUT
#define NAME_BUF_SZ 1024

void format_user_name(uid_t uid, char * buf, int bufsz) {
    struct passwd pwd, *result;
    char namebuf[NAME_BUF_SZ];
    if (getpwuid_r(uid, &pwd, buf, bufsz, &result) == 0 && result != NULL) {
       g_snprintf(buf, bufsz, "%s", pwd.pw_name);
    } else {
        g_snprintf("%u", (unsigned int)uid);  /* fall back to numeric UID */
    }
}

void format_group_name(gid_t gid, char * buf, int bufsz) {
    struct group grp, *result;

    if (getgrgid_r(gid, &grp, buf, NAME_BUF_SZ, &result) == 0 && result != NULL) {
        g_snprint(buf, bufsz, "%s", grp.gr_name);
    } else {
        g_snprintf(buf, bufsz, "%u", (unsigned int)gid);  /* fall back to numeric GID */
    }
}
#endif



static char * format_facl_tag(acl_entry_t entry, acl_tag_t tag) {
   char * result = NULL;

   /* Format the tag as a label */
   switch (tag) {
   case ACL_USER_OBJ:
      result = g_strdup_printf("   user:: ");
      break;
   case ACL_USER: {
      uid_t *uidp = (uid_t *)acl_get_qualifier(entry);
      result = (uidp != NULL)
                ? g_strdup_printf("   user: %s: ", uid_name(*uidp))
                : strdup("  user:<NULL>: ");
      if (uidp) acl_free(uidp);
      break;
   }
   case ACL_GROUP_OBJ:
      result = g_strdup_printf("   group:: ");
      break;
   case ACL_GROUP: {
       gid_t *gidp = (gid_t *)acl_get_qualifier(entry);
       result = (gidp != NULL)
                 ? g_strdup_printf("  group: %s: ", gid_name(*gidp))
                 : strdup("  group:<NULL>: ");
       if (gidp) acl_free(gidp);
       break;
   }
   case ACL_MASK:
      result = strdup("   mask:: ");
       break;
   case ACL_OTHER:
       result = strdup("   other:: ");
       break;
   default:
       result = g_strdup_printf("   <unknown_tag:%d>: ", (int)tag);
       break;
   }
   return result;
}


GPtrArray * rpt_facl_collect1(
      const char * fqfn,
      GPtrArray *  collector,
      int          depth)
{
   if (!collector)
      collector = g_ptr_array_new_with_free_func(g_free);

   acl_entry_t entry;
   acl_tag_t tag;

   acl_t acl = acl_get_file(fqfn, ACL_TYPE_ACCESS);
   if (acl == NULL) {
      G_PTR_ARRAY_ADD_STRING(collector, "acl_get_file(\"%s\") failed. errno=%d", fqfn, errno);
      goto bye;
    }

    G_PTR_ARRAY_ADD_STRING(collector, "ACL entries for %s:", fqfn);

    int entry_id = ACL_FIRST_ENTRY;
    while (true) {
        if (acl_get_entry(acl, entry_id, &entry) != 1) {
            break;   /* no more entries */
        }

        if (acl_get_tag_type(entry, &tag) == -1) {
            G_PTR_ARRAY_ADD_STRING(collector, "acl_get_tag_type() failed");
            break;
        }

        char * formatted_tag = format_facl_tag(entry, tag);

        /* Print permissions */
        char * perms = NULL;
        acl_permset_t permset;
        if (acl_get_permset(entry, &permset) != -1) {
            perms = g_strdup_printf("%c%c%c",
                   acl_get_perm(permset, ACL_READ) ? 'r' : '-',
                   acl_get_perm(permset, ACL_WRITE) ? 'w' : '-',
                   acl_get_perm(permset, ACL_EXECUTE) ? 'x' : '-');
        }
        else
           perms = strdup("");

        G_PTR_ARRAY_ADD_STRING(collector, "%s%s", formatted_tag, perms);
        free(formatted_tag);
        free(perms);

        entry_id = ACL_NEXT_ENTRY;
    }

    acl_free(acl);  /* still free the ACL object */

bye:
   return collector;
}

char * get_user_acl(const char * fqfn, uid_t uid) {
   bool debug  = false;

   acl_entry_t entry;
   acl_tag_t tag;

   char * perms = NULL;
   acl_t acl = acl_get_file(fqfn, ACL_TYPE_ACCESS);
   if (acl == NULL) {
       SIMPLE_STD_SYSLOG(LOG_WARNING, "acl_get_file(\"%s\") failed. errno=%d", fqfn, errno);
       goto bye;
   }

   int entry_id = ACL_FIRST_ENTRY;
   while (true) {
       if (acl_get_entry(acl, entry_id, &entry) != 1) {
           break;   /* no more entries */
       }

       if (acl_get_tag_type(entry, &tag) == -1) {
          SIMPLE_STD_SYSLOG(LOG_ERR, "acl_get_tag_type() failed");
           break;
       }

      if (tag == ACL_USER) {
         uid_t *uidp = (uid_t *)acl_get_qualifier(entry);
         if (uidp) {
            if (*uidp == uid) {
               acl_permset_t permset;
               if (acl_get_permset(entry, &permset) != -1) {
                  perms = g_strdup_printf("%c%c%c",
                               acl_get_perm(permset, ACL_READ) ? 'r' : '-',
                               acl_get_perm(permset, ACL_WRITE) ? 'w' : '-',
                               acl_get_perm(permset, ACL_EXECUTE) ? 'x' : '-');
               }
            }
            acl_free(uidp);
            break;
         }
      }
      entry_id = ACL_NEXT_ENTRY;
   }

   acl_free(acl);

bye:
   DBGF(debug, "fqfn=%s, uid=%d, persm=%s", fqfn, uid, perms);
   return perms;
}


bool is_acl_rw(const char * fqfn, acl_tag_t tag, uid_t id) {
   bool debug  = false;

   acl_entry_t entry;
   acl_tag_t   entry_tag;

   bool result = false;

   acl_t acl = acl_get_file(fqfn, ACL_TYPE_ACCESS);
   if (acl == NULL) {
       SIMPLE_STD_SYSLOG(LOG_WARNING, "acl_get_file(\"%s\") failed. errno=%d", fqfn, errno);
       goto bye;
   }

   int entry_id = ACL_FIRST_ENTRY;
   while (true) {
       if (acl_get_entry(acl, entry_id, &entry) != 1) {
           break;   /* no more entries */
       }

       if (acl_get_tag_type(entry, &entry_tag) == -1) {
          SIMPLE_STD_SYSLOG(LOG_ERR, "acl_get_tag_type() failed");
          break;
       }

      if (tag == ACL_USER && entry_tag == ACL_USER) {
         uid_t *uidp = (uid_t *)acl_get_qualifier(entry);
         if (uidp) {
            if (*uidp == id) {
               acl_permset_t permset;
               if (acl_get_permset(entry, &permset) != -1)
                  result = acl_get_perm(permset, ACL_READ) && acl_get_perm(permset, ACL_WRITE);
            }
            acl_free(uidp);
            break;
         }
      }
      else if (entry_tag == ACL_GROUP_OBJ && tag == ACL_GROUP_OBJ) {
         acl_permset_t permset;
         if (acl_get_permset(entry, &permset) != -1) {
            result = acl_get_perm(permset, ACL_READ) && acl_get_perm(permset, ACL_WRITE);
         }
         break;
      }
      else if (entry_tag == ACL_GROUP && tag == ACL_GROUP) {
         gid_t *gidp = (gid_t *)acl_get_qualifier(entry);
         if (gidp) {
            if (*gidp == id) {
               acl_permset_t permset;
               if (acl_get_permset(entry, &permset) != -1)
                  result = acl_get_perm(permset, ACL_READ) && acl_get_perm(permset, ACL_WRITE);
            }
            acl_free(gidp);
            break;
         }
      }

      entry_id = ACL_NEXT_ENTRY;
   }

   acl_free(acl);

bye:
   DBGF(debug, "fqfn=%s, tag=%d, id=%d, result=%s", fqfn, tag, id, sbool(result));
   return result;
}

bool is_cur_user_acl_rw(const char * fqfn) {
   uid_t uid = getuid();
   bool result = is_acl_rw(fqfn, ACL_USER, uid);
   return result;
}

bool is_file_group_acl_rw(const char * fqfn) {
   bool result = false;
   result = is_acl_rw(fqfn, ACL_GROUP_OBJ, 0);
   return result;
}



#ifdef UNUSED
GPtrArray * rpt_facl_collect(const char * fqfn) {
   return rpt_facl_collect0(fqfn, NULL, 0);
}
#endif

#ifdef UNUSED
void report_facl(const char * fqfn, int depth) {
   GPtrArray * lines = rpt_facl_collect0(fqfn, NULL, depth);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * s = g_ptr_array_index(lines, ndx);
      rpt_vstring(depth, "%s", s);
   }
   g_ptr_array_free(lines, true);
}
#endif

#ifdef UNUSED
void report_facl_to_syslog(const char * fqfn, int log_level, int depth) {
   GPtrArray * lines = rpt_facl_collect0(fqfn, NULL, depth);
   for (int ndx = 0; ndx < lines->len; ndx++) {
      char * s = g_ptr_array_index(lines, ndx);
      syslog(log_level, "%s", s);
   }
   g_ptr_array_free(lines, true);
}
#endif
