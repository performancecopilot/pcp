#
# Copyright (c) 2014-2022 Red Hat.
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

SUBDIRS = \
	atop \
	buddyinfo \
	dmcache \
	dstat \
	free \
	geolocate \
	htop \
	iostat \
        ipcs \
	meminfo \
	mpstat \
	netstat \
	numastat \
	pidstat \
	ps \
	python \
	rocestat \
	shping \
	slabinfo \
	ss \
	summary \
	tapestat \
	uptime \
	verify \
	vmstat \
	xsos \
	zoneinfo

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: install_pcp

install_pcp : $(SUBDIRS)
	$(INSTALL) -m 755 pcp.sh $(PCP_BIN_DIR)/pcp$(SHELLSUFFIX)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
