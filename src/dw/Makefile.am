# src/dw/Makefile.am

AM_CPPFLAGS =         \
  $(LIBDRM_CFLAGS)    \
  $(GLIB_CFLAGS)      \
  $(JANSSON_CFLAGS)   \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/public

AM_CFLAGS = $(AM_CFLAGS_STD)


CLEANFILES = \
*expand

# Intermediate Library
noinst_LTLIBRARIES = libdw.la

libdw_la_SOURCES =         \
ddc_status_events.c         

if ENABLE_UDEV_COND
libdw_la_SOURCES += \
  ddc_dw_common.c \
  ddc_dw_main.c \
  ddc_dw_poll.c \
  ddc_dw_dref.c \
  ddc_dw_udev.c \
  ddc_dw_xevent.c
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
