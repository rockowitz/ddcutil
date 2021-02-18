AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS  = -Wall 
AM_CFLAGS += -Werror
# vcp_feature_codes.c requires extensive changes if -Wpedantic
# AM_CFLAGS += -Wpedantic

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif


CLEANFILES = \
*expand

clean-local:
	@echo "(src/vcp/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/vcp/Makefile) mostlyclean-local"

distclean-local:
	@echo "(src/vcp/Makefile) distclean-local"

dist-hook:
	@echo "(src/vcp/Makefile) dist-hook"

# Intermediate Library
noinst_LTLIBRARIES = libvcp.la

libvcp_la_SOURCES =           \
ddc_command_codes.c           \
parse_capabilities.c          \
parsed_capabilities_feature.c \
persistent_capabilities.c     \
vcp_feature_codes.c           \
vcp_feature_set.c             \
vcp_feature_values.c    

