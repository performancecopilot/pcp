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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs
SCRIPTS = $(TOPDIR)/build

LSRCFILES = remove README \
	preinstall.head.in preinstall.head preinstall.tail \
	postinstall.head.in postinstall.head postinstall.tail

LDIRT = $(wildcard pcp*.tar.gz pcp*.md5sum.src) \
	preinstall preinstall.head postinstall postinstall.head

default install install_pcp default_pcp : preinstall postinstall

preinstall: preinstall.head $(SCRIPTS)/cleanconfigs preinstall.tail
	rm -f $@
	cat preinstall.head $(SCRIPTS)/cleanconfigs preinstall.tail > $@
	chmod ugo+x $@

postinstall: postinstall.head postinstall.tail
	rm -f $@
	cat postinstall.head postinstall.tail > $@
	chmod ugo+x $@

pack_pcp : 
	@ BINTAR=$(PACKAGE_NAME)-$(PACKAGE_VERSION)-$(PACKAGE_BUILD).tar.gz \
	; export HERE=`pwd` \
	; ./pack-perl $${DIST_ROOT:-/} \
	; ./listfiles $${DIST_ROOT:-/} \
	| ( cd $${DIST_ROOT:-/} ; $(TAR) -T - -cf - ) \
	| gzip > $$HERE/$$BINTAR \
	; echo Wrote: $$HERE/$$BINTAR

include $(BUILDRULES)
