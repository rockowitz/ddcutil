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

SUBDIRS = src man data package

RPM = rpm

rpms srcrpm: dist
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

obsrpm:  
	echo "---> Makefile: obsrpm"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

dpkg:  
	echo "---> Makefile: dpkg"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

pdebuild:  
	echo "---> Makefile: pdebuild"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1


obsdeb:  
	echo "---> Makefile: obsdeb"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

ppa:  
	echo "---> Makefile: ppa"
	(cd package && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1



.PHONY: srcrpm rpms obsrpm dpkg obsdeb ppa



# rpmarch=`$(RPM) --showrc | grep "^build arc" | \
# 	sed 's/\(.*: \)\(.*\)/\2/'`; \
#	echo "$$rpmarch" ;\
#test -z "obs/$$rpmarch" || \
#	( mv obs/$$rpmarch/* . && rm -rf /obs/$$rpmarch )
#rm -rf obs/$(distdir)
