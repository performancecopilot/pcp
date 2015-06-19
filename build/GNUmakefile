#
# Copyright (c) 2014 Red Hat.
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
LSRCFILES = $(SCRIPTS) GNUlocaldefs.in
LDIRT = pack_pcp.bin pack_pro.bin pro.src
LDIRDIRT = deb $(TOPDIR)/$(PACKAGE_NAME)-?.?.?

# for clean and clobber
SUBDIRS = tar rpm mac aix sun containers

default install default_pcp install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

pack :: pack_pcp

pack_pcp :
	@DIST_MANIFEST=`pwd`/$@.bin; DIST_ROOT=/tmp/pcp-build-$$$$; \
	export DIST_MANIFEST DIST_ROOT; \
	rm -f $$DIST_MANIFEST; \
	echo === install === ; \
	$(MAKE) -j 1 -C $(TOPDIR) install_pcp || exit $$?; \
	if test ! -z "$(TAR)"; then \
	    echo === tar === && $(MAKE) -j 1 -C tar $@ || exit $$?; \
	fi; \
	if test ! -z "$(PACKAGE_MAKER)"; then \
	    echo === mac === && $(MAKE) -j 1 -C mac $@ || exit $$?; \
	fi; \
	if test ! -z "$(MKINSTALLP)"; then \
	    echo === aix === && $(MAKE) -j 1 -C aix $@ || exit $$?; \
	fi; \
	if test ! -z "$(PKGMK)" ; then \
	    echo === sun === && $(MAKE) -j 1 -C sun $@ || exit $$?; \
	fi; \
	test -z "$$KEEP_DIST_ROOT" && rm -rf $$DIST_ROOT; echo Done

include $(BUILDRULES)
