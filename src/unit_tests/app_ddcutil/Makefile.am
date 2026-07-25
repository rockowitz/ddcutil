# File src/unit_tests/app_ddcutil/Makefile.am
# Unit tests for modules in src/app_ddcutil.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Copy the built test executables to src/unit_test_executables/app_ddcutil
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/app_ddcutil

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

# Like src/unit_tests/ddc/dw/dynvcp/usb, app_ddcutil/*.c files
# cross-reference the rest of the ddcutil core (ddc, vcp, dynvcp, i2c,
# base, util) extensively, so there is no small per-directory library to
# link against. Link the top-level libcommon convenience library -- the
# same aggregate the ddcutil executable itself links -- plus the same
# external libraries ddcutil_LDADD/libcommon_la_LIBADD pull in (see
# top-level src/Makefile.am).
# Mirror ddcutil_LDADD (see top-level src/Makefile.am): libapp.la
# aggregates libappddcutil.la plus (conditionally) app_sysenv/libtestcases,
# and app_ddcutil's own init function pulls in app_interrogate.o, which
# references app_sysenv symbols when ENABLE_ENVCMDS_COND is set.
LIBAPP    = $(top_builddir)/src/libapp.la
LIBCOMMON = $(top_builddir)/src/libcommon.la

EXTRA_SYSLIBS = $(ACL_LIBS) $(JANSSON_LIBS) $(UDEV_LIBS) $(LIBUSB_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif
if USE_DBUS_COND
EXTRA_SYSLIBS += $(DBUS_LIBS)
endif
if USE_SYSTEMD_COND
EXTRA_SYSLIBS += $(SYSTEMD_LIBS)
endif
if USE_LIBDRM_COND
EXTRA_SYSLIBS += $(LIBDRM_LIBS)
endif

check_PROGRAMS = test_app_capabilities test_app_vcpinfo \
  test_app_experimental test_app_dynamic_features test_app_ddcutil_services
TESTS = test_app_capabilities test_app_vcpinfo \
  test_app_experimental test_app_dynamic_features test_app_ddcutil_services

# app_testcases.c is gated behind --enable-testcases (default: disabled)
# and does not currently compile in that configuration -- it still
# references parsed_cmd->pdid, a Parsed_Cmd field removed elsewhere in the
# codebase. Since --enable-testcases is off by default, this is
# pre-existing bit rot in a disabled developer-only code path, not
# something introduced here; there is no test file for app_testcases.c.

test_app_capabilities_SOURCES  = test_app_capabilities.c
test_app_capabilities_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_app_capabilities_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_app_vcpinfo_SOURCES  = test_app_vcpinfo.c
test_app_vcpinfo_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_app_vcpinfo_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_app_experimental_SOURCES  = test_app_experimental.c
test_app_experimental_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_app_experimental_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_app_dynamic_features_SOURCES  = test_app_dynamic_features.c
test_app_dynamic_features_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_app_dynamic_features_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_app_ddcutil_services_SOURCES  = test_app_ddcutil_services.c
test_app_ddcutil_services_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_app_ddcutil_services_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
