# to automatically update libtool script if it becomes out of date
LIBTOOL_DEPS = @LIBTOOL_DEPS@

# *** missing separator (did you mean TAB instead of 8 spaces?).  Stop.
# libtool: $(LIBTOOL_DEPS)
#    $(SHELL) ./config.status libtool



SUBDIRS = src man
dist_data_DATA = \
	data/etc/udev/rules.d/45-i2c-tools.rules \
	data/etc/X11/xorg.conf.d/90-nvidia-i2c.conf


resfiles = \
	data/etc/udev/rules.d/45-i2c-tools.rules \
	data/etc/X11/xorg.conf.d/90-nvidia-i2c.conf
resdir = @DATADIR@/ddctool/data
ddctooldir = @DATADIR@/ddctool/data
ddctool_DATA = $(resfiles)
EXTRA_DIST = $(resfiles)


# echo $(packagedatadir) 
# echo $(datadir)
# echo @DATADIR@
# echo @PACKAGEDATADIR@
# echo $(ddctooldir)

