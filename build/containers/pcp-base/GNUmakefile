#
# Copyright (c) 2015 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

# Directory containing PCP RPMs to be installed in the base container.
# The default is the RPMs just build by the Makepkgs script. These packages
# will also be used to build all containers layered on pcp-base.
PACKAGES=../../rpm

CONTAINER = pcp-base
LDIRT = $(CONTAINER)-$(CONTAINER_VERSION).$(CONTAINER_ARCH).tgz RPMS

default default_pcp install install_pcp:
	@echo nothing to do

pack_pcp: RPMS $(CONTAINER)-$(CONTAINER_VERSION).$(CONTAINER_ARCH).tgz

RPMS: # Docker build runs createrepo RPMS
	@echo Using PCP RPMS from $(PACKAGES)
	cp -a $(PACKAGES) RPMS

# rule to build $(CONTAINER).tgz from Dockerfile
include ../GNUlocalrules

include $(BUILDRULES)
