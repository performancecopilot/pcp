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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

LSRCFILES = preinstall postinstall remove README

LDIRT = $(wildcard pcp*.tar.gz pcp*.md5sum.src)

default install install_pcp default_pcp :

.PHONY: pack_pcp
pack_pcp : 
	@ BINTAR=$(PACKAGE_NAME)-$(PACKAGE_VERSION)-$(PACKAGE_BUILD).tar.gz; \
	HERE=`pwd`; cd $${DIST_ROOT:-/}; \
	sort -u $$DIST_MANIFEST | $(PCP_AWK_PROG) ' \
	$$1 == "f" { printf (".%s\n", $$6); } \
	$$1 == "d" { next; } \
	$$1 == "l" { printf (".%s\n", $$3); }' | $(TAR) -T - -cf - | \
		gzip > $$HERE/$$BINTAR; echo Wrote: $$HERE/$$BINTAR

include $(BUILDRULES)
