#
# Copyright (c) 2015 Red Hat.
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

CFILES	= autofsd-probe.c
CMDTARGET = autofsd-probe
LLDLIBS = $(PCPLIB)

MAN_SECTION = 1
MAN_PAGES = $(CMDTARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me

include $(TOPDIR)/src/include/buildrules

ifneq "$(findstring $(PCP_PLATFORM),darwin freebsd netbsd)" ""
build-me: $(CMDTARGET)

install: build-me
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install
