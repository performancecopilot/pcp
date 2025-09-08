#
# Copyright (c) 2013-2015,2020 Red Hat.
# Copyright (c) 2007-2009 Silicon Graphics, Inc.  All Rights Reserved.
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

CMDTARGET = pmdaib$(EXECSUFFIX)
CFILES	= ib.c pmda.c
HFILES  = ibpmda.h
LLDLIBS	= $(IB_LIBS) $(PCP_LIBS) -lpcp_pmda -lpcp -libverbs

IAM	= ib
DOMAIN  = IB
LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/infiniband
PMDATMPDIR = $(PCP_PMDAS_DIR)/infiniband

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_INFINIBAND)" "true"
build-me: $(CMDTARGET)

install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Upgrade Remove $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) pmns root help domain.h $(PMDAADMDIR)
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
