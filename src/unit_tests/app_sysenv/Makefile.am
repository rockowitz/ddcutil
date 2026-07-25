# File src/unit_tests/app_sysenv/Makefile.am
# Unit tests for modules in src/app_sysenv.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC

# Like src/unit_tests/app_ddcutil, app_sysenv/*.c files cross-reference the
# rest of the ddcutil core extensively, so there is no small per-directory
# library to link against. Mirror ddcutil_LDADD (see top-level
# src/Makefile.am): libapp.la aggregates libappddcutil.la and (since
# ENABLE_ENVCMDS_COND is set, matching src/app_sysenv's own gating)
# app_sysenv/libappsysenv.la.
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

check_PROGRAMS = test_query_sysenv_base test_query_sysenv_sysfs_common \
  test_query_sysenv_xref test_query_sysenv_final_analysis
TESTS = test_query_sysenv_base test_query_sysenv_sysfs_common \
  test_query_sysenv_xref test_query_sysenv_final_analysis

test_query_sysenv_base_SOURCES  = test_query_sysenv_base.c
test_query_sysenv_base_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_query_sysenv_base_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_query_sysenv_sysfs_common_SOURCES  = test_query_sysenv_sysfs_common.c
test_query_sysenv_sysfs_common_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_query_sysenv_sysfs_common_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_query_sysenv_xref_SOURCES  = test_query_sysenv_xref.c
test_query_sysenv_xref_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_query_sysenv_xref_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_query_sysenv_final_analysis_SOURCES  = test_query_sysenv_final_analysis.c
test_query_sysenv_final_analysis_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_query_sysenv_final_analysis_LDADD    = $(LIBAPP) $(LIBCOMMON) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
