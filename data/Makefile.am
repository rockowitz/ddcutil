
resfiles = \
	etc/udev/rules.d/45-ddctool-i2c.rules \
	etc/udev/rules.d/45-ddctool-usb.rules \
	etc/X11/xorg.conf.d/90-nvidia-i2c.conf 
# resdir: not used
# resdir = $(datadir)/ddctool/data
ddctooldir = $(datadir)/ddctool/data
# ddctool_DATA requires ddctooldir
# Causes files (w/o directory structure) to be installed in /usr/local/sare/ddctool/data:
ddctool_DATA = $(resfiles)

# Causes files (with directory structure) to be included in tarball:
EXTRA_DIST = $(resfiles)

install-data-local:
	@echo "Executing rule: install-data-local"

# @xxx@ names are not defined, names with $() are 
install-data-hook:
	@echo "Executing rule: install-data-hook"
	@echo "packagedatadir: $(packagedatadir)"
	@echo "datadir:        $(datadir)"
	@echo "ddctooldir:     $(ddctooldir)"
	@echo "srcdir:         $(srcdir)"


