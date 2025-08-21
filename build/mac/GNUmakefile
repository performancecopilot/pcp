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

LDIRDIRT = pcp-*
LDIRT = $(wildcard pcp*.pkg) $(wildcard pcp*.dmg) \
	idb installer-files info description uninstall-pcp
SUBDIRS = English.lproj installer-resources

default install install_pcp default_pcp :

.PHONY: pack_pcp
pack_pcp : extrabits
	@PACKAGE_VERSION=${PACKAGE_VERSION} \
	 PACKAGE_MAJOR=${PACKAGE_MAJOR} \
	 PACKAGE_MINOR=${PACKAGE_MINOR} \
	 PACKAGE_BUILD=${PACKAGE_BUILD} \
	./build-installer $(PKGBUILD) $(HDIUTIL) \
			  $$DIST_ROOT ${PACKAGE_NAME} idb 

idb: $(DIST_MANIFEST)
# We need to do a fair bit of munging here
# Firstly, add a version number
# then swap source and destination
# ensure there is a mode and user/group for EVERY entry
# Add a 'rule' (eg replace)
# make destination relative 
# make source relative to DIST_ROOT
# ensure that a symlink has a full path for the source (no change needed
#   if source is already a full path)
# Remove a linux-specific directory/file that isn't useful and isn't allowed in /usr/lib anyway (tmpfiles.d)
# Get rid of the QA & Testsuite stuff is as they're not useful resources for the Mac installation (it's not a source-bundle) and some files don't correctly come across anyway
# - this also speeds up the install and trims down the archive
# append the installer scripts
	@echo Making idb...
	@echo "# Version 1" > idb
	@echo "# IDB for PCP" >> idb
	@WA=`cd $(TOPDIR) ; pwd` ; \
	$(PCP_AWK_PROG) ' \
		$$1 == "d" { printf "%s %s %s %s %s src/ replace\n", $$1, $$2, $$3, $$4, $$5 ; next; } \
		$$1 == "l" { \
			str = ""; \
			if (substr($$2,1,1) != "/") { \
			    i = split($$3, a, "/"); \
				for( n = 1; n < i; n++ ) { \
					str = str a[n] "/"; \
				} \
			} \
			printf "s 0 $(PCP_USER_INSTALL) $(PCP_GROUP_INSTALL) %s %s%s replace\n", $$3, str, $$2 ; \
			next; \
		} \
		$$5 ~ "'$$DIST_ROOT'" { \
			sub(".*'$$DIST_ROOT'", "", $$5); \
		} \
		{ \
			printf "%s %s %s %s %s %s replace\n", $$1, $$2, $$3, $$4, $$6, $$5 ; \
		} \
		' $$DIST_MANIFEST \
	| sed -e 's# '$$WA'/# #g' -e 's# /# #g' | \
	grep -v "usr/lib/tmpfiles.d" | \
	$(PCP_AWK_PROG) ' \
		$$6 !~ /^qa/ && $$5 !~ /testsuite/ {print} \
	 ' >> idb
	@(bdir=`echo $(PCP_BINADM_DIR) | cut -c2-`; \
	  echo "f 644 root wheel $$bdir/idb idb replace"; \
	  echo "f 755 root wheel $$bdir/install-pcp install-pcp replace"; \
	  echo "f 755 root wheel $$bdir/uninstall-pcp uninstall-pcp replace") >> idb
	@echo "f 644 root wheel Library/LaunchDaemons/io.pcp.pmcd.plist io.pcp.pmcd.plist replace" >> idb
	@echo idb created

extrabits: idb
	DIST_MANIFEST= $(INSTALL) -m 755 idb install-pcp uninstall-pcp \
		$(PCP_BINADM_DIR)
	DIST_MANIFEST= $(INSTALL) -m 644 io.pcp.pmcd.plist \
		Library/LaunchDaemons/io.pcp.pmcd.plist


include $(BUILDRULES)
