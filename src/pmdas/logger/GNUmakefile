#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2011 Nathan Scott.  All Rights Reversed.
# Copyright (c) 2011-2012 Red Hat Inc.
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

CMDTARGET = pmdalogger$(EXECSUFFIX)
DFILES 	= README
CFILES	= event.c util.c logger.c
HFILES	= event.h util.h
LLDLIBS = $(PCP_PMDALIB)
LSRCFILES = Install Remove pmns help $(DFILES) root

IAM	= logger
DOMAIN	= LOGGER
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET)

default: domain.h $(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns $(PMDADIR)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)

logger.o: domain.h
event.o logger.o: event.h
util.o event.o logger.o: util.h

.NOTPARALLEL:
.ORDER:	domain.h $(OBJECTS)

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
