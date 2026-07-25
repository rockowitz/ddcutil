# File src/unit_tests/vcp/Makefile.am
# Unit tests for modules in src/vcp.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC


# Unit tests link the internal libvcp/libbase/libutil convenience
# libraries directly, since these symbols are not exported from libddcutil.
# Run by "make check".
LIBVCP  = $(top_builddir)/src/vcp/libvcp.la
LIBBASE = $(top_builddir)/src/base/libbase.la
LIBUTIL = $(top_builddir)/src/util/libutil.la

# libbase transitively pulls the udev/drm/dbus/acl util modules, and (via
# base modules' X11 fallbacks) X11/Xrandr.
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif

check_PROGRAMS = ../../unit_test_executables/vcp/test_vcp_feature_values ../../unit_test_executables/vcp/test_parsed_capabilities_feature \
  ../../unit_test_executables/vcp/test_parse_capabilities ../../unit_test_executables/vcp/test_persistent_capabilities ../../unit_test_executables/vcp/test_vcp_feature_codes
TESTS = ../../unit_test_executables/vcp/test_vcp_feature_values ../../unit_test_executables/vcp/test_parsed_capabilities_feature \
  ../../unit_test_executables/vcp/test_parse_capabilities ../../unit_test_executables/vcp/test_persistent_capabilities ../../unit_test_executables/vcp/test_vcp_feature_codes

______unit_test_executables_vcp_test_vcp_feature_values_SOURCES  = test_vcp_feature_values.c
______unit_test_executables_vcp_test_vcp_feature_values_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_vcp_test_vcp_feature_values_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_vcp_test_parsed_capabilities_feature_SOURCES  = test_parsed_capabilities_feature.c
______unit_test_executables_vcp_test_parsed_capabilities_feature_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_vcp_test_parsed_capabilities_feature_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_vcp_test_parse_capabilities_SOURCES  = test_parse_capabilities.c
______unit_test_executables_vcp_test_parse_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_vcp_test_parse_capabilities_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_vcp_test_persistent_capabilities_SOURCES  = test_persistent_capabilities.c
______unit_test_executables_vcp_test_persistent_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_vcp_test_persistent_capabilities_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_vcp_test_vcp_feature_codes_SOURCES  = test_vcp_feature_codes.c
______unit_test_executables_vcp_test_vcp_feature_codes_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_vcp_test_vcp_feature_codes_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
