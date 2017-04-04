#
# Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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

TOPDIR	= ../..
include $(TOPDIR)/src/include/builddefs

ifeq "$(HAVE_PERL)" "true"
SUBDIRS	= PMDA LogSummary MMV LogImport
endif

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
ifeq "$(shell [ '$(PACKAGE_DISTRIBUTION)' = cocoa \
                -o '$(PACKAGE_DISTRIBUTION)' = macosx \
                -o '$(PACKAGE_DISTRIBUTION)' = gentoo \
                -o '$(PACKAGE_DISTRIBUTION)' = solaris \
	      ] && echo 1)" "1"
	# Gather installed Perl files before packaging
	if [ -n "$(DIST_MANIFEST)" ]; then \
	    if [ "`echo $(TOPDIR)/perl-pcp-*.list`" != "$(TOPDIR)/perl-pcp-*.list" ]; then \
		cat $(TOPDIR)/perl-pcp-*.list | while read f; do \
		    bn=`basename $$f .gz`; \
		    dn=`dirname $$f`; \
		    $(INSTALL) -d $$dn || exit 1; \
		    src=`find */blib -name $$bn`; \
		    if [ -x $$src ] ; then mode=0755; else mode=0644; fi; \
		    $(INSTALL) -m $$mode $$src $$dn/$$bn || exit 1; \
		done; \
	    fi; \
	fi
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install
