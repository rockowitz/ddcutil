# File data/Makefile.am

resfiles = \
	etc/udev/rules.d/45-ddcutil-i2c.rules \
	etc/udev/rules.d/45-ddcutil-usb.rules \
	etc/X11/xorg.conf.d/90-nvidia-i2c.conf

# Causes files (with directory structure) to be included in tarball:
EXTRA_DIST = $(resfiles)

ddcutildir = $(datadir)/ddcutil/data
# ddcutil_DATA requires ddcutildir
# Causes files (w/o directory structure) to be installed in /usr/local/share/ddcutil/data:
# or /usr/share/ddcutil/data
ddcutil_DATA = $(resfiles)

if ENABLE_SHARED_LIB_COND
   # where FindDDCUtil.cmake will installed: 
   cmakedir = $(datadir)/cmake/Modules
   # cmakedir = $(libdir)/cmake/Modules
      
   # where FindDDCUtil.cmake is found: 
   cmake_DATA = usr/share/cmake/Modules/FindDDCUtil.cmake

   EXTRA_DIST += usr/share/cmake/Modules/FindDDCUtil.cmake
endif


# executes before install-data
install-data-local:
	@echo "(data/Makefile) ==>  Executing rule: install-data-local"
	@echo "prefix:         ${prefix}"
	@echo "includedir      ${includedir}"
	@echo "docdir          ${docdir}"
	@echo "libdir          ${libdir}"
	@echo "packagedatadir: $(packagedatadir)"
	@echo "datadir:        $(datadir)"
	@echo "ddcutildir:     $(ddcutildir)"
	@echo "srcdir:         $(srcdir)"
	@echo "bindir:         ${bindir}"
	@echo "cmakedir:       ${cmakedir}"
	@echo "DESTDIR:        ${DESTDIR}"

# alternative to using cmakedir/cmake_DATA:
# @echo "looking in usr/share/cmake/Modules for file to copy:"
# ls usr/share/cmake/Modules
# mkdir -p ${DESTDIR}${datadir}/cmake/Modules
# cp -p usr/share/cmake/Modules/FindDDCUtil.cmake ${DESTDIR}${datadir}/cmake/Modules
# @echo "(data/Makefile) ==>  Done: install-data-local"

# @xxx@ names are not defined, names with $() are 
# executes after install-data
install-data-hook:
	@echo "(data/install-data-hook)===> Executing rule: install-data-hook"

