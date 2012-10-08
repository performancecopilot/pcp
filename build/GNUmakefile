#
# Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

LDIRT = $(wildcard *.src.tar.gz) pack-pcp-gui.bin pcp-gui.src \
	$(TOPDIR)/$(PKG_NAME)-$(PKG_VERSION) \
	$(TOPDIR)/$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE)
LDIRDIRT = deb

# for clean and clobber
SUBDIRS = tar rpm mac

# nothing to build here (it's all packaging)
default install install-dev install-lib:

pack :: pack-pcp-gui

pack-pcp-gui : pcp-gui.src
	@DIST_MANIFEST=`pwd`/$@.bin; DIST_ROOT=/tmp/pcp-gui-build-$$$$; \
	export DIST_MANIFEST DIST_ROOT; \
	rm -f $$DIST_MANIFEST; \
	echo === install === && $(MAKE) -j 1 -C $(TOPDIR) install || exit $$?; \
	if test ! -z "$(TAR)"; then \
	    ( echo "=== tar ===" && $(MAKEF) -j 1 -C tar $@ || exit $$? ); \
	fi; \
	if test ! -z "$(RPMBUILD)"; then \
	    ( echo "=== rpm ===" && $(MAKEF) -j 1 -C rpm $@ || exit $$? ); \
	fi; \
	if test ! -z "$(PACKAGE_MAKER)"; then \
	    ( echo "=== mac ===" && $(MAKEF) -j 1 -C mac $@ || exit $$? ); \
	fi; \
	test -z "$$KEEP_DIST_ROOT" || rm -rf $$DIST_ROOT; echo Done

# Either a symlink in the TOPDIR is used to pack files relative to
# product-version directory, or hardlinks below build dir (MinGW).
# After sources have been packed, touch .census in the topdir to
# prevent second run on setup by RPM.
pcp-gui.src : $(_FORCE)
	export PKG_ROOT=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE); \
	if [ "$(PCP_PLATFORM)" = mingw ] ; then \
	    export SRC_ROOT=. ; \
	else \
	    export SRC_ROOT=$$PKG_ROOT; \
	    if [ ! -L $(TOPDIR)/$$SRC_ROOT ] ; then \
		$(LN_S) . $(TOPDIR)/$$SRC_ROOT || exit 1; \
	    fi; \
	fi; \
	CDIR=`pwd`; cd $(TOPDIR); \
	$(MAKE) -j 1 $(MAKEOPTS) $(@:%.src=src-%) >/tmp/$$$$.makesrc; \
	s=$$?; if [ $$s -ne 0 ] ; then exit $$s; \
	else \
	    sed -e "s;^\.;$$SRC_ROOT;" </tmp/$$$$.makesrc >$$CDIR/$@ ;\
	    rm -f /tmp/$$$$.makesrc; \
	    unset TAPE; SRCTAR=$${PKG_ROOT}.src.tar.gz;\
	    $(TAR) -T $$CDIR/$@ -cf - | gzip > $$CDIR/tar/$$SRCTAR; \
	    test -f .census || touch .census; \
	    if [ ! -L $$SRC_ROOT -a "$(PCP_PLATFORM)" != mingw ] ; then \
		if [ -n "$$SRC_ROOT" ]; then \
		    rm -rf $$SRC_ROOT || exit 1; \
		    $(LN_S) . $$SRC_ROOT || exit 1; \
		fi; \
	    fi; \
	fi

include $(BUILDRULES)
