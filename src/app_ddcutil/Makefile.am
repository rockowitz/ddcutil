AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

if USE_LIBDRM_COND
AM_CPPFLAGS += \
  $(LIBDRM_CFLAGS)
endif


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
noinst_LTLIBRARIES = libappddcutil.la

libappddcutil_la_SOURCES =     \
main.c \
app_capabilities.c \
app_dumpload.c \
app_dynamic_features.c \
app_experimental.c \
app_getvcp.c \
app_probe.c \
app_ddcutil_services.c \
app_setvcp.c \
app_vcpinfo.c \
app_watch.c

if INCLUDE_TESTCASES_COND
libappddcutil_la_SOURCES += app_testcases.c
endif

if ENABLE_ENVCMDS_COND
libappddcutil_la_SOURCES += \
app_interrogate.c
endif
