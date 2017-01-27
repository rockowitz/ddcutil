AM_CPPFLAGS = \
$(GLIB_CFLAGS) \
-I$(top_srcdir)/src



AM_CFLAGS = -Wall 

if ENABLE_CALLGRAPH_COND
AM_CFLAGS += -fdump-rtl-expand
endif


CLEANFILES = \
*expand 

clean-local:
	@echo "(src/usbutil/Makefile) clean-local"

mostlyclean-local:
	@echo "(src/usbutil/Makefile) mostlyclean-local"


distclean-local:
	@echo "(src/usbutil/Makefile) distclean-local"

# The dist-hook find statements here find nothing, but the corresponding 
# statements in src/Makefile.am do.   why?

dist-hook:
	@echo "(src/usbutil/Makefile) dist-hook. top_distdir=$(top_distdir) distdir=$(distdir)"
	find $(distdir) -name "*.o"
	find $(distdir) -name "*.lo"


if ENABLE_USB_COND
# Intermediate Libraries

noinst_LTLIBRARIES = libusbutil.la

libusbutil_la_SOURCES = \
usb_hid_common.c \
hiddev_reports.c \
hiddev_util.c \
hidraw_util.c \
libusb_reports.c \
libusb_util.c \
base_hid_report_descriptor.c \
hid_report_descriptor.c 


endif

