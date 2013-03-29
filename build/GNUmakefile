#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

SCRIPTS = cleanconfigs cleantmpdirs
LSRCFILES = $(SCRIPTS)
LDIRT = $(wildcard *.src.tar.gz) pack_pcp.bin pcp.src \
	$(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION) \
	$(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-$(PACKAGE_REVISION) \
	pack_pro.bin pro.src deb

# for clean and clobber
SUBDIRS = tar rpm mac aix sun slack

# generate scripts/configs
MD5DIR = slack

default install default_pcp install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

pack :: pack_pcp

pack_pcp : pcp.src
	@DIST_MANIFEST=`pwd`/$@.bin; DIST_ROOT=/tmp/pcp-build-$$$$; \
	export DIST_MANIFEST DIST_ROOT; \
	rm -f $$DIST_MANIFEST; \
	echo === install === && $(MAKE) -j 1 -C $(TOPDIR) install_pcp || exit $$?; \
	if test ! -z "$(TAR)"; then \
	    echo === tar === && $(MAKEF) -j 1 -C tar $@ || exit $$?; \
	fi; \
	if test ! -z "$(RPM)"; then \
	    echo === rpm === && $(MAKEF) -j 1 -C rpm $@ || exit $$?; \
	fi; \
	if test ! -z "$(PACKAGE_MAKER)"; then \
	    echo === mac === && $(MAKEF) -j 1 -C mac $@ || exit $$?; \
	fi; \
	if test ! -z "$(MKINSTALLP)"; then \
	    echo === aix === && $(MAKEF) -j 1 -C aix $@ || exit $$?; \
	fi; \
	if test ! -z "$(PKGMK)" ; then \
	    echo === sun === && $(MAKEF) -j 1 -C sun $@ || exit $$?; \
	fi; \
	test -z "$$KEEP_DIST_ROOT" && rm -rf $$DIST_ROOT; echo Done

# Either a symlink in the TOPDIR is used to pack files relative to
# product-version directory, or hardlinks below build dir (MinGW).
# After sources have been packed, touch .census in the topdir to
# prevent second run on setup by RPM.
pcp.src : $(_FORCE)
	PKG_ROOT=$(PACKAGE_NAME)-$(PACKAGE_VERSION); \
	if [ "$(TARGET_OS)" = mingw ] ; then \
	    SRC_ROOT=. ; \
	else \
	    SRC_ROOT=$$PKG_ROOT; \
	    if [ ! -L $(TOPDIR)/$$SRC_ROOT ] ; then \
		$(LN_S) . $(TOPDIR)/$$SRC_ROOT || exit 1; \
	    fi; \
	fi; export PKG_ROOT SRC_ROOT; \
	CDIR=`pwd`; cd $(TOPDIR); \
	$(MAKE) -j 1 $(MAKEOPTS) $(@:%.src=src-%) >/tmp/$$$$.makesrc; \
	s=$$?; if [ $$s -ne 0 ] ; then exit $$s; \
	else \
	    sed -e "s;^\.;$$SRC_ROOT;" </tmp/$$$$.makesrc >$$CDIR/$@ ;\
	    rm -f /tmp/$$$$.makesrc; \
	    unset TAPE; MD5=$$CDIR/tar/pcp.md5sum.src; \
	    SRCTAR=$$CDIR/tar/$${PKG_ROOT}.src.tar.gz;\
	    $(TAR) -T $$CDIR/$@ -cf - | gzip > $$SRCTAR; \
	    $(MD5SUM) -b $$SRCTAR | sed -e 's/ .*//g' > $$MD5; \
	    $(MAKE) -j 1 -C $$CDIR/$(MD5DIR) md5sum_pcp; \
	    test -f .census || touch .census; \
	    if [ ! -L $$SRC_ROOT -a "$(TARGET_OS)" != mingw ] ; then \
		if [ -n "$$SRC_ROOT" ]; then \
		    rm -rf $$SRC_ROOT || exit 1; \
		    $(LN_S) . $$SRC_ROOT || exit 1; \
		fi; \
	    fi; \
	fi

include $(BUILDRULES)
