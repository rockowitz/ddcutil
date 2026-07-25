# File src/unit_tests/sysfs/Makefile.am
# Unit tests for modules in src/sysfs.

# serial-tests: use the simple test runner (run each TEST, check exit status)
# rather than the parallel-tests harness, which requires config/test-driver.
AUTOMAKE_OPTIONS = serial-tests

AM_CPPFLAGS =    \
-I$(srcdir)      \
-I$(top_srcdir)/src/public \
-I$(top_srcdir)/src

AM_CFLAGS = -Wall -fPIC


# Unit tests link the internal libsysfs/libi2c/libbase/libutil convenience
# libraries directly, since these symbols are not exported from libddcutil.
# Run by "make check".
LIBSYSFS = $(top_builddir)/src/sysfs/libsysfs.la
LIBI2C   = $(top_builddir)/src/i2c/libi2c.la
LIBBASE  = $(top_builddir)/src/base/libbase.la
LIBUTIL  = $(top_builddir)/src/util/libutil.la

# libsysfs transitively pulls the udev, drm, dbus, and acl util modules, and
# (via sysfs_dpms.c's X11 DPMS fallback) X11/Xrandr.
EXTRA_SYSLIBS = $(UDEV_LIBS) $(LIBDRM_LIBS) $(DBUS_LIBS) $(ACL_LIBS)
if USE_X11_COND
EXTRA_SYSLIBS += $(LIBX11_LIBS) $(XEXT_LIBS) $(XRANDR_LIBS)
endif

check_PROGRAMS = ../../unit_test_executables/sysfs/test_sysfs_base ../../unit_test_executables/sysfs/test_sysfs_conflicting_drivers \
  ../../unit_test_executables/sysfs/test_sysfs_dpms ../../unit_test_executables/sysfs/test_sysfs_i2c_info ../../unit_test_executables/sysfs/test_sysfs_i2c_sys_info \
  ../../unit_test_executables/sysfs/test_sysfs_services ../../unit_test_executables/sysfs/test_sysfs_sys_drm_connector
TESTS = ../../unit_test_executables/sysfs/test_sysfs_base ../../unit_test_executables/sysfs/test_sysfs_conflicting_drivers \
  ../../unit_test_executables/sysfs/test_sysfs_dpms ../../unit_test_executables/sysfs/test_sysfs_i2c_info ../../unit_test_executables/sysfs/test_sysfs_i2c_sys_info \
  ../../unit_test_executables/sysfs/test_sysfs_services ../../unit_test_executables/sysfs/test_sysfs_sys_drm_connector

______unit_test_executables_sysfs_test_sysfs_base_SOURCES  = test_sysfs_base.c
______unit_test_executables_sysfs_test_sysfs_base_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_base_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_sysfs_test_sysfs_conflicting_drivers_SOURCES  = test_sysfs_conflicting_drivers.c
______unit_test_executables_sysfs_test_sysfs_conflicting_drivers_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_conflicting_drivers_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_sysfs_test_sysfs_dpms_SOURCES  = test_sysfs_dpms.c
______unit_test_executables_sysfs_test_sysfs_dpms_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_dpms_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_sysfs_test_sysfs_i2c_info_SOURCES  = test_sysfs_i2c_info.c
______unit_test_executables_sysfs_test_sysfs_i2c_info_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_i2c_info_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_sysfs_test_sysfs_i2c_sys_info_SOURCES  = test_sysfs_i2c_sys_info.c
______unit_test_executables_sysfs_test_sysfs_i2c_sys_info_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_i2c_sys_info_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_sysfs_test_sysfs_services_SOURCES  = test_sysfs_services.c
______unit_test_executables_sysfs_test_sysfs_services_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_services_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)

______unit_test_executables_sysfs_test_sysfs_sys_drm_connector_SOURCES  = test_sysfs_sys_drm_connector.c
______unit_test_executables_sysfs_test_sysfs_sys_drm_connector_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)
______unit_test_executables_sysfs_test_sysfs_sys_drm_connector_LDADD    = $(LIBSYSFS) $(LIBI2C) $(LIBBASE) $(LIBUTIL) $(GLIB_LIBS) $(EXTRA_SYSLIBS)
