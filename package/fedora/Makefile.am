
rpmspec = $(PACKAGE_TARNAME).spec

rpmmacros = \
 --define="-rpmdir $${PWD}/built_rpms"\
 --define="-srcrpmdir $${PWD}/built_rpms"\
 --define="-sourcedir $${PWD}/../.."\
 --define="-specdir $${PWD}"\
 --define="-builddir $${PWD}/rpmwork"

# RPMENV is hack for Fedora rpm build error from rpm/check-paths error into warning:
#  file '/usr/bin/laclient' contains a standard rpath '/usr/lib64' in [/usr/lib64]
RPMENV = QA_RPATHS=$$(( 0x0001 ))
RPMBUILD = rpmbuild
# --showrc   - displays macros 
RPMFLAGS = --nodeps --buildroot="$${PWD}/_rpm"  --verbose


rpmcheck:
	if [ which $(RPMBUILD) &> /dev/null ]; then \
		echo "*** This target requires an rpm build environmnent"; \
		(exit 1); exit 1; \
	fi

fsrcrpm: rpmcheck $(rpmspec)
	echo "--------------------------> package/fedora/Makefile: target fsrcrpm"
	$(RPMBUILD) $(RPMFLAGS) -bs $(rpmmacros) ddcutil.spec 

frpms: rpmcheck $(rpmspec)
	echo "--------------------------> package/fedora/Makefile: target frpms"
	$(RPMENV) $(RPMBUILD) $(RPMFLAGS) -ba $(rpmmacros) ddcutil.spec

.PHONY:  rpmcheck

dist-hook:
	@echo "(package/fedora/Makefile) Executing dist-hook"
