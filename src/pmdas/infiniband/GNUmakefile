#
# Copyright (c) 2013 Red Hat.
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
CFILES	= ib.c pmda.c daemon.c
HFILES  = ib.h ibpmda.h

LSRCFILES = help root pmns Install Remove 

IB_LIBS = -libumad -libmad
LLDLIBS	= $(IB_LIBS) $(PCP_LIBS) -lpcp_pmda -lpcp

IAM	= ib
DOMAIN  = IB
PMDADIR = $(PCP_PMDAS_DIR)/infiniband
LDIRT	= domain.h *.o $(IAM).log $(CMDTARGET)

default: $(CMDTARGET)

include $(BUILDRULES)

install: default
	install -m 755 -d $(PMDADIR)
	install -m 755 Install Remove $(CMDTARGET) $(PMDADIR)
	install -m 644 pmns root help domain.h $(PMDADIR)

ib.o:	domain.h

.NOTPARALLEL:
.ORDER: domain.h $(OBJECTS)

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
