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

# Copy the built test executables to src/unit_test_executables/cmdline
# after `make check` builds them here, so the compiled binaries land
# in a directory separate from these sources. Does not change how or
# where compilation/linking itself happens.
EXEC_OUTPUT_DIR = $(top_builddir)/src/unit_test_executables/cmdline

check-local: $(check_PROGRAMS)
	@mkdir -p $(EXEC_OUTPUT_DIR)
	@for p in $(check_PROGRAMS); do cp -f $$p $(EXEC_OUTPUT_DIR)/; done

clean-local:
	@for p in $(check_PROGRAMS); do rm -f $(EXEC_OUTPUT_DIR)/$$p; done

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

check_PROGRAMS = test_cmd_parser_aux test_cmd_parser_goption test_parsed_cmd
TESTS = test_cmd_parser_aux test_cmd_parser_goption test_parsed_cmd

test_cmd_parser_aux_SOURCES  = test_cmd_parser_aux.c
test_cmd_parser_aux_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_cmd_parser_aux_LDADD    = $(LIBCMDLINE) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_cmd_parser_goption_SOURCES  = test_cmd_parser_goption.c
test_cmd_parser_goption_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_cmd_parser_goption_LDADD    = $(LIBCMDLINE) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm

test_parsed_cmd_SOURCES  = test_parsed_cmd.c
test_parsed_cmd_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_parsed_cmd_LDADD    = $(LIBCMDLINE) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS) -lm
