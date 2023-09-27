# File data/Makefile.am

# Copyright (C) 2016-2023 Sanford Rockowitz <rockowitz@minsoft.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# The proper location for pkgconfig files is ambiguous, and the 
# subject of much discussion.  .
# However, it appears that: 
#   - /usr/lib64/pkgconfig should hold x64 specific pkgconfig files
#   - /usr/lib/pkgconfig should hold i386 specific pkgconfig files
#   - /usr/share/pkgconfig should hold architecture agnostic pkgconfig files
#
# The tail wags the dog. libddcutil-dev can contain
#   usr/*/pkgconfig
# or 
#   usr/lib/x86_64-linux-gnu/pkgconfig
# but there's no way to "or" these two statements. 
#
# When building on OBS, the same libddcutil-dev.install is used for all (Debian based) builds, 
# and if using $(libdir) the location of ddcutil.pc will depend on platform, e.g. 
# some times it will be /usr/lib/x86_64-linux-gnu/pkgconfig, sometimes something else. 
# 
# Note the the entry for ddcutil.pc in the Debian .install file needs
# to be kept in sync with where autotools puts the files.



resfiles = \
	etc/modprobe.d/nvidia-i2c.conf \
	etc/udev/rules.d/60-ddcutil-i2c.rules \
	etc/udev/rules.d/60-ddcutil-usb.rules \
	etc/X11/xorg.conf.d/90-nvidia-i2c.conf

rulesfiles = \
	usr/lib/udev/rules.d/60-ddcutil-i2c.rules \
	usr/lib/udev/rules.d/60-ddcutil-usb.rules

distributed_modulesfiles = \
    usr/lib/modules-load.d/ddcutil.conf

if !INSTALL_LIB_ONLY_COND
installed_modulesfiles = \
    usr/lib/modules-load.d/ddcutil.conf
endif 

pkgconfigfiles = \
    ddcutil.pc

# Causes files (with directory structure) to be included in tarball:
EXTRA_DIST = $(resfiles) $(rulesfiles) $(distributed_modulesfiles) ddcutil.pc.in

# Target directory
pkgconfigdir = ${libdir}/pkgconfig
   
# Target directory (/usr/local/share/ddcutil/data or /usr/share/ddcutil/data):
ddcutildir = $(datadir)/ddcutil/data
# Causes files (w/o directory structure) to be installed in target directory:

if !INSTALL_LIB_ONLY_COND
ddcutil_DATA = $(resfiles)
pkgconfig_DATA = ddcutil.pc
endif

# Use prefix instead of libdir here because it appears that 
# udev/rules.d is always a subdirectory of /usr/lib
rulesdir = ${prefix}/lib/udev/rules.d
if !INSTALL_LIB_ONLY_COND
rules_DATA = $(rulesfiles)
endif

# Similar comment for modules-load.d
installed_modulesdir = $(prefix)/lib/modules-load.d
if !INSTALL_LIB_ONLY_COND
installed_modules_DATA  = $(installed_modulesfiles)
endif

# include FindDDCUtil.cmake in tarball:
EXTRA_DIST += cmake/ddcutil/FindDDCUtil.cmake
# where FindDDCUtil.cmake will installed:
cmakedir = $(libdir)/cmake/ddcutil

if ENABLE_SHARED_LIB_COND
if !INSTALL_LIB_ONLY_COND
# where make install finds FindDDCUtil.cmake:
cmake_DATA = cmake/ddcutil/FindDDCUtil.cmake
endif
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
if !INSTALL_LIB_ONLY_COND
	sed  -i "s|/usr|${prefix}|" ${DESTDIR}${rulesdir}/60-ddcutil-usb.rules 
endif
	@echo "Clear out files possibly left over from earlier installation"
	rm -f ${DESTDIR}${resdir}/45-ddcutil*
	rm -f ${DESTDIR}${rulesdir}/60-ddcutil.rules

uninstall-hook:
	@echo "(data/uninstall-hook)===> Executing rule: uninstall-hook"

