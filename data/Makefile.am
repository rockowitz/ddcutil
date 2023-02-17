# File data/Makefile.am

# Copyright (C) 2016-2023 Sanford Rockowitz <rockowitz@minsoft.com>
# SPDX-License-Identifier: GPL-2.0-or-later

resfiles = \
	etc/udev/rules.d/45-ddcutil-i2c.rules \
	etc/udev/rules.d/45-ddcutil-usb.rules \
	etc/X11/xorg.conf.d/90-nvidia-i2c.conf

rulesfiles = \
	usr/lib/udev/rules.d/60-ddcutil.rules

distributed_modulesfiles = \
    usr/lib/modules-load.d/ddcutil.conf \
    usr/lib/modules-load.d/libddcutil.conf

installed_modulesfiles  =  usr/lib/modules-load.d/ddcutil.conf 
if ENABLE_SHARED_LIB_COND
 installed_modulesfiles += usr/lib/modules-load.d/libddcutil.conf
endif

# Causes files (with directory structure) to be included in tarball:
EXTRA_DIST = $(resfiles) $(rulesfiles) $(distributed_modulesfiles)

# Target directory (/usr/local/share/ddcutil/data or /usr/share/ddcutil/data):
ddcutildir = $(datadir)/ddcutil/data
# Causes files (w/o directory structure) to be installed in target directory: 
ddcutil_DATA = $(resfiles)

# Use prefix instead of libdir here because it appears that 
# udev/rules.d is always a subdirectory of /usr/lib
rulesdir = ${prefix}/lib/udev/rules.d
rules_DATA = $(rulesfiles)

# Similar comment for modules-load.d
installed_modulesdir = $(prefix)/lib/modules-load.d
installed_modules_DATA  = $(installed_modulesfiles)

# include FindDDCUtil.cmake in tarball:
EXTRA_DIST += cmake/ddcutil/FindDDCUtil.cmake
if ENABLE_SHARED_LIB_COND
   # where make install finds FindDDCUtil.cmake:
   cmake_DATA = cmake/ddcutil/FindDDCUtil.cmake
   # where FindDDCUtil.cmake will installed:
   cmakedir = $(libdir)/cmake/ddcutil
endif


# n. -local executes before target, -hook executes after

all-local:
	@echo "(data/Makefile) ==>  Executing rule: all-local"

install-data-local:
	@echo "(data/Makefile) ==>  Executing rule: install-data-local"
	@echo "prefix:         ${prefix}"
	@echo "includedir      ${includedir}"
	@echo "docdir          ${docdir}"
	@echo "libdir          ${libdir}"
	@echo "rulesdir        ${rulesdir}"
	@echo "packagedatadir: $(packagedatadir)"
	@echo "datadir:        $(datadir)"
	@echo "ddcutildir:     $(ddcutildir)"
	@echo "srcdir:         $(srcdir)"
	@echo "bindir:         ${bindir}"
	@echo "cmakedir:       ${cmakedir}"
	@echo "DESTDIR:        ${DESTDIR}"
	@echo "rulesfiles:     ${rulesfiles}"
	@echo "resfiles:       ${resfiles}"
	@echo "udevdir:        ${udevdir}"

# @xxx@ names are not defined, names with $() are 
install-data-hook:
	@echo "(data/install-data-hook)===> Executing rule: install-data-hook"

