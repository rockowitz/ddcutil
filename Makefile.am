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


EXTRA_DIST =  README.md NEWS.md

# if ENABLE_GOBJECT_COND
DISTCHECK_CONFIGURE_FLAGS = --enable-introspection
EXTRA_DIST += m4/introspection.m4 
# endif

if USE_DOXYGEN
DOXYDIR = docs
endif



# doc_DATA = AUTHORS COPYING NEWS.md README.md ChangeLog

SUBDIRS = src man data  $(DOXYDIR)

if ENABLE_SHARED_LIB_COND
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ddcutil.pc
# cmakedir = $(datadir)/cmake/Modules
# cmake_DATA = FindDDCUtil.cmake
# EXTRA_DIST += FindDDCUtil.cmake
endif

if ENABLE_SHARED_LIB_COND
libddcdocdir = $(datarootdir)/doc/libddcutil
# libddcdoc_DATA = AUTHORS
endif


# Too many false positives: 
#  alpha.clone.CloneChecker
#  alpha.deadcode.UnreachableCode
#  alpha.core.CastToStruct

# Copied and adapted from colord
# is calling autogen.sh within this file dangerous?
clang:
	$(MAKE) clean;                                  \
	rm -rf clang;                                   \
	scan-build --use-analyzer=/usr/bin/clang        \
	        -o clang-report                         \
		./autogen.sh				\
	        --disable_gobject_api                   \
		--enable-cffi                           \
		--enable-cython                         \
	        ;                                       \
	scan-build --use-analyzer=/usr/bin/clang        \
	        -o clang-report                         \
		-enable-checker alpha.core.CastSize       \
		-enable-checker alpha.core.BoolAssignment \
		-enable-checker alpha.core.Conversion     \
		-enable-checker alpha.core.SizeofPtr      \
		make



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




.PHONY: clang show

# ldconfig fails when executed in pbuilder due to permissions
# just have to tell users to run in manually

# install-exec-local:
# 	@echo "(install-exec-local):"
# 	ldconfig

# uninstall-local:
# 	@echo "(uninstall-local):="
#	ldconfig

install-data-local:
	@echo "(install-data-local):"
	@echo "  docdir                 = $(docdir)"



# rpmarch=`$(RPM) --showrc | grep "^build arc" | \
# 	sed 's/\(.*: \)\(.*\)/\2/'`; \
#	echo "$$rpmarch" ;\
#test -z "obs/$$rpmarch" || \
#	( mv obs/$$rpmarch/* . && rm -rf /obs/$$rpmarch )
#rm -rf obs/$(distdir)
