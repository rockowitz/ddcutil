AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
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
noinst_LTLIBRARIES = libsharedlib.la

libsharedlib_la_SOURCES =     \
   api_base.c \
   api_displays.c \
   api_error_info_internal.c \
   api_metadata.c \
   api_feature_access.c \
   api_capabilities.c \
   api_services_internal.c


