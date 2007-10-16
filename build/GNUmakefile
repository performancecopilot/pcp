#
# Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

MANIFEST=src-manifest
SRCTAR=$(PKG_NAME)-$(PKG_VERSION).src.tar.gz

LDIRT = *-manifest *.gz $(TOPDIR)/$(PKG_NAME)-*

# for clean and clobber
SUBDIRS = tar rpm

LSRCFILES = mac

# nothing to build here (it's all packaging)
default install install-dev install-lib:

include $(BUILDRULES)

# Symlink in the TOPDIR is used to pack files relative to
# product-version directory.
$(MANIFEST) : $(_FORCE)
	@if [ ! -L $(TOPDIR)/$(PKG_NAME)-$(PKG_VERSION) ] ; then \
	    $(LN_S) . $(TOPDIR)/$(PKG_NAME)-$(PKG_VERSION) ; \
	fi
	@CDIR=`pwd`; cd $(TOPDIR); \
	$(MAKE) --no-print-directory source | \
	    sed -e 's/^\./$(PKG_NAME)-$(PKG_VERSION)/' > $$CDIR/$@ ;\
	if [ $$? -ne 0 ] ; then  \
	    exit 1; \
	else \
	    unset TAPE; \
	    $(TAR) -T $$CDIR/$@ -cf - | $(ZIP) --best > $$CDIR/$(SRCTAR); \
	    echo Wrote: $$CDIR/$(SRCTAR); \
	fi

dist : default $(MANIFEST)
	@DIST_MANIFEST=`pwd`/bin-manifest; DIST_ROOT=/tmp/kmchart-build-$$$$; \
	export DIST_MANIFEST DIST_ROOT; \
	rm -f $$DIST_MANIFEST; \
	echo === install === && $(MAKE) -C $(TOPDIR) install || exit $$?; \
	if test ! -z "$(TAR)"; then \
	    ( echo "=== tar ===" && $(MAKEF) -C tar $@ || exit $$? ); \
	fi; \
	if test ! -z "$(RPMBUILD)"; then \
	    ( echo "=== rpm ===" && $(MAKEF) -C rpm $@ || exit $$? ); \
	fi; \
	if test ! -z "$(PACKAGE_MAKER)"; then \
	    echo === mac === && $(MAKEF) -j 1 -C mac $@ || exit $$?; \
	fi; \
	test -z "$$KEEP_DIST_ROOT" || rm -rf $$DIST_ROOT; echo Done
