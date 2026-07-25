# File src/unit_tests/Makefile.am
# Parent directory for standalone unit tests of internal (non-exported) modules.

SUBDIRS = base util i2c sysfs cmdline vcp ddc dynvcp
if WATCH_DISPLAYS_COND
SUBDIRS += dw
endif
if ENABLE_SHARED_LIB_COND
SUBDIRS += libmain
endif
if ENABLE_USB_COND
SUBDIRS += usb_util
endif
