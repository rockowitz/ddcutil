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
  test_drm_card_connector_util test_device_id_util test_error_info test_edid \
  test_glib_string_util test_file_util test_failsim \
  test_i2c_util test_libdrm_util test_libdrm_aux_util test_linux_basic_util \
  test_linux_util test_regex_util test_multi_level_map test_pnp_ids \
  test_simple_ini_file test_subprocess_util test_sysfs_filter_functions \
  test_sysfs_util test_sysfs_i2c_util test_systemd_util
TESTS = test_string_util test_glib_util test_debug_util test_timestamp \
  test_data_structures test_file_util_base test_traced_function_stack test_msg_util \
  test_report_util test_ddcutil_config_file test_dbus_util test_acl_util \
  test_drm_card_connector_util test_device_id_util test_error_info test_edid \
  test_glib_string_util test_file_util test_failsim \
  test_i2c_util test_libdrm_util test_libdrm_aux_util test_linux_basic_util \
  test_linux_util test_regex_util test_multi_level_map test_pnp_ids \
  test_simple_ini_file test_subprocess_util test_sysfs_filter_functions \
  test_sysfs_util test_sysfs_i2c_util test_systemd_util

# The udev-based tests build only when the udev subsystem is enabled, since the
# udev_* modules are then compiled into libutil.
if ENABLE_UDEV_COND
check_PROGRAMS += test_udev_i2c_util test_udev_util test_udev_usb_util
TESTS += test_udev_i2c_util test_udev_util test_udev_usb_util
endif

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
test_error_info_SOURCES        = test_error_info.c
test_error_info_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_error_info_LDADD          = ../util/libutil.la $(GLIB_LIBS)
test_edid_SOURCES              = test_edid.c
test_edid_CPPFLAGS             = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_edid_LDADD                = ../util/libutil.la $(GLIB_LIBS)
test_glib_string_util_SOURCES  = test_glib_string_util.c
test_glib_string_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_glib_string_util_LDADD    = ../util/libutil.la $(GLIB_LIBS)
test_file_util_SOURCES         = test_file_util.c
test_file_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_file_util_LDADD           = ../util/libutil.la $(GLIB_LIBS)
# failsim.c is only compiled into libutil under --enable-failsim, so the test
# compiles it directly to remain runnable in the default configuration.
test_failsim_SOURCES           = test_failsim.c ../util/failsim.c
test_failsim_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_failsim_LDADD             = ../util/libutil.la $(GLIB_LIBS)
test_i2c_util_SOURCES          = test_i2c_util.c
test_i2c_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_i2c_util_LDADD            = ../util/libutil.la $(GLIB_LIBS)
test_libdrm_util_SOURCES       = test_libdrm_util.c
test_libdrm_util_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
test_libdrm_util_LDADD         = ../util/libutil.la $(GLIB_LIBS) $(LIBDRM_LIBS)
test_libdrm_aux_util_SOURCES   = test_libdrm_aux_util.c
test_libdrm_aux_util_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
test_libdrm_aux_util_LDADD     = ../util/libutil.la $(GLIB_LIBS) $(LIBDRM_LIBS)
test_linux_basic_util_SOURCES  = test_linux_basic_util.c
test_linux_basic_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_linux_basic_util_LDADD    = ../util/libutil.la $(GLIB_LIBS)
# linux_util.o transitively pulls the acl/dbus/drm util modules, so link their libs.
test_linux_util_SOURCES        = test_linux_util.c
test_linux_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_linux_util_LDADD          = ../util/libutil.la $(GLIB_LIBS) $(DBUS_LIBS) \
  $(LIBDRM_LIBS) $(ACL_LIBS)
test_regex_util_SOURCES        = test_regex_util.c
test_regex_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_regex_util_LDADD          = ../util/libutil.la $(GLIB_LIBS)
test_multi_level_map_SOURCES   = test_multi_level_map.c
test_multi_level_map_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_multi_level_map_LDADD     = ../util/libutil.la $(GLIB_LIBS)
test_pnp_ids_SOURCES           = test_pnp_ids.c
test_pnp_ids_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_pnp_ids_LDADD             = ../util/libutil.la $(GLIB_LIBS)
test_simple_ini_file_SOURCES   = test_simple_ini_file.c
test_simple_ini_file_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_simple_ini_file_LDADD     = ../util/libutil.la $(GLIB_LIBS)
test_subprocess_util_SOURCES   = test_subprocess_util.c
test_subprocess_util_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_subprocess_util_LDADD     = ../util/libutil.la $(GLIB_LIBS)
test_sysfs_filter_functions_SOURCES  = test_sysfs_filter_functions.c
test_sysfs_filter_functions_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_sysfs_filter_functions_LDADD    = ../util/libutil.la $(GLIB_LIBS)
test_sysfs_util_SOURCES        = test_sysfs_util.c
test_sysfs_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_sysfs_util_LDADD          = ../util/libutil.la $(GLIB_LIBS)
test_sysfs_i2c_util_SOURCES    = test_sysfs_i2c_util.c
test_sysfs_i2c_util_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_sysfs_i2c_util_LDADD      = ../util/libutil.la $(GLIB_LIBS)
test_systemd_util_SOURCES      = test_systemd_util.c
test_systemd_util_CPPFLAGS     = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_systemd_util_LDADD        = ../util/libutil.la $(GLIB_LIBS) $(SYSTEMD_LIBS)
test_udev_i2c_util_SOURCES     = test_udev_i2c_util.c
test_udev_i2c_util_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(UDEV_CFLAGS)
test_udev_i2c_util_LDADD       = ../util/libutil.la $(GLIB_LIBS) $(UDEV_LIBS)
test_udev_util_SOURCES         = test_udev_util.c
test_udev_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(UDEV_CFLAGS)
test_udev_util_LDADD           = ../util/libutil.la $(GLIB_LIBS) $(UDEV_LIBS)
test_udev_usb_util_SOURCES     = test_udev_usb_util.c
test_udev_usb_util_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(UDEV_CFLAGS)
test_udev_usb_util_LDADD       = ../util/libutil.la $(GLIB_LIBS) $(UDEV_LIBS)

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

