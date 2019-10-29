
resfiles = \
	etc/udev/rules.d/45-ddcutil-i2c.rules \
	etc/udev/rules.d/45-ddcutil-usb.rules \
	etc/X11/xorg.conf.d/90-nvidia-i2c.conf

if ENABLE_SHARED_LIB_COND
# resfiles += usr/share/cmake/Modules/FindDDCUtil.cmake
endif


# resdir: not used
# resdir = $(datadir)/ddcutil/data
ddcutildir = $(datadir)/ddcutil/data
# ddcutil_DATA requires ddcutildir
# Causes files (w/o directory structure) to be installed in /usr/local/share/ddcutil/data:
# or /usr/share/ddcutil/data
ddcutil_DATA = $(resfiles)

if ENABLE_SHARED_LIB_COND
   cmakedir = $(datadir)/cmake/Modules
   cmake_DATA = usr/share/cmake/Modules/FindDDCUtil.cmake
endif


# Causes files (with directory structure) to be included in tarball:
EXTRA_DIST = $(resfiles)
# EXTRA_DIST += usr/share/cmake/Modules/FindDDCUtil.cmake

install-data-local:
	@echo "(data/Makefile) Executing rule: install-data-local"

install-cmake-local:
	@echo "(data/Makefile) Executing rule: install-cmake-local"

# @xxx@ names are not defined, names with $() are 
install-data-hook:
	@echo "Executing rule: install-data-hook"
	@echo "packagedatadir: $(packagedatadir)"
	@echo "datadir:        $(datadir)"
	@echo "ddcutildir:     $(ddcutildir)"
	@echo "srcdir:         $(srcdir)"
	@echo "bindir:         ${bindir}"
	@echo "cmakedir:       ${cmakedir}"

