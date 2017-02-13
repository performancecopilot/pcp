#
# Copyright (c) 2015,2017 Red Hat.
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

# The PCP RPMs to be installed in the base container is defined by the yum
# repo that is enabled in $(TOPDIR)/build/containers/pcp-base/pcp.repo.
# This can be easily modified, e.g. to use the packages just built by the
# Makepkgs script (as defined by the PACKAGES macro below), if desired.
# The repo enabled in the pcp.repo file supplies the packages that will be
# used to build all containers layered on pcp-base. Also see the comments in
# pcp-base/Dockerfile.
#
PACKAGES=$(shell echo ../../rpm/*.rpm)

CONTAINER = pcp-base
LDIRT = $(CONTAINER)-$(CONTAINER_VERSION).$(CONTAINER_ARCH).tgz RPMS

default default_pcp install install_pcp:
	@echo nothing to do

pack_pcp: RPMS $(CONTAINER)-$(CONTAINER_VERSION).$(CONTAINER_ARCH).tgz

RPMS: # Docker build runs createrepo RPMS
	@echo Adding PCP RPMS from $(PACKAGES)
	test ! -d RPMS && mkdir RPMS; cp -a $(PACKAGES) RPMS

# rule to build $(CONTAINER).tgz from Dockerfile
include ../GNUlocalrules

include $(BUILDRULES)
