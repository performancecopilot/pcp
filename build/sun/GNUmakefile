#
# Copyright (c) 2013 Red Hat.
# Copyright (c) 2009 Max Matveev.  All Rights Reserved.
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

LSRCFILES = pcp.xml preinstall postinstall preremove
LDIRT += pkginfo proto pcp pcp-$(PACKAGE_VERSION)
PACKAGE_ARCH = $(shell uname -p)

default install default_pcp :

install_pcp :
ifeq "$(TARGET_OS)" "solaris"
	$(INSTALL) -m644 pcp.xml /var/svc/manifest/application/pcp.xml
endif

pack_pcp : proto pkginfo
	@rm -rf pcp
	$(PKGMK) -b $${DIST_ROOT} -d . -f proto
	pkgtrans . $(PACKAGE_NAME)-$(PACKAGE_VERSION) $(PACKAGE_NAME)

proto : $(_FORCE)
	@rm -f $@
	@sort -u ../pack_pcp.bin | \
	    awk 'BEGIN { print "i pkginfo=pkginfo"; \
			 print "i preremove=preremove"; \
			 print "i preinstall=preinstall"; \
			 print "i postinstall=postinstall"; \
			} \
	     $$1 == "l" {printf "s none %s=%s\n", substr($$3, 2), $$2; next; } \
	     {printf "%s none %s 0%d %s %s\n", \
		 $$1, substr($$NF, 2) , $$2, $$3, $$4}' > $@

pkginfo : $(_FORCE)
	@rm -f $@
	@echo "PKG=$(PACKAGE_NAME)" > $@
	@echo "NAME=Performance Co-Pilot" >> $@
	@echo "ARCH=$(PACKAGE_ARCH)" >> $@
	@echo "VERSION=$(PACKAGE_VERSION)" >> $@
	@echo "EMAIL=pcp@oss.sgi.com" >> $@
	@echo "CATEGORY=application" >> $@
	@echo "BASEDIR=/" >> $@
	@echo "CLASSES=application none" >> $@
ifneq "$(GIT)" ""
	@echo "PSTAMP="`$(GIT) log -n1 --pretty=format:%H` >> $@
endif

include $(BUILDRULES)

