# File src/unit_tests/cmdline/Makefile.am
# Unit tests for modules in src/cmdline.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC


# Unit tests link the internal libcmdline/libbase/libutil convenience
# libraries directly, since these symbols are not exported from libddcutil.
# Run by "make check".
LIBCMDLINE = $(top_builddir)/src/cmdline/libcmdline.la
LIBBASE    = $(top_builddir)/src/base/libbase.la
LIBUTIL    = $(top_builddir)/src/util/libutil.la

# libbase transitively pulls the udev/drm/dbus/acl util modules, and (via
# base modules' X11 fallbacks) X11/Xrandr.
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif

check_PROGRAMS = ../../unit_test_executables/cmdline/test_cmd_parser_aux ../../unit_test_executables/cmdline/test_cmd_parser_goption ../../unit_test_executables/cmdline/test_parsed_cmd
TESTS = ../../unit_test_executables/cmdline/test_cmd_parser_aux ../../unit_test_executables/cmdline/test_cmd_parser_goption ../../unit_test_executables/cmdline/test_parsed_cmd

______unit_test_executables_cmdline_test_cmd_parser_aux_SOURCES  = test_cmd_parser_aux.c
______unit_test_executables_cmdline_test_cmd_parser_aux_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_cmdline_test_cmd_parser_aux_LDADD    = $(LIBCMDLINE) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_cmdline_test_cmd_parser_goption_SOURCES  = test_cmd_parser_goption.c
______unit_test_executables_cmdline_test_cmd_parser_goption_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_cmdline_test_cmd_parser_goption_LDADD    = $(LIBCMDLINE) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

______unit_test_executables_cmdline_test_parsed_cmd_SOURCES  = test_parsed_cmd.c
______unit_test_executables_cmdline_test_parsed_cmd_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_cmdline_test_parsed_cmd_LDADD    = $(LIBCMDLINE) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
