# to automatically update libtool script if it becomes out of date
LIBTOOL_DEPS = @LIBTOOL_DEPS@

# From autoconf manual: 
#   ... if you use aclocal from Automake to generate aclocal.m4, you must also 
#   set ACLOCAL_AMFLAGS = -I dir in your top-level Makefile.am. 
#   Due to a limitation in the Autoconf implementation of autoreconf, these 
#   include directives currently must be set on a single line in Makefile.am, 
#   without any backslash-newlines
# Introspection does this. 
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = --enable-introspection
EXTRA_DIST = m4/introspection.m4 README.md

if HAVE_DOXYGEN
DOXYDIR = docs
endif

SUBDIRS = src man data package $(DOXYDIR)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ddcutil.pc



RPM = rpm

rpms srcrpm: dist
	@echo "----> Makefile: rpms srcrpm"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

obsrpm:  
	echo "---> Makefile: obsrpm"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

dpkg:  dist
	echo "---> Makefile: dpkg"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

pdebuild:  dist
	echo "---> Makefile: pdebuild"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1


obsdeb:  
	echo "---> Makefile: obsdeb"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

ppa:  
	echo "---> Makefile: ppa"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1


# Copied and adapted from colord
# is calling autogen.sh within this file dangerous?
clang:
	$(MAKE) clean;                                  \
	rm -rf clang;                                   \
	scan-build --use-analyzer=/usr/bin/clang        \
	        -o clang-report                         \
		./autogen.sh				\
	        --disable_gobject_api                   \
	        ;                                       \
	scan-build --use-analyzer=/usr/bin/clang        \
	        -o clang-report make


# Copied and adapted from colord
#	export COVERITY_EMAIL=`cat ~/.coverity | grep EMAIL | cut -f2`; \
#	export COVERITY_PASSWORD=`cat ~/.coverity | grep PASSWORD_ddcutil | cut -f2`; \
coverity:
	(MAKE) clean                    \
	cov-build --dir cov-int $(MAKE);        \
	tar czvf ddcutil-coverity.tgz cov-int;       \
	export COVERITY_EMAIL=`cat ~/.coverity | grep EMAIL | cut -f2`; \
	export COVERITY_PASSWORD=`cat ~/.coverity | grep PASSWORD_colord | cut -f2`; \
	curl --form file=@ddcutil-coverity.tgz       \
	     --form project=ddcutil   \
	     --form password=${COVERITY_PASSWORD}   \
	     --form email=${COVERITY_EMAIL}     \
	     http://scan5.coverity.com/cgi-bin/upload.py


# 	$(foreach v, $(.VARIABLES), @echo "$v = $$v")
show:
	@echo "---> Show variables"
	@echo "Set by AM_PATH_PYTHON:"
	@echo "  PYTHON               = $(PYTHON) "
	@echo "  PYTHON_PREFIX        = $(PYTHON_PREFIX) "
	@echo "  PYTHON_EXEC_PREFIX   = $(PYTHON_EXEC_PREFIX) "
	@echo "  pythondir            = $(pythondir) "
	@echo "  pkgpythondir         = $(pkgpythondir) "
	@echo "  pyexecdir            = $(pyexecdir) "
	@echo "  pkgpyexecdir         = $(pkgpyexecdir) "
	@echo ""
	@echo "Explicitly set:"
	@echo "  PYTHONDIR            = $(PYTHONDIR) "
	@echo "  PKGPYTHONDIR         = $(PKGPYTHONDIR) "
	@echo "  PYEXECDIR            = $(PYEXECDIR) "
	@echo "  PKGPYEXECDIR         = $(PKGPYEXECDIR) "
	@echo ""
	@echo "Set by AX_PYTHON:"
	@echo "  PYTHON_BIN           = $(PYTHON_BIN), @PYTHON_BIN@ "
	@echo "  PYTHON_INCLUDE_DIR   = $(PYTHON_INCLUDE_DIR) "
	@echo "  PYTHON_LIB           = $(PYTHON_LIB) "
	@echo ""
	@echo "Set by AX_PYTHON_DEVEL:"
	@echo "  PYTHON_CPPFLAGS      = $(PYTHON_CPPFLAGS), @PYTHON_CPPFLAGS@ "
	@echo "  PYTHON_LIBS          = $(PYTHON_LIBS) "
	@echo "  PYTHON_LDFLAGS       = $(PYTHON_LDFLAGS)"
	@echo "  PYTHON_EXTRA_LIBS    = $(PYTHON_EXTRA_LIBS) "
	@echo "  PYTHON_EXTRA_LDFLAGS = $(PYTHON_EXTRA_LDFLAGS) "
	@echo "  PYTHON_SITE_PKG      = $(PYTHON_SITE_PKG)"
	@echo "  PYTHON_SITE_PKG_EXEC = $(PYTHON_SITE_PKG_EXEC)"
	@echo "  PYTHON_CFLAGS        = $(PYTHON_CFLAGS)"
	@echo ""
	@echo "Set by AX_SWIG_PYTHON:"
	@echo "  AX_SWIG_PYTHON_CPPFLAGS = $(AX_SWIG_PYTHON_CPPFLAGS), @AX_SWIG_PYTHON_CPPFLAGS@"
	@echo "  AX_SWIG_PYTHON_OPT      = $(AS_SWIG_PYTHON_OPT)"
	@echo "  AX_SWIG_PYTHON_LIBS     = $(AX_SWIG_PYTHON_LIBS)"
	@echo ""
	@echo "Set by AX_PKG_SWIG:"
	@echo "  SWIG                    = $(SWIG), @SWIG@"
	@echo "  SWIG_LIB                = $(SWIG_LIB)"
	@echo ""
	@echo "Set by PKG_CHECK_MODULES:"
	@echo "  PYTHON_CFLAGS           = $(PYTHON_CFLAGS), @PYTHON_CFLAGS@)"
	@echo "  PYTHON_LIBS             = $(PYTHON_LIBS)"
	@echo "  GLIB_CFLAGS             = $(GLIB_CFLAGS) "
	@echo "  GLIB_LIBS               = $(GLIB_LIBS)"
	@echo "  UDEV_CFLAGS             = $(UDEV_CFLAGS)"
	@echo "  UDEV_LIBS               = $(UDEV_LIBS)"
	@echo "  SYSTEMD_CFLAGS          = $(SYSTEMD_CFLAGS)"
	@echo "  SYSTEMD_LIBS            = $(SYSTEMD_LIBS)" 
	@echo "  LIBUSB_CFLAGS           = $(LIBUSB_CFLAGS)"
	@echo "  LIBUSB_LIBS             = $(LIBUSB_LIBS)"
	@echo "  GOBJECT_CFLAGS          = $(GOBJECT_CFLAGS)"
	@echo "  GOBJECT_LIBS            = $(GOBJECT_LIBS)"
	@echo ""
	@echo "Addtional:"
	@echo "  prefix                  = $(prefix)"
	@echo "  exec_prefix             = $(exec_prefix)"
	@echo "  libdir                  = $(libdir)"
	@echo "  libexecdir              = $(libexecdir)"
	@echo "  top_srcdir              = $(top_srcdir)"
	@echo "  srcdir                  = $(srcdir)"
	@echo "  pythondir               = $(pythondir)"
	@echo "  pyexecdir               = $(pyexecdir)"




.PHONY: srcrpm rpms obsrpm dpkg obsdeb ppa clang show



# rpmarch=`$(RPM) --showrc | grep "^build arc" | \
# 	sed 's/\(.*: \)\(.*\)/\2/'`; \
#	echo "$$rpmarch" ;\
#test -z "obs/$$rpmarch" || \
#	( mv obs/$$rpmarch/* . && rm -rf /obs/$$rpmarch )
#rm -rf obs/$(distdir)
