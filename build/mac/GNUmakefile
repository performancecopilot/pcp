#! gmake
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
include /etc/pcp.conf

LDIRT = $(wildcard kmchart*.pkg) $(wildcard kmchart*.dmg) \
	idb-kmchart installer-files

INSTFILES = idb-kmchart install-kmchart uninstall-kmchart \
	    English.lproj/Localizable.strings

LSRCFILES = English.lproj/Localizable.strings build-installer \
	    install-kmchart installer-description installer-info \
	    installer-resources/InstallationCheck \
	    installer-resources/License.html \
	    installer-resources/ReadMe.html \
	    installer-resources/Welcome.txt \
	    installer-resources/background.jpg \
	    installer-resources/postflight \
	    installer-resources/postinstall \
	    installer-resources/postupgrade \
	    installer-resources/preflight \
	    installer-resources/preinstall \
	    installer-resources/preupgrade \
	    uninstall-kmchart

default install :

.PHONY: pack_kmchart
pack_kmchart : extrabits
	@PACKAGE_VERSION=${PACKAGE_VERSION} \
	 PACKAGE_MAJOR=${PACKAGE_MAJOR} \
	 PACKAGE_MINOR=${PACKAGE_MINOR} \
	 PACKAGE_BUILD=${PACKAGE_BUILD} \
	./build-installer $(PACKAGE_MAKER) $(HDIUTIL) \
			  $$DIST_ROOT ${PACKAGE_NAME} idb-kmchart

idb-kmchart: $(DIST_MANIFEST)
# We need to do a fair bit of munging here
# Firstly, add a version number
# then swap source and destination
# ensure there is a mode and user/group for EVERY entry
# Add a 'rule' (eg replace)
# make destination relative 
# make source relative to DIST_ROOT
# ensure that a symlink has a full path for the source
# append the installer scripts
	@echo Making idb-kmchart...
	@echo "# Version 1" > idb-kmchart
	@echo "# IDB for kmchart" >> idb-kmchart
	@WA=`cd $(TOPDIR) ; pwd` ; \
	$(AWK) ' \
		$$1 == "d" { printf "%s %s %s %s %s src/ replace\n", $$1, $$2, $$3, $$4, $$5 ; next; } \
		$$1 == "l" { \
			i = split($$3, a, "/"); \
			str = ""; \
			    for( n = 1; n < i; n++ ) { \
				    str = str a[n] "/"; \
			    } \
			printf "s 0 $(INSTALL_USER) $(INSTALL_GROUP) %s %s%s replace\n", $$3, str, $$2 ; \
			next; \
		} \
		$$5 ~ "'$$DIST_ROOT'" { \
			sub(".*'$$DIST_ROOT'", "", $$5); \
		} \
		{ \
			printf "%s %s %s %s %s %s replace\n", $$1, $$2, $$3, $$4, $$6, $$5 ; \
		} \
		' $$DIST_MANIFEST \
	| sed -e 's# '$$WA'/# #g' -e 's# /# #g' >> idb-kmchart
	@(bdir=`echo $$PCP_BINADM_DIR | cut -c2-`; \
	  echo "f 644 root wheel $$bdir/idb-kmchart idb-kmchart replace"; \
	  echo "f 755 root wheel $$bdir/install-kmchart install-kmchart replace"; \
	  echo "f 755 root wheel $$bdir/uninstall-kmchart uninstall-kmchart replace") >> idb-kmchart
	@echo idb-kmchart created

extrabits: idb-kmchart
	DIST_MANIFEST= $(INSTALL) -m 755 idb-kmchart install-kmchart uninstall-kmchart \
		$$PCP_BINADM_DIR

include $(BUILDRULES)
