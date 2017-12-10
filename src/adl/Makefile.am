AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CPPFLAGS +=  -I@ADL_HEADER_DIR@

AM_CFLAGS = -Wall 
AM_CFLAGS += -Werror

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif

CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libadl.la

libadl_la_SOURCES = 

if HAVE_ADL_COND
libadl_la_SOURCES += \
  adl_impl/adl_aux_intf.c \
  adl_impl/adl_errors.c \
  adl_impl/adl_intf.c \
  adl_impl/adl_report.c \
  adl_impl/adl_shim.c
else
libadl_la_SOURCES += \
  adl_mock_impl/adl_mock_shim.c \
  adl_mock_impl/adl_mock_errors.c
endif


