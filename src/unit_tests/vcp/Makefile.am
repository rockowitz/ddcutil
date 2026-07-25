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

# Copy the built test executables to src/unit_test_executables/vcp
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/vcp

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

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

check_PROGRAMS = test_vcp_feature_values test_parsed_capabilities_feature \
  test_parse_capabilities test_persistent_capabilities test_vcp_feature_codes
TESTS = test_vcp_feature_values test_parsed_capabilities_feature \
  test_parse_capabilities test_persistent_capabilities test_vcp_feature_codes

test_vcp_feature_values_SOURCES  = test_vcp_feature_values.c
test_vcp_feature_values_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_vcp_feature_values_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_parsed_capabilities_feature_SOURCES  = test_parsed_capabilities_feature.c
test_parsed_capabilities_feature_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_parsed_capabilities_feature_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_parse_capabilities_SOURCES  = test_parse_capabilities.c
test_parse_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_parse_capabilities_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_persistent_capabilities_SOURCES  = test_persistent_capabilities.c
test_persistent_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_persistent_capabilities_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_vcp_feature_codes_SOURCES  = test_vcp_feature_codes.c
test_vcp_feature_codes_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_vcp_feature_codes_LDADD    = $(LIBVCP) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
