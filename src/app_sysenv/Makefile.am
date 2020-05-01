AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public


if USE_LIBDRM_COND
AM_CPPFLAGS += \
  $(LIBDRM_CFLAGS)
endif


AM_CFLAGS = -Wall 
AM_CFLAGS += -Werror
# AM_CFLAGS += -Wpedantic

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libappsysenv.la

libappsysenv_la_SOURCES =     \
query_sysenv.c \
query_sysenv_access.c \
query_sysenv_base.c \
query_sysenv_dmidecode.c \
query_sysenv_i2c.c \
query_sysenv_logs.c \
query_sysenv_modules.c \
query_sysenv_procfs.c \
query_sysenv_sysfs.c \
query_sysenv_xref.c 

if ENABLE_USB_COND
libappsysenv_la_SOURCES += \
   query_sysenv_usb.c
endif

if USE_LIBDRM_COND
libappsysenv_la_SOURCES += \
   query_sysenv_drm.c
endif

