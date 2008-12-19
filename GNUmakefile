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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#

ifneq (,)
This makefile requires GNU Make.
endif

TOPDIR = .
-include $(TOPDIR)/src/include/builddefs

CONFFILES = configure kmchart.lsm
LSRCFILES = Makepkgs aclocal.m4 install-sh README VERSION \
	    configure.in kmchart.lsm.in

LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	Logs/* built .census install.* install-dev.* *.gz

SUBDIRS = src m4 images doc man debian build

default :: default_kmchart

kmchart : default_kmchart

default_kmchart : configure_kmchart
	@for d in `echo $(SUBDIRS)`; do \
	    if test -d "$$d" ; then \
		echo === $$d ===; \
		$(MAKE) -C $$d default || exit $$?; \
	    fi; \
	done

install :: default_kmchart install_kmchart

pack_kmchart : default_kmchart
	$(MAKE) -C build $@

install_kmchart : default_kmchart
	@for d in `echo $(SUBDIRS)`; do \
	    if test -d "$$d" ; then \
		echo === $$d ===; \
		$(MAKE) -C $$d install || exit $$?; \
	    fi; \
	done
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 kmchart.lsm README $(PKG_DOC_DIR)

ifdef BUILDRULES
include $(BUILDRULES)
else
# if src/include/builddefs doesn't exist, we are pristine (hence also clean)
realclean distclean clean clobber:
	@true
endif

configure_kmchart: kmchart.lsm

kmchart.lsm: configure kmchart.lsm.in
	./configure

configure : configure.in
	rm -fr config.cache autom4te.cache
	autoconf

aclocal.m4:
	aclocal --acdir=`pwd`/m4 --output=$@
