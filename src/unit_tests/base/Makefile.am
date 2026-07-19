# File src/unit_tests/base/Makefile.am
# Unit tests for modules in src/base.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Unit tests link the internal libbase/libutil convenience libraries directly,
# since these symbols are not exported from libddcutil. Run by "make check".
LIBBASE = $(top_builddir)/src/base/libbase.la
LIBUTIL = $(top_builddir)/src/util/libutil.la

# Several base modules transitively pull the udev/drm/dbus/acl util modules
# (via the display-path and connector reporting helpers), so those tests link
# their libs.
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS)

check_PROGRAMS = test_core test_core_per_thread_settings test_ddc_errno \
  test_linux_errno test_trace_control test_ddc_packets test_display_lock \
  test_displays test_display_retry_data test_drm_connector_state \
  test_dw_base test_execution_stats test_dsa2 test_dynamic_features \
  test_vcp_version test_feature_lists test_monitor_model_key \
  test_feature_set_ref test_status_code_mgt
TESTS = test_core test_core_per_thread_settings test_ddc_errno \
  test_linux_errno test_trace_control test_ddc_packets test_display_lock \
  test_displays test_display_retry_data test_drm_connector_state \
  test_dw_base test_execution_stats test_dsa2 test_dynamic_features \
  test_vcp_version test_feature_lists test_monitor_model_key \
  test_feature_set_ref test_status_code_mgt

test_core_SOURCES  = test_core.c
test_core_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_core_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_core_per_thread_settings_SOURCES  = test_core_per_thread_settings.c
test_core_per_thread_settings_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_core_per_thread_settings_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_ddc_errno_SOURCES         = test_ddc_errno.c
test_ddc_errno_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_errno_LDADD           = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_linux_errno_SOURCES       = test_linux_errno.c
test_linux_errno_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_linux_errno_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_trace_control_SOURCES     = test_trace_control.c
test_trace_control_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_trace_control_LDADD       = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_ddc_packets_SOURCES       = test_ddc_packets.c
test_ddc_packets_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_ddc_packets_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_display_lock_SOURCES      = test_display_lock.c
test_display_lock_CPPFLAGS     = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_display_lock_LDADD        = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_displays_SOURCES          = test_displays.c
test_displays_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_displays_LDADD            = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_display_retry_data_SOURCES  = test_display_retry_data.c
test_display_retry_data_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_display_retry_data_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_drm_connector_state_SOURCES  = test_drm_connector_state.c
test_drm_connector_state_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
test_drm_connector_state_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_dw_base_SOURCES           = test_dw_base.c
test_dw_base_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dw_base_LDADD             = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_execution_stats_SOURCES   = test_execution_stats.c
test_execution_stats_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_execution_stats_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

# dsa2.c uses libm (math.h).
test_dsa2_SOURCES              = test_dsa2.c
test_dsa2_CPPFLAGS             = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dsa2_LDADD                = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_dynamic_features_SOURCES  = test_dynamic_features.c
test_dynamic_features_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_dynamic_features_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_vcp_version_SOURCES       = test_vcp_version.c
test_vcp_version_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_vcp_version_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_feature_lists_SOURCES     = test_feature_lists.c
test_feature_lists_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_feature_lists_LDADD       = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_monitor_model_key_SOURCES  = test_monitor_model_key.c
test_monitor_model_key_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_monitor_model_key_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_feature_set_ref_SOURCES   = test_feature_set_ref.c
test_feature_set_ref_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_feature_set_ref_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

test_status_code_mgt_SOURCES   = test_status_code_mgt.c
test_status_code_mgt_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_status_code_mgt_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)
