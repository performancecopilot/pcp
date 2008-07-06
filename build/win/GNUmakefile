#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

LSRCFILES = pcp.nsi config.nsh.in

LDIRT = $(wildcard pcp-*-setup.exe) config.nsh nsi-files nsi-dirs \
	pcp-files pcp-dirs

default install :

.PHONY: pack_pcp
pack_pcp : config.nsh nsi-files nsi-dirs pcp-files pcp-dirs
	$(MAKENSIS) -V1 pcp.nsi

config.nsh: config.nsh.IN
	$(SED) -e 's/@VERSION@/$(PACKAGE_VERSION)-$(PACKAGE_BUILD)/g' < $< > $@

nsi-files:
	@echo Making nsi-files list.
	@WA=`cd $(TOPDIR) ; pwd` ; \
	 DIST_ROOT=`cd $(TOPDIR) ; pwd` ; \
	 DIST_MANIFEST=$(TOPDIR)/build/pack_pcp.bin ; \
	$(PCP_AWK_PROG) ' \
		$$1 == "d" { next; } \
		$$1 == "l" { next; } \
	{ printf "File \"oname=$$INSTDIR\%s\" %s\n", $$6, $$5 ; } \
		' $$DIST_MANIFEST | sed -e 's#/#\\#g' > nsi-files

nsi-dirs:
	@echo Making nsi-dirs list.
	@WA=`cd $(TOPDIR) ; pwd` ; \
	 DIST_ROOT=`cd $(TOPDIR) ; pwd` ; \
	 DIST_MANIFEST=$(TOPDIR)/build/pack_pcp.bin ; \
	$(PCP_AWK_PROG) ' \
		$$1 == "d" \
	{ printf "CreateDirectory \"$$INSTDIR\%s\"\n", $$5; } \
		' $$DIST_MANIFEST | sed -e 's#/#\\#g' > nsi-dirs

pcp-files:
	@echo Making pcp-files list.
	@WA=`cd $(TOPDIR) ; pwd` ; \
	 DIST_ROOT=`cd $(TOPDIR) ; pwd` ; \
	 DIST_MANIFEST=$(TOPDIR)/build/pack_pcp.bin ; \
	$(PCP_AWK_PROG) ' \
		$$1 == "d" { next; } \
		$$1 == "l" { next; } \
		$$5 ~ "'$$DIST_ROOT'" { sub(".*'$$DIST_ROOT'", "", $$5); } \
	{ printf "%s\n", $$5 ; } \
		' $$DIST_MANIFEST \
	| sed -e 's#^/##g' -e 's#/#\\#g' > pcp-files

pcp-dirs:
	@echo Making pcp-dirs list.
	@WA=`cd $(TOPDIR) ; pwd` ; \
	 DIST_MANIFEST=$(TOPDIR)/build/pack_pcp.bin ; \
	$(PCP_AWK_PROG) ' \
		$$1 == "d" { printf "%s\n", $$5; } \
		' $$DIST_MANIFEST \
	| sed -e 's#^/##g' -e 's#/#\\#g' > pcp-dirs

include $(BUILDRULES)
