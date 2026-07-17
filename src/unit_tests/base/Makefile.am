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

check_PROGRAMS = test_core test_core_per_thread_settings
TESTS = test_core test_core_per_thread_settings

test_core_SOURCES  = test_core.c
test_core_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_core_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)

test_core_per_thread_settings_SOURCES  = test_core_per_thread_settings.c
test_core_per_thread_settings_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
test_core_per_thread_settings_LDADD    = $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS)
