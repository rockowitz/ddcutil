# File src/unit_tests/i2c/Makefile.am
# Unit tests for modules in src/i2c.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC


# Unit tests link the internal libi2c/libbase/libutil convenience libraries
# directly, since these symbols are not exported from libddcutil. Run by
# "make check".
LIBI2C  = $(top_builddir)/src/i2c/libi2c.la
LIBBASE = $(top_builddir)/src/base/libbase.la
LIBUTIL = $(top_builddir)/src/util/libutil.la

# libi2c transitively pulls the sysfs, udev, drm, dbus, and acl util modules
# (bus/EDID/connector inspection, driver-name lookup), so these tests link
# their libs.  libsysfs's DPMS fallback (sysfs_dpms.c) also pulls X11/Xrandr.
LIBSYSFS = $(top_builddir)/src/sysfs/libsysfs.la
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif

check_PROGRAMS = ../../unit_test_executables/i2c/test_i2c_bus_collections ../../unit_test_executables/i2c/test_i2c_bus_core \
  ../../unit_test_executables/i2c/test_i2c_bus_selector ../../unit_test_executables/i2c/test_i2c_edid ../../unit_test_executables/i2c/test_i2c_execute ../../unit_test_executables/i2c/test_i2c_services \
  ../../unit_test_executables/i2c/test_i2c_strategy_dispatcher
TESTS = ../../unit_test_executables/i2c/test_i2c_bus_collections ../../unit_test_executables/i2c/test_i2c_bus_core \
  ../../unit_test_executables/i2c/test_i2c_bus_selector ../../unit_test_executables/i2c/test_i2c_edid ../../unit_test_executables/i2c/test_i2c_execute ../../unit_test_executables/i2c/test_i2c_services \
  ../../unit_test_executables/i2c/test_i2c_strategy_dispatcher

______unit_test_executables_i2c_test_i2c_bus_collections_SOURCES  = test_i2c_bus_collections.c
______unit_test_executables_i2c_test_i2c_bus_collections_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_bus_collections_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_i2c_test_i2c_bus_core_SOURCES  = test_i2c_bus_core.c
______unit_test_executables_i2c_test_i2c_bus_core_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_bus_core_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_i2c_test_i2c_bus_selector_SOURCES  = test_i2c_bus_selector.c
______unit_test_executables_i2c_test_i2c_bus_selector_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_bus_selector_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_i2c_test_i2c_edid_SOURCES  = test_i2c_edid.c
______unit_test_executables_i2c_test_i2c_edid_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_edid_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_i2c_test_i2c_execute_SOURCES  = test_i2c_execute.c
______unit_test_executables_i2c_test_i2c_execute_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_execute_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_i2c_test_i2c_services_SOURCES  = test_i2c_services.c
______unit_test_executables_i2c_test_i2c_services_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_services_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_i2c_test_i2c_strategy_dispatcher_SOURCES  = test_i2c_strategy_dispatcher.c
______unit_test_executables_i2c_test_i2c_strategy_dispatcher_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_i2c_test_i2c_strategy_dispatcher_LDADD    = $(LIBI2C) $(LIBSYSFS) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)
