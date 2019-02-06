#
# Copyright (c) 2013-2015,2018-2019 Red Hat.
# Copyright (c) 2009-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2000-2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM		= mmv
DOMAIN		= MMV
CMDTARGET	= pmda$(IAM)$(EXECSUFFIX)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
TARGETS		= $(CMDTARGET) $(LIBTARGET)

CFILES		= mmv.c
VERSION_SCRIPT	= exports
LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)
LDIRT		= domain.h *.log pmns

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)

default_pcp default: $(TARGETS) pmns

include $(BUILDRULES)

install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -o $(PCP_USER) -g $(PCP_GROUP) -m 775 -d $(PCP_TMP_DIR)/$(IAM)
	$(INSTALL) -o $(PCP_USER) -g $(PCP_GROUP) -m 775 -d $(PCP_TMP_DIR)/pmproxy
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 755 $(TARGETS) Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmns $(PMDADIR)/root_mmv
	$(INSTALL) -m 644 root_mmv root_pmproxy $(PCP_VAR_DIR)/pmns

$(CMDTARGET):	$(OBJECTS)

$(LIBTARGET) : $(VERSION_SCRIPT)

$(OBJECTS): domain.h

domain.h: ../../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
	@echo '#define PMPROXY 4' >> $@

pmns:
	$(LN_S) -f root_mmv pmns

mmv.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check::	$(CFILES)
	$(CLINT) $^
