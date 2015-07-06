#
# Copyright (c) 2012-2014 Red Hat.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= papi
DOMAIN	= PAPI
CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAINIT = $(IAM)_init

LLDLIBS = $(PAPI_LIBS) $(PCP_PMDALIB)
LCFLAGS = $(PAPI_CFLAGS) $(INVISIBILITY) -I.

CFILES	= papi.c
DFILES	= README help

LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET) $(LIBTARGET)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_PAPI)" "true"
build-me:	domain.h $(CMDTARGET) $(LIBTARGET)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(CMDTARGET) $(LIBTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root pmns domain.h $(DFILES) $(PMDADIR)
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

papi.o: domain.h
papi.o: $(VERSION_SCRIPT)
