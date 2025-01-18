AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  $(LIBDRM_CFLAGS)   \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = -Wall
if WARNINGS_ARE_ERRORS_COND
AM_CFLAGS += -Werror
endif
# AM_CFLAGS += -Wpedantic

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libsysfs.la

libsysfs_la_SOURCES =       \
sysfs_dpms.c                \
sysfs_sys_drm_connector.c   \
sysfs_base.c          \
sysfs_conflicting_drivers.c \
sysfs_i2c_info.c      \
sysfs_i2c_sys_info.c  \
sysfs_top.c           \
sysfs_services.c
