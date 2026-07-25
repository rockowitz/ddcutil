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

check_PROGRAMS = ../../unit_test_executables/base/test_core ../../unit_test_executables/base/test_core_per_thread_settings ../../unit_test_executables/base/test_ddc_errno \
  ../../unit_test_executables/base/test_linux_errno ../../unit_test_executables/base/test_trace_control ../../unit_test_executables/base/test_ddc_packets ../../unit_test_executables/base/test_display_lock \
  ../../unit_test_executables/base/test_displays ../../unit_test_executables/base/test_display_retry_data ../../unit_test_executables/base/test_drm_connector_state \
  ../../unit_test_executables/base/test_dw_base ../../unit_test_executables/base/test_execution_stats ../../unit_test_executables/base/test_dsa2 ../../unit_test_executables/base/test_dynamic_features \
  ../../unit_test_executables/base/test_vcp_version ../../unit_test_executables/base/test_feature_lists ../../unit_test_executables/base/test_monitor_model_key \
  ../../unit_test_executables/base/test_feature_set_ref ../../unit_test_executables/base/test_status_code_mgt \
  ../../unit_test_executables/base/test_rtti ../../unit_test_executables/base/test_ddc_command_codes ../../unit_test_executables/base/test_stats ../../unit_test_executables/base/test_build_info ../../unit_test_executables/base/test_tuned_sleep \
  ../../unit_test_executables/base/test_feature_metadata ../../unit_test_executables/base/test_i2c_bus_aux ../../unit_test_executables/base/test_flock ../../unit_test_executables/base/test_i2c_bus_base \
  ../../unit_test_executables/base/test_per_thread_data \
  ../../unit_test_executables/base/test_build_timestamp ../../unit_test_executables/base/test_monitor_quirks ../../unit_test_executables/base/test_sleep ../../unit_test_executables/base/test_per_display_data \
  ../../unit_test_executables/base/test_base_services
TESTS = ../../unit_test_executables/base/test_core ../../unit_test_executables/base/test_core_per_thread_settings ../../unit_test_executables/base/test_ddc_errno \
  ../../unit_test_executables/base/test_linux_errno ../../unit_test_executables/base/test_trace_control ../../unit_test_executables/base/test_ddc_packets ../../unit_test_executables/base/test_display_lock \
  ../../unit_test_executables/base/test_displays ../../unit_test_executables/base/test_display_retry_data ../../unit_test_executables/base/test_drm_connector_state \
  ../../unit_test_executables/base/test_dw_base ../../unit_test_executables/base/test_execution_stats ../../unit_test_executables/base/test_dsa2 ../../unit_test_executables/base/test_dynamic_features \
  ../../unit_test_executables/base/test_vcp_version ../../unit_test_executables/base/test_feature_lists ../../unit_test_executables/base/test_monitor_model_key \
  ../../unit_test_executables/base/test_feature_set_ref ../../unit_test_executables/base/test_status_code_mgt \
  ../../unit_test_executables/base/test_rtti ../../unit_test_executables/base/test_ddc_command_codes ../../unit_test_executables/base/test_stats ../../unit_test_executables/base/test_build_info ../../unit_test_executables/base/test_tuned_sleep \
  ../../unit_test_executables/base/test_feature_metadata ../../unit_test_executables/base/test_i2c_bus_aux ../../unit_test_executables/base/test_flock ../../unit_test_executables/base/test_i2c_bus_base \
  ../../unit_test_executables/base/test_per_thread_data \
  ../../unit_test_executables/base/test_build_timestamp ../../unit_test_executables/base/test_monitor_quirks ../../unit_test_executables/base/test_sleep ../../unit_test_executables/base/test_per_display_data \
  ../../unit_test_executables/base/test_base_services

______unit_test_executables_base_test_core_SOURCES  = test_core.c
______unit_test_executables_base_test_core_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_core_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

______unit_test_executables_base_test_core_per_thread_settings_SOURCES  = test_core_per_thread_settings.c
______unit_test_executables_base_test_core_per_thread_settings_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_core_per_thread_settings_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

______unit_test_executables_base_test_ddc_errno_SOURCES         = test_ddc_errno.c
______unit_test_executables_base_test_ddc_errno_CPPFLAGS        = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_ddc_errno_LDADD           = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

______unit_test_executables_base_test_linux_errno_SOURCES       = test_linux_errno.c
______unit_test_executables_base_test_linux_errno_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_linux_errno_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

______unit_test_executables_base_test_trace_control_SOURCES     = test_trace_control.c
______unit_test_executables_base_test_trace_control_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_trace_control_LDADD       = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

______unit_test_executables_base_test_ddc_packets_SOURCES       = test_ddc_packets.c
______unit_test_executables_base_test_ddc_packets_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_ddc_packets_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

______unit_test_executables_base_test_display_lock_SOURCES      = test_display_lock.c
______unit_test_executables_base_test_display_lock_CPPFLAGS     = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_display_lock_LDADD        = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_displays_SOURCES          = test_displays.c
______unit_test_executables_base_test_displays_CPPFLAGS         = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_displays_LDADD            = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_display_retry_data_SOURCES  = test_display_retry_data.c
______unit_test_executables_base_test_display_retry_data_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_display_retry_data_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_drm_connector_state_SOURCES  = test_drm_connector_state.c
______unit_test_executables_base_test_drm_connector_state_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS) $(LIBDRM_CFLAGS)
______unit_test_executables_base_test_drm_connector_state_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_dw_base_SOURCES           = test_dw_base.c
______unit_test_executables_base_test_dw_base_CPPFLAGS          = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_dw_base_LDADD             = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_execution_stats_SOURCES   = test_execution_stats.c
______unit_test_executables_base_test_execution_stats_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_execution_stats_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

# dsa2.c uses libm (math.h).
______unit_test_executables_base_test_dsa2_SOURCES              = test_dsa2.c
______unit_test_executables_base_test_dsa2_CPPFLAGS             = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_dsa2_LDADD                = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_base_test_dynamic_features_SOURCES  = test_dynamic_features.c
______unit_test_executables_base_test_dynamic_features_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_dynamic_features_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_vcp_version_SOURCES       = test_vcp_version.c
______unit_test_executables_base_test_vcp_version_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_vcp_version_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_feature_lists_SOURCES     = test_feature_lists.c
______unit_test_executables_base_test_feature_lists_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_feature_lists_LDADD       = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_monitor_model_key_SOURCES  = test_monitor_model_key.c
______unit_test_executables_base_test_monitor_model_key_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_monitor_model_key_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_feature_set_ref_SOURCES   = test_feature_set_ref.c
______unit_test_executables_base_test_feature_set_ref_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_feature_set_ref_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_status_code_mgt_SOURCES   = test_status_code_mgt.c
______unit_test_executables_base_test_status_code_mgt_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_status_code_mgt_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_rtti_SOURCES              = test_rtti.c
______unit_test_executables_base_test_rtti_CPPFLAGS             = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_rtti_LDADD                = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_ddc_command_codes_SOURCES  = test_ddc_command_codes.c
______unit_test_executables_base_test_ddc_command_codes_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_ddc_command_codes_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_stats_SOURCES             = test_stats.c
______unit_test_executables_base_test_stats_CPPFLAGS            = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_stats_LDADD               = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_build_info_SOURCES        = test_build_info.c
______unit_test_executables_base_test_build_info_CPPFLAGS       = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_build_info_LDADD          = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_tuned_sleep_SOURCES       = test_tuned_sleep.c
______unit_test_executables_base_test_tuned_sleep_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_tuned_sleep_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_feature_metadata_SOURCES  = test_feature_metadata.c
______unit_test_executables_base_test_feature_metadata_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_feature_metadata_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_i2c_bus_aux_SOURCES       = test_i2c_bus_aux.c
______unit_test_executables_base_test_i2c_bus_aux_CPPFLAGS      = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_i2c_bus_aux_LDADD         = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_flock_SOURCES             = test_flock.c
______unit_test_executables_base_test_flock_CPPFLAGS            = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_flock_LDADD               = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_i2c_bus_base_SOURCES      = test_i2c_bus_base.c
______unit_test_executables_base_test_i2c_bus_base_CPPFLAGS     = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_i2c_bus_base_LDADD        = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_per_thread_data_SOURCES   = test_per_thread_data.c
______unit_test_executables_base_test_per_thread_data_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_per_thread_data_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_build_timestamp_SOURCES   = test_build_timestamp.c
______unit_test_executables_base_test_build_timestamp_CPPFLAGS  = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_build_timestamp_LDADD     = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_monitor_quirks_SOURCES    = test_monitor_quirks.c
______unit_test_executables_base_test_monitor_quirks_CPPFLAGS   = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_monitor_quirks_LDADD      = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_base_test_sleep_SOURCES             = test_sleep.c
______unit_test_executables_base_test_sleep_CPPFLAGS            = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_sleep_LDADD               = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

# per_display_data and base_services pull dsa2, which uses libm.
______unit_test_executables_base_test_per_display_data_SOURCES  = test_per_display_data.c
______unit_test_executables_base_test_per_display_data_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_per_display_data_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_base_test_base_services_SOURCES     = test_base_services.c
______unit_test_executables_base_test_base_services_CPPFLAGS    = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_base_test_base_services_LDADD       = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
