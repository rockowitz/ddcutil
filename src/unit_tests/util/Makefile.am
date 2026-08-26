# File src/unit_tests/util/Makefile.am
# Unit tests for modules in src/util.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC


# Unit tests link the internal util convenience library directly, since these
# symbols are not exported from libddcutil. Run by "make check".
LIBUTIL = $(top_builddir)/src/util/libutil.la

check_PROGRAMS = ../../unit_test_executables/util/test_string_util ../../unit_test_executables/util/test_glib_util ../../unit_test_executables/util/test_debug_util ../../unit_test_executables/util/test_timestamp \
  ../../unit_test_executables/util/test_data_structures ../../unit_test_executables/util/test_file_util_base ../../unit_test_executables/util/test_traced_function_stack ../../unit_test_executables/util/test_msg_util \
  ../../unit_test_executables/util/test_report_util ../../unit_test_executables/util/test_ddcutil_config_file ../../unit_test_executables/util/test_dbus_util ../../unit_test_executables/util/test_acl_util \
  ../../unit_test_executables/util/test_drm_card_connector_util ../../unit_test_executables/util/test_device_id_util ../../unit_test_executables/util/test_error_info ../../unit_test_executables/util/test_edid \
  ../../unit_test_executables/util/test_glib_string_util ../../unit_test_executables/util/test_file_util ../../unit_test_executables/util/test_failsim \
  ../../unit_test_executables/util/test_i2c_util ../../unit_test_executables/util/test_libdrm_util ../../unit_test_executables/util/test_libdrm_aux_util ../../unit_test_executables/util/test_linux_basic_util \
  ../../unit_test_executables/util/test_linux_util ../../unit_test_executables/util/test_regex_util ../../unit_test_executables/util/test_multi_level_map ../../unit_test_executables/util/test_pnp_ids \
  ../../unit_test_executables/util/test_simple_ini_file ../../unit_test_executables/util/test_subprocess_util ../../unit_test_executables/util/test_sysfs_filter_functions \
  ../../unit_test_executables/util/test_sysfs_util ../../unit_test_executables/util/test_sysfs_i2c_util ../../unit_test_executables/util/test_systemd_util ../../unit_test_executables/util/test_utilrpt ../../unit_test_executables/util/test_xdg_util \
  ../../unit_test_executables/util/test_backtrace ../../unit_test_executables/util/test_suspend_resume_util
TESTS = ../../unit_test_executables/util/test_string_util ../../unit_test_executables/util/test_glib_util ../../unit_test_executables/util/test_debug_util ../../unit_test_executables/util/test_timestamp \
  ../../unit_test_executables/util/test_data_structures ../../unit_test_executables/util/test_file_util_base ../../unit_test_executables/util/test_traced_function_stack ../../unit_test_executables/util/test_msg_util \
  ../../unit_test_executables/util/test_report_util ../../unit_test_executables/util/test_ddcutil_config_file ../../unit_test_executables/util/test_dbus_util ../../unit_test_executables/util/test_acl_util \
  ../../unit_test_executables/util/test_drm_card_connector_util ../../unit_test_executables/util/test_device_id_util ../../unit_test_executables/util/test_error_info ../../unit_test_executables/util/test_edid \
  ../../unit_test_executables/util/test_glib_string_util ../../unit_test_executables/util/test_file_util ../../unit_test_executables/util/test_failsim \
  ../../unit_test_executables/util/test_i2c_util ../../unit_test_executables/util/test_libdrm_util ../../unit_test_executables/util/test_libdrm_aux_util ../../unit_test_executables/util/test_linux_basic_util \
  ../../unit_test_executables/util/test_linux_util ../../unit_test_executables/util/test_regex_util ../../unit_test_executables/util/test_multi_level_map ../../unit_test_executables/util/test_pnp_ids \
  ../../unit_test_executables/util/test_simple_ini_file ../../unit_test_executables/util/test_subprocess_util ../../unit_test_executables/util/test_sysfs_filter_functions \
  ../../unit_test_executables/util/test_sysfs_util ../../unit_test_executables/util/test_sysfs_i2c_util ../../unit_test_executables/util/test_systemd_util ../../unit_test_executables/util/test_utilrpt ../../unit_test_executables/util/test_xdg_util \
  ../../unit_test_executables/util/test_backtrace ../../unit_test_executables/util/test_suspend_resume_util

# The udev-based tests build only when the udev subsystem is enabled, since the
# udev_* modules are then compiled into libutil.
if ENABLE_UDEV_COND
check_PROGRAMS += ../../unit_test_executables/util/test_udev_i2c_util ../../unit_test_executables/util/test_udev_util ../../unit_test_executables/util/test_udev_usb_util
TESTS += ../../unit_test_executables/util/test_udev_i2c_util ../../unit_test_executables/util/test_udev_util ../../unit_test_executables/util/test_udev_usb_util
endif

# The x11 test builds only when x11 support is enabled, since x11_util.c is then
# compiled into libutil.
if USE_X11_COND
check_PROGRAMS += ../../unit_test_executables/util/test_x11_util
TESTS += ../../unit_test_executables/util/test_x11_util
endif

______unit_test_executables_util_test_string_util_SOURCES       = test_string_util.c
______unit_test_executables_util_test_string_util_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_string_util_LDADD         = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_glib_util_SOURCES         = test_glib_util.c
______unit_test_executables_util_test_glib_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_glib_util_LDADD           = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_debug_util_SOURCES        = test_debug_util.c
______unit_test_executables_util_test_debug_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_debug_util_LDADD          = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_timestamp_SOURCES         = test_timestamp.c
______unit_test_executables_util_test_timestamp_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_timestamp_LDADD           = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_data_structures_SOURCES   = test_data_structures.c
______unit_test_executables_util_test_data_structures_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_data_structures_LDADD     = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_file_util_base_SOURCES    = test_file_util_base.c
______unit_test_executables_util_test_file_util_base_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_file_util_base_LDADD      = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_traced_function_stack_SOURCES  = test_traced_function_stack.c
______unit_test_executables_util_test_traced_function_stack_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_traced_function_stack_LDADD    = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_msg_util_SOURCES          = test_msg_util.c
______unit_test_executables_util_test_msg_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_msg_util_LDADD            = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_report_util_SOURCES       = test_report_util.c
______unit_test_executables_util_test_report_util_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_report_util_LDADD         = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_ddcutil_config_file_SOURCES  = test_ddcutil_config_file.c
______unit_test_executables_util_test_ddcutil_config_file_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_ddcutil_config_file_LDADD    = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_dbus_util_SOURCES         = test_dbus_util.c
______unit_test_executables_util_test_dbus_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(DBUS_CFLAGS)
______unit_test_executables_util_test_dbus_util_LDADD           = $(LIBUTIL) $(GLIB_LIBS) $(DBUS_LIBS)
______unit_test_executables_util_test_acl_util_SOURCES          = test_acl_util.c
______unit_test_executables_util_test_acl_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(ACL_CFLAGS)
______unit_test_executables_util_test_acl_util_LDADD            = $(LIBUTIL) $(GLIB_LIBS) $(ACL_LIBS)
______unit_test_executables_util_test_drm_card_connector_util_SOURCES  = test_drm_card_connector_util.c
______unit_test_executables_util_test_drm_card_connector_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
______unit_test_executables_util_test_drm_card_connector_util_LDADD    = $(LIBUTIL) $(GLIB_LIBS) $(LIBDRM_LIBS)
______unit_test_executables_util_test_device_id_util_SOURCES    = test_device_id_util.c
______unit_test_executables_util_test_device_id_util_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_device_id_util_LDADD      = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_error_info_SOURCES        = test_error_info.c
______unit_test_executables_util_test_error_info_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_error_info_LDADD          = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_edid_SOURCES              = test_edid.c
______unit_test_executables_util_test_edid_CPPFLAGS             = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_edid_LDADD                = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_glib_string_util_SOURCES  = test_glib_string_util.c
______unit_test_executables_util_test_glib_string_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_glib_string_util_LDADD    = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_file_util_SOURCES         = test_file_util.c
______unit_test_executables_util_test_file_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_file_util_LDADD           = $(LIBUTIL) $(GLIB_LIBS)
# failsim.c is only compiled into libutil under --enable-failsim, so the test
# compiles it directly to remain runnable in the default configuration.
______unit_test_executables_util_test_failsim_SOURCES           = test_failsim.c $(top_srcdir)/src/util/failsim.c
______unit_test_executables_util_test_failsim_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_failsim_LDADD             = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_i2c_util_SOURCES          = test_i2c_util.c
______unit_test_executables_util_test_i2c_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_i2c_util_LDADD            = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_libdrm_util_SOURCES       = test_libdrm_util.c
______unit_test_executables_util_test_libdrm_util_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
______unit_test_executables_util_test_libdrm_util_LDADD         = $(LIBUTIL) $(GLIB_LIBS) $(LIBDRM_LIBS)
______unit_test_executables_util_test_libdrm_aux_util_SOURCES   = test_libdrm_aux_util.c
______unit_test_executables_util_test_libdrm_aux_util_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
______unit_test_executables_util_test_libdrm_aux_util_LDADD     = $(LIBUTIL) $(GLIB_LIBS) $(LIBDRM_LIBS)
______unit_test_executables_util_test_linux_basic_util_SOURCES  = test_linux_basic_util.c
______unit_test_executables_util_test_linux_basic_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_linux_basic_util_LDADD    = $(LIBUTIL) $(GLIB_LIBS)
# linux_util.o transitively pulls the acl/dbus/drm util modules, so link their libs.
______unit_test_executables_util_test_linux_util_SOURCES        = test_linux_util.c
______unit_test_executables_util_test_linux_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_linux_util_LDADD          = $(LIBUTIL) $(GLIB_LIBS) $(DBUS_LIBS) \
  $(LIBDRM_LIBS) $(ACL_LIBS)
# suspend_resume_util.o pulls dbus_util.o, which in turn pulls linux_util.o and
# the acl/drm util modules it uses, so link the same libs as test_linux_util.
______unit_test_executables_util_test_suspend_resume_util_SOURCES  = test_suspend_resume_util.c
______unit_test_executables_util_test_suspend_resume_util_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_suspend_resume_util_LDADD    = $(LIBUTIL) $(GLIB_LIBS) $(DBUS_LIBS) \
  $(LIBDRM_LIBS) $(ACL_LIBS)
______unit_test_executables_util_test_regex_util_SOURCES        = test_regex_util.c
______unit_test_executables_util_test_regex_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_regex_util_LDADD          = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_multi_level_map_SOURCES   = test_multi_level_map.c
______unit_test_executables_util_test_multi_level_map_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_multi_level_map_LDADD     = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_pnp_ids_SOURCES           = test_pnp_ids.c
______unit_test_executables_util_test_pnp_ids_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_pnp_ids_LDADD             = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_simple_ini_file_SOURCES   = test_simple_ini_file.c
______unit_test_executables_util_test_simple_ini_file_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_simple_ini_file_LDADD     = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_subprocess_util_SOURCES   = test_subprocess_util.c
______unit_test_executables_util_test_subprocess_util_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_subprocess_util_LDADD     = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_sysfs_filter_functions_SOURCES  = test_sysfs_filter_functions.c
______unit_test_executables_util_test_sysfs_filter_functions_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_sysfs_filter_functions_LDADD    = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_sysfs_util_SOURCES        = test_sysfs_util.c
______unit_test_executables_util_test_sysfs_util_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_sysfs_util_LDADD          = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_sysfs_i2c_util_SOURCES    = test_sysfs_i2c_util.c
______unit_test_executables_util_test_sysfs_i2c_util_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_sysfs_i2c_util_LDADD      = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_systemd_util_SOURCES      = test_systemd_util.c
______unit_test_executables_util_test_systemd_util_CPPFLAGS     = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_systemd_util_LDADD        = $(LIBUTIL) $(GLIB_LIBS) $(SYSTEMD_LIBS)
______unit_test_executables_util_test_udev_i2c_util_SOURCES     = test_udev_i2c_util.c
______unit_test_executables_util_test_udev_i2c_util_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(UDEV_CFLAGS)
______unit_test_executables_util_test_udev_i2c_util_LDADD       = $(LIBUTIL) $(GLIB_LIBS) $(UDEV_LIBS)
______unit_test_executables_util_test_udev_util_SOURCES         = test_udev_util.c
______unit_test_executables_util_test_udev_util_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(UDEV_CFLAGS)
______unit_test_executables_util_test_udev_util_LDADD           = $(LIBUTIL) $(GLIB_LIBS) $(UDEV_LIBS)
______unit_test_executables_util_test_udev_usb_util_SOURCES     = test_udev_usb_util.c
______unit_test_executables_util_test_udev_usb_util_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(UDEV_CFLAGS)
______unit_test_executables_util_test_udev_usb_util_LDADD       = $(LIBUTIL) $(GLIB_LIBS) $(UDEV_LIBS)
______unit_test_executables_util_test_utilrpt_SOURCES           = test_utilrpt.c
______unit_test_executables_util_test_utilrpt_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_utilrpt_LDADD             = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_xdg_util_SOURCES          = test_xdg_util.c
______unit_test_executables_util_test_xdg_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_xdg_util_LDADD            = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_backtrace_SOURCES         = test_backtrace.c
______unit_test_executables_util_test_backtrace_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_util_test_backtrace_LDADD           = $(LIBUTIL) $(GLIB_LIBS)
______unit_test_executables_util_test_x11_util_SOURCES          = test_x11_util.c
______unit_test_executables_util_test_x11_util_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBX11_CFLAGS)
______unit_test_executables_util_test_x11_util_LDADD            = $(LIBUTIL) $(GLIB_LIBS) $(LIBX11_LIBS) \
  $(XRANDR_LIBS) $(XEXT_LIBS)
