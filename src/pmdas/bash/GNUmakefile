#
# Copyright (c) 2012 Nathan Scott.  All Rights Reversed.
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

IAM		= bash
DOMAIN		= BASH

CMDTARGET	= pmdabash$(EXECSUFFIX)
#LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
TARGETS		= $(CMDTARGET) $(LIBTARGET)
LLDLIBS		= -lpcp -lpcp_pmda
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
LDIRT		= domain.h *.log *.dir *.pag so_locations

CFILES		= event.c pmda.c util.c
HFILES		= event.h
SCRIPTS		= bashproc.sh pcp.sh
SAMPLES		= test-child.sh test-trace.sh
LSRCFILES	= Install Remove pmns help root $(SCRIPTS) $(SAMPLES)

default:	build-me

include $(BUILDRULES)

build-me: domain.h $(CMDTARGET)

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(TARGETS) Install Remove $(PMDADIR)
	$(INSTALL) -m 644 root help pmns $(PMDADIR)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	$(INSTALL) -m 644 bashproc.sh $(PCP_SHARE_DIR)/lib/bashproc.sh
	$(INSTALL) -m 644 pcp.sh $(PCP_ETC_DIR)/pcp.sh

pmda.o: domain.h
event.o pmda.o util.o: event.h

.NOTPARALLEL:
.ORDER:	domain.h $(OBJECTS)

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
