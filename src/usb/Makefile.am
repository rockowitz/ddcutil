AM_CPPFLAGS =   \
$(GLIB_CFLAGS)  \
-I$(top_srcdir) \
-I$(top_srcdir)/src \
-I$(top_srcdir)/src/public 

AM_CFLAGS = $(AM_CFLAGS_STD)

CLEANFILES = \
*expand 

clean-local:
	@echo "(src/usb/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/usb/Makefile) mostlyclean-local"


distclean-local:
	@echo "(src/usb/Makefile) distclean-local"


dist-hook:
	@echo "(src/usb/Makefile) dist-hook. top_distdir=$(top_distdir) distdir=$(distdir)"
	find $(distdir) -name "*.o"
	find $(distdir) -name "*.lo"


if ENABLE_USB_COND
# Intermediate library
noinst_LTLIBRARIES = libusb.la

libusb_la_SOURCES = \
usb_base.c      \
usb_displays.c  \
usb_edid.c      \
usb_services.c  \
usb_vcp.c  
endif
