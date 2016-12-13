if HAVE_DOXYGEN
  SUBDIRS = doxygen
endif

if HAVE_DOXYGEN
if HAVE_DOCBASE

docbasedir = $(datadir)/doc-base

docbase_DATA = ddcutil-c-api

EXTRA_DIST = ddcutil-c-api

install-data-local:
	@echo "(docs/Makefile) Executing rule install-data-local"

install-data-hook:
	@echo "(docs/Makefile) Executing rule install-data-hook"
	@echo "docbasedir     $(docbasedir)"
	@echo "datadir        $(datadir)"
	install-docs --install-changed

# uninstall-data-hook doesn't exist, use uninstall-hook: 

uninstall-hook:
	@echo "(docs/Makefile) Executing rule uninstall-data-hook"
	install-docs --install-changed 

endif
endif
