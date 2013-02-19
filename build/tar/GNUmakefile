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
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

LSRCFILES = preinstall.head.in preinstall.head preinstall.tail postinstall remove README

LDIRT = $(wildcard pcp*.tar.gz pcp*.md5sum.src) preinstall preinstall.head

default install install_pcp default_pcp : preinstall

preinstall: preinstall.head ../../debian/cleanconfigs preinstall.tail
	rm -f preinstall
	cat preinstall.head ../../debian/cleanconfigs preinstall.tail >preinstall

# Note on sed below
# for f and l lines of manifest, want the last word (target name),
# stripped of leading /
#
.PHONY: pack_pcp
pack_pcp : 
	@ BINTAR=$(PACKAGE_NAME)-$(PACKAGE_VERSION)-$(PACKAGE_BUILD).tar.gz; \
	HERE=`pwd`; cd $${DIST_ROOT:-/}; \
	sort -u $$DIST_MANIFEST | sed -n -e '/^[fl]/s/.* \/*//p' \
	| $(TAR) -T - -cf - \
	| gzip > $$HERE/$$BINTAR; echo Wrote: $$HERE/$$BINTAR

include $(BUILDRULES)
