#
# Copyright (c) 2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

ifneq (,)
This makefile requires GNU Make.
endif

TOPDIR = .
-include $(TOPDIR)/src/include/builddefs

CONFFILES = configure pcp-gui.lsm
LSRCFILES = Makepkgs aclocal.m4 install-sh README VERSION \
	    configure.in pcp-gui.lsm.in

LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	Logs/* built .census install.* install-dev.* *.gz \
	install.manifest base_files.rpm docs_files.rpm conf_files
LDIRDIRT = pcp-gui-*

SUBDIRS = src qa m4 images doc man debian build

default :: default-pcp-gui

pcp-gui : default-pcp-gui

default-pcp-gui : configure-pcp-gui
	@for d in `echo $(SUBDIRS)`; do \
	    if test -d "$$d" ; then \
		echo === $$d ===; \
		$(MAKE) -C $$d default || exit $$?; \
	    fi; \
	done

install :: default-pcp-gui install-pcp-gui

pack-pcp-gui : default-pcp-gui
	$(MAKE) -C build $@

install-pcp-gui : default-pcp-gui
	@for d in `echo $(SUBDIRS)`; do \
	    if test -d "$$d" ; then \
		echo === $$d ===; \
		$(MAKE) -C $$d install || exit $$?; \
	    fi; \
	done
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 pcp-gui.lsm README $(PKG_DOC_DIR)
	$(INSTALL) -m 755 -d $(PKG_HTML_DIR)
	$(INSTALL) -m 755 -d $(PKG_ICON_DIR)
	$(INSTALL) -m 755 -d $(PKG_DESKTOP_DIR)

ifdef BUILDRULES
include $(BUILDRULES)
else
# if src/include/builddefs doesn't exist, we are pristine (hence also clean)
realclean distclean clean clobber:
	@true
endif

configure-pcp-gui: pcp-gui.lsm

pcp-gui.lsm: configure pcp-gui.lsm.in
	./configure

configure : configure.in
	rm -fr config.cache autom4te.cache
	autoconf

aclocal.m4:
	aclocal --acdir=`pwd`/m4 --output=$@
