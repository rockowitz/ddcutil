AM_CPPFLAGS =        \
  $(GLIB_CFLAGS)     \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = $(AM_CFLAGS_STD)


CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libddc.la

libddc_la_SOURCES =         \
common_init.c               \
ddc_async.c                 \
ddc_displays.c              \
ddc_display_lock.c          \
ddc_dumpload.c              \
ddc_multi_part_io.c         \
ddc_output.c                \
ddc_packet_io.c             \
ddc_read_capabilities.c     \
ddc_services.c              \
ddc_strategy.c              \
ddc_vcp.c                   \
ddc_vcp_version.c           \
ddc_try_stats.c 

if ENABLE_SHARED_LIB_COND
libddc_la_SOURCES += \
  ddc_watch_displays.c
endif

# Rename to "all=local" for development 
all-local-disabled:
	@echo ""
	@echo "(src/Makefile:all-local) Variable values:"
	@echo "   CLEANFILES:       $(CLEANFILES)"
	@echo "   CFLAGS:           $(CFLAGS)"
	@echo "   AM_CFLAGS:        $(AM_CFLAGS)"
	@echo "   CPPFLAGS:         $(CPPFLAGS)"
	@echo "   AM_CPPFLAGS:      $(AM_CPPFLAGS)"
	@echo "   ddcutil_FLAGS:    $(ddcutil_CFLAGS)"
	@echo "   AUTOMAKE_OPTIONS: $(AUTOMAKE_OPTIONS)"
	@echo "   MAKELEVEL:        $(MAKELEVEL)"
	@echo "   MAKEFLAGS:        $(MAKEFLAGS)"
	@echo "   V:                $(V)"
	@echo "   AM_CFLAGS_STD:    $(AM_CFLAGS_STD)"
	@echo ""
