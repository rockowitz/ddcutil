SUBDIRS = fedora

rpmspec = $(PACKAGE_TARNAME).spec

rpmmacros = \
 --define="-rpmdir $${PWD}/built_rpms"\
 --define="-srcrpmdir $${PWD}/built_rpms"\
 --define="-sourcedir $${PWD}/.."\
 --define="-specdir $${PWD}"\
 --define="-builddir $${PWD}/rpmwork"

# RPMENV is hack for Fedora rpm build error from rpm/check-paths error into warning:
#  file '/usr/bin/laclient' contains a standard rpath '/usr/lib64' in [/usr/lib64]
RPMENV = QA_RPATHS=$$(( 0x0001 ))
RPMBUILD = rpmbuild
# --showrc   - displays macros 
RPMFLAGS = --nodeps --buildroot="$${PWD}/_rpm"  --verbose

# n. ver must be set here, not within a target, where it's treated as an unrecognized command
ver = @PACKAGE_VERSION@
rlse = @DEBIAN_RELEASE@

rpmcheck:
	if [ which $(RPMBUILD) &> /dev/null ]; then \
		echo "*** This target requires an rpm build environmnent"; \
		(exit 1); exit 1; \
	fi

fsrcrpm frpms: rpmcheck $(rpmspec)
	echo "--------------------------> package/Makefile: target fsrcrpm"
	(cd fedora && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

#	rpmspec=ddcutil_fedora.spec
#	$(RPMBUILD) $(RPMFLAGS) -bs $(rpmmacros) ddcutil_fedora.spec 

# frpms: rpmcheck $(rpmspec)
# 	echo "--------------------------> package/Makefile: target frpms"
# 	rpmspec=ddcutil_fedora.spec
# 	$(RPMENV) $(RPMBUILD) $(RPMFLAGS) -ba $(rpmmacros) ddcutil_fedora.spec


srcrpm: rpmcheck $(rpmspec)
	echo "--------------------------> package/Makefile: target srcrpm"
	$(RPMBUILD) $(RPMFLAGS) -bs $(rpmmacros) $(rpmspec)

rpms: rpmcheck $(rpmspec)
	echo "--------------------------> package/Makefile: target rpms"
	$(RPMENV) $(RPMBUILD) $(RPMFLAGS) -ba $(rpmmacros) $(rpmspec)

obsrpm:
	echo "--------------------------> package/Makefile: target obsrpm"
	echo "ver --> ${ver}"
	./upload_obsrpm ${ver} ddcutil 

obsrpmtest:
	echo "--------------------------> package/Makefile: target obsrpmtest"
	echo "ver --> ${ver}"
	./upload_obsrpm ${ver} ddcutil-test 

dpkg:
	echo "--------------------------> obs/Makefile: target dpkg"
	./build_dpkg debuild

pdebuild:
	echo "--------------------------> obs/Makefile: target pdebuild"
	./build_dpkg pdebuild


obsdeb:
	echo "--------------------------> package/Makefile: target obsdeb"
	./build_dpkg obsdeb 
	./upload_obsdeb ${ver} ${rlse}


obsdebtest:
	echo "--------------------------> package/Makefile: target obsdeb"
	./build_dpkg obsdeb 
	./upload_obsdeb ${ver} ${rlse} ddcutil-test


ppa:
	echo "--------------------------> package/Makefile: target ppa"
	chmod +x build_dpkg upload_ppa
	./build_dpkg ppa
	debsign debwork/ddcutil_${ver}-${rlse}_source.changes   
	./upload_ppa ${ver} ${rlse}


mentors:
	echo "--------------------------> package/Makefile: target mentors"
	chmod +x build_dpkg upload_ppa upload_mentors
	./build_dpkg mentors 
	debsign debwork/ddcutil_${ver}-${rlse}_source.changes   
	./upload_mentors ${ver} ${rlse}

.PHONY:  rpmcheck srcrpm rpms obsrpm dpkg obsdeb ppa

dist-hook:
	@echo "(package/Makefile) Executing dist-hook"
