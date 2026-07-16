# File src/sample_clients/Makefile.am
# Makefile for sample and test programs for libddcutil

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS=     \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC
# AM_CFLAGS += -Werror

check_PROGRAMS =
if ENABLE_SHARED_LIB_COND
# Sample C client program for shared library:
check_PROGRAMS += \
  laclient \
  demo_capabilities \
  demo_display_selection \
  demo_feature_list \
  demo_get_set_vcp \
  demo_global_settings \
  demo_profile_features \
  demo_redirection \
  demo_vcpinfo \
  stress_watch
endif

# Unit test for src/util/string_util.c.  Links the internal util convenience
# library directly, since these symbols are not exported from libddcutil, so it
# does not depend on ENABLE_SHARED_LIB_COND.  Run by "make check" (the demo
# clients are not, as they require an attached display).
check_PROGRAMS += test_string_util test_glib_util test_debug_util test_timestamp \
  test_data_structures test_file_util_base test_traced_function_stack test_msg_util \
  test_report_util test_ddcutil_config_file test_dbus_util test_acl_util \
  test_drm_card_connector_util test_device_id_util
TESTS = test_string_util test_glib_util test_debug_util test_timestamp \
  test_data_structures test_file_util_base test_traced_function_stack test_msg_util \
  test_report_util test_ddcutil_config_file test_dbus_util test_acl_util \
  test_drm_card_connector_util test_device_id_util

laclient_SOURCES               = clmain.c
demo_capabilities_SOURCES      = demo_capabilities.c
demo_display_selection_SOURCES = demo_display_selection.c
demo_feature_list_SOURCES      = demo_feature_list.c
demo_get_set_vcp_SOURCES       = demo_get_set_vcp.c
demo_global_settings_SOURCES   = demo_global_settings.c
demo_profile_features_SOURCES  = demo_profile_features.c
demo_redirection_SOURCES       = demo_redirection.c
demo_vcpinfo_SOURCES           = demo_vcpinfo.c
stress_watch_SOURCES           = stress_watch.c
stress_watch_LDADD             = ../libddcutil.la -lpthread
test_string_util_SOURCES       = test_string_util.c
test_string_util_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_string_util_LDADD         = ../util/libutil.la $(GLIB_LIBS)
test_glib_util_SOURCES         = test_glib_util.c
test_glib_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_glib_util_LDADD           = ../util/libutil.la $(GLIB_LIBS)
test_debug_util_SOURCES        = test_debug_util.c
test_debug_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_debug_util_LDADD          = ../util/libutil.la $(GLIB_LIBS)
test_timestamp_SOURCES         = test_timestamp.c
test_timestamp_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_timestamp_LDADD           = ../util/libutil.la $(GLIB_LIBS)
test_data_structures_SOURCES   = test_data_structures.c
test_data_structures_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_data_structures_LDADD     = ../util/libutil.la $(GLIB_LIBS)
test_file_util_base_SOURCES    = test_file_util_base.c
test_file_util_base_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_file_util_base_LDADD      = ../util/libutil.la $(GLIB_LIBS)
test_traced_function_stack_SOURCES  = test_traced_function_stack.c
test_traced_function_stack_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_traced_function_stack_LDADD    = ../util/libutil.la $(GLIB_LIBS)
test_msg_util_SOURCES          = test_msg_util.c
test_msg_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_msg_util_LDADD            = ../util/libutil.la $(GLIB_LIBS)
test_report_util_SOURCES       = test_report_util.c
test_report_util_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_report_util_LDADD         = ../util/libutil.la $(GLIB_LIBS)
test_ddcutil_config_file_SOURCES  = test_ddcutil_config_file.c
test_ddcutil_config_file_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddcutil_config_file_LDADD    = ../util/libutil.la $(GLIB_LIBS)
test_dbus_util_SOURCES         = test_dbus_util.c
test_dbus_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(DBUS_CFLAGS)
test_dbus_util_LDADD           = ../util/libutil.la $(GLIB_LIBS) $(DBUS_LIBS)
test_acl_util_SOURCES          = test_acl_util.c
test_acl_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(ACL_CFLAGS)
test_acl_util_LDADD            = ../util/libutil.la $(GLIB_LIBS) $(ACL_LIBS)
test_drm_card_connector_util_SOURCES  = test_drm_card_connector_util.c
test_drm_card_connector_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
test_drm_card_connector_util_LDADD    = ../util/libutil.la $(GLIB_LIBS) $(LIBDRM_LIBS)
test_device_id_util_SOURCES    = test_device_id_util.c
test_device_id_util_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_device_id_util_LDADD      = ../util/libutil.la $(GLIB_LIBS)

LDADD       = ../libddcutil.la
AM_LDFLAGS  = -pie


clean-local: 
	@echo "(src/sample_clients/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/sample_clients/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/sample_clients/Makefile) distclean-local"


# Make this a dependency of something to enable it:
show_vars:
	@echo "src/sample_clients/Makefile"
	@echo "srcdir:      ${srcdir}"
	@echo "top_srcdir:  ${top_srcdir}"

.PHONY: show_vars

