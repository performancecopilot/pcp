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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

LDIRT += $(wildcard *.src.tar.gz) pack_pcp.bin pcp.src \
	$(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION) \
	$(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-$(PACKAGE_REVISION) \
	pack_pro.bin pro.src

# for clean and clobber
SUBDIRS = tar rpm mac

# nothing to build here (it's all packaging)
default install default_pcp install_pcp :

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
	if test ! -z "$(MAKENSIS)"; then \
	    echo === win === && $(MAKEF) -j 1 -C win $@ || exit $$?; \
	fi; \
	test -z "$$KEEP_DIST_ROOT" && rm -rf $$DIST_ROOT; echo Done

# Symlink in the TOPDIR is used to pack files relative to
# product-version directory. It is replaced with a directory if we
# have to do any post-processing on sources. After sources has been
# packed, touch .census in the topdir to prevent second run on
# setup by RPM.
pcp.src : $(_FORCE)
	SRC_ROOT=$(PACKAGE_NAME)-$(PACKAGE_VERSION); \
	export SRC_ROOT; \
	if [ -x $(TOPDIR)/tools/mangle-src ] ; then \
	    if [ -L $(TOPDIR)/$$SRC_ROOT ] ; then \
	    	rm $(TOPDIR)/$$SRC_ROOT || exit 1; \
	    elif [ -d $(TOPDIR)/$$SRC_ROOT ] ; then \
	        rm -rf $(TOPDIR)/$$SRC_ROOT || exit 1; \
	    fi; \
	    mkdir $(TOPDIR)/$$SRC_ROOT || exit 1 ; \
	else \
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
	    unset TAPE; SRCTAR=$${SRC_ROOT}-$(PACKAGE_BUILD).src.tar.gz;\
	    $(TAR) -T $$CDIR/$@ -cf - | gzip > $$CDIR/tar/$$SRCTAR; \
	    test -f .census || touch .census; \
	    if [ ! -L $$SRC_ROOT ] ; then \
		if [ -n "$$SRC_ROOT" ]; then \
		    rm -rf $$SRC_ROOT || exit 1; \
		    $(LN_S) . $$SRC_ROOT || exit 1; \
		fi; \
	    fi; \
	fi


include $(BUILDRULES)
