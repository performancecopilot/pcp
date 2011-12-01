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
# 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LDIRT = $(wildcard pcp-gui*.pkg) $(wildcard pcp-gui*.dmg) \
	idb-pcp-gui description info
LDIRDIRT = installer-files $(PKG_NAME)-*.*.*

SUBDIRS = English.lproj installer-resources
INSTFILES = idb-pcp-gui install-pcp-gui uninstall-pcp-gui

LSRCFILES = build-installer installer-description installer-info \
	    install-pcp-gui uninstall-pcp-gui

default install :

.PHONY: pack-pcp-gui
pack-pcp-gui : extrabits
	@PKG_VERSION=${PKG_VERSION} \
	 PKG_MAJOR=${PKG_MAJOR} \
	 PKG_MINOR=${PKG_MINOR} \
	 PKG_RELEASE=${PKG_RELEASE} \
	./build-installer $(PACKAGE_MAKER) $(HDIUTIL) \
			  $$DIST_ROOT ${PKG_NAME} idb-pcp-gui

# We need to do a fair bit of munging here.  Firstly, add a version number
# then swap source and destination.  Ensure there is a mode and user/group
# for EVERY entry.  Add a 'rule' (eg replace).  Make destination relative.
# Make source relative to DIST_ROOT.  Ensure that a symlink has a full path
# for the source append the installer scripts.
idb-pcp-gui: $(DIST_MANIFEST)
	@echo Making idb-pcp-gui...
	@echo "# Version 1" > idb-pcp-gui
	@echo "# IDB for pcp-gui" >> idb-pcp-gui
	@WA=`cd $(TOPDIR) ; pwd` ; \
	sort -u $(DIST_MANIFEST) | $(AWK) ' \
		$$1 == "d" { printf "%s %s %s %s %s src/ replace\n", $$1, $$2, $$3, $$4, $$5 ; next; } \
		$$1 == "l" { \
			i = split($$3, a, "/"); \
			str = ""; \
			    for ( n = 1; n < i; n++ ) { \
				    str = str a[n] "/"; \
			    } \
			printf "s 0 $(PKG_USER) $(PKG_GROUP) %s %s%s replace\n", $$3, str, $$2 ; \
			next; \
		} \
		$$5 ~ "'$$DIST_ROOT'" { \
			sub(".*'$$DIST_ROOT'", "", $$5); \
		} \
		{ \
			printf "%s %s %s %s %s %s replace\n", $$1, $$2, $$3, $$4, $$6, $$5 ; \
		} \
		' | sed -e 's# '$$WA'/# #g' -e 's# /# #g' >> idb-pcp-gui
	@(bdir=`echo $(PCP_BINADM_DIR) | cut -c2-`; \
	  echo "f 644 root admin $$bdir/idb-pcp-gui idb-pcp-gui replace"; \
	  echo "f 755 root admin $$bdir/install-pcp-gui install-pcp-gui replace"; \
	  echo "f 755 root admin $$bdir/uninstall-pcp-gui uninstall-pcp-gui replace") >> idb-pcp-gui
	@echo idb-pcp-gui created

extrabits: idb-pcp-gui
	DIST_MANIFEST= $(INSTALL) -m 755 -d $(PCP_BINADM_DIR)
	DIST_MANIFEST= $(INSTALL) -m 755 $(INSTFILES) $(PCP_BINADM_DIR)

include $(BUILDRULES)
