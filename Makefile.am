# Top level Makefile.am

# Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# To automatically update libtool script if it becomes out of date
LIBTOOL_DEPS = @LIBTOOL_DEPS@

# From autoconf manual: 
#   ... if you use aclocal from Automake to generate aclocal.m4, you must also 
#   set ACLOCAL_AMFLAGS = -I dir in your top-level Makefile.am. 
#   Due to a limitation in the Autoconf implementation of autoreconf, these 
#   include directives currently must be set on a single line in Makefile.am, 
#   without any backslash-newlines
# Introspection does this. 
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DIST_SUBDIRS = src man data docs 
EXTRA_DIST =  README.md NEWS.md CHANGELOG.md
EXTRA_DIST += m4/ax_prog_doxygen.m4 

if USE_DOXYGEN
DOXYDIR = docs
endif

SUBDIRS = src man data  $(DOXYDIR)

# if ENABLE_GOBJECT_COND
DISTCHECK_CONFIGURE_FLAGS = --enable-introspection
# endif
EXTRA_DIST += m4/introspection.m4 

# install-data-local:
#	@echo "(Makefile) install-data-local):"
#	@echo "  docdir                 = $(docdir)"

if ENABLE_SHARED_LIB_COND
  libddcdocdir = $(datarootdir)/doc/libddcutil
# libddcdoc_DATA = AUTHORS
endif

dist-hook:
	echo "(Makefile) Executing dist-hook..."
	chmod a-x ${distdir}/AUTHORS ${distdir}/COPYING ${distdir}/README.md
	find ${distdir}  -name "*~"    -exec rm -v {} \;
	find ${distdir}  -name "*.ctl" -exec rm -v {} \;
	find ${distdir}  -name "*.lst" -exec rm -v {} \;
	find ${distdir}  -name "*.la"  -exec rm -v {} \;
	find ${distdir}  -name "*.old" -exec rm -v {} \;
	find ${distdir}  -name "*.new" -exec rm -v {} \;
	find ${distdir}  -name "*.tmp" -exec rm -v {} \;
	find ${distdir}  -name "*old"  -type d -prune -exec rm -fv {} \;
	find ${distdir}  -name "*new"  -type d -prune -exec rm -fv {} \;
	find ${distdir}  -name ".gitignore"    -exec rm -v {} \;

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
	@echo ""
	@echo "Set by PKG_CHECK_MODULES:"
	@echo "  GLIB_CFLAGS             = $(GLIB_CFLAGS) "
	@echo "  GLIB_LIBS               = $(GLIB_LIBS)"
	@echo "  JANSSON_LIBS            = $(JANSSON_LIBS)"
	@echo "  JANSSON_CFLAGS          = $(JANSSON_CFLAGS)"

	@echo "  UDEV_CFLAGS             = $(UDEV_CFLAGS)"
	@echo "  UDEV_LIBS               = $(UDEV_LIBS)"
	@echo "  SYSTEMD_CFLAGS          = $(SYSTEMD_CFLAGS)"
	@echo "  SYSTEMD_LIBS            = $(SYSTEMD_LIBS)" 
	@echo "  LIBUSB_CFLAGS           = $(LIBUSB_CFLAGS)"
	@echo "  LIBUSB_LIBS             = $(LIBUSB_LIBS)"
	@echo ""
	@echo "Addtional:"
	@echo "  prefix                  = $(prefix)"
	@echo "  exec_prefix             = $(exec_prefix)"
	@echo "  libdir                  = $(libdir)"
	@echo "  libexecdir              = $(libexecdir)"
	@echo "  top_srcdir              = $(top_srcdir)"
	@echo "  srcdir                  = $(srcdir)"
	@echo "  pkgconfigdir:           = ${pkgconfigdir}"
	@echo ""
	@echo "  CFLAGS                  = $(CFLAGS)"
	@echo "  CPPFLAGS                = $(CPPFLAGS)"
	@echo "  LDFLAGS                 = $(LDFLAGS)"

.PHONY: clang show

# ldconfig fails when executed in pbuilder due to permissions
# just have to tell users to run it manually

# install-exec-local:
# 	@echo "(install-exec-local):"
# 	ldconfig

# uninstall-local:
# 	@echo "(uninstall-local):="
#	ldconfig


# Rename to "all-local" for development
all-local-disabled:
	@echo ""
	@echo "(Makefile:all-local) Variable values:"
	@echo "   CLEANFILES:       $(CLEANFILES)"
	@echo "   CFLAGS:           $(CFLAGS)"
	@echo "   AM_CFLAGS:        $(AM_CFLAGS)"
	@echo "   CPPFLAGS:         $(CPPFLAGS)"
	@echo "   AM_CPPFLAGS:      $(AM_CPPFLAGS)"
	@echo "   AUTOMAKE_OPTIONS: $(AUTOMAKE_OPTIONS)"
	@echo "   MAKELEVEL:        $(MAKELEVEL)"
	@echo "   MAKEFLAGS:        $(MAKEFLAGS)"
	@echo "   V:                $(V)"
	@echo "   AM_CFLAGS_STD:    $(AM_CFLAGS_STD)"
	@echo ""
