AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = $(AM_CFLAGS_STD)

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libdynvcp.la

libdynvcp_la_SOURCES =      \
dyn_feature_set.c           \
dyn_parsed_capabilities.c   \
dyn_feature_codes.c         \
dyn_feature_files.c
